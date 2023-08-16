#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <iostream>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>
#include <time.h>
#include <vector>

#include "EkaFhCmeGr.h"
#include "EkaFhCmeParser.h"
#include "EkaFhThreadAttr.h"

using namespace Cme;

void *getCmeSnapshot(void *attr);
int ekaUdpMcConnect(EkaDev *dev, uint32_t ip, uint16_t port,
                    uint32_t srcIp = 0);

/* ################################################## */
int EkaFhCmeGr::createPktQ() {
  EKA_LOG("%s:%u: Creating PktQ", EKA_EXCH_DECODE(exch),
          id);
  pktQ = new PktQ(dev, exch, this, id);
  if (pktQ == NULL)
    on_error("pktQ == NULL");
  return 0;
}

/* ################################################## */

int EkaFhCmeGr::bookInit() {
  book = new FhBook(dev, id, exch);
  if (!book)
    on_error("!book");

  book->init();

  return 0;
}
/* ################################################## */
int EkaFhCmeGr::printConfig() {
  char productNamesBuf[MaxProductMaskNameBufSize];
  EKA_LOG(
      "%s:%u : "
      "productMask: \'%s\' (0x%x) "
      "MCAST: %s:%u, "
      "SNAPSHOT MC: %s:%u, "
      "RECOVERY MC: %s:%u, "
      "RECOVERY SrcIp: %s %s, "
      "connectRetryDelayTime=%d, "
      "staleDataNsThreshold=%ju",
      EKA_EXCH_DECODE(exch), id,
      lookupProductMaskNames(productMask, productNamesBuf),
      productMask, EKA_IP2STR(mcast_ip), mcast_port,
      EKA_IP2STR(snapshot_ip), be16toh(snapshot_port),
      EKA_IP2STR(recovery_ip), be16toh(recovery_port),

      recoverySrcIp ? "Manually Set" : dev->genIfName,
      recoverySrcIp ? EKA_IP2STR(recoverySrcIp)
                    : EKA_IP2STR(dev->genIfIp),

      connectRetryDelayTime, staleDataNsThreshold);
  return 0;
}
/* ################################################## */
void EkaFhCmeGr::pushPkt2Q(const uint8_t *pkt,
                           uint16_t pktSize,
                           uint64_t sequence) {

  if ((int)pktSize > PktElem::PKT_SIZE)
    on_error("pktSize %u > EkaFhPktQElem::PKT_SIZE %d",
             pktSize, PktElem::PKT_SIZE);

  PktElem *p = pktQ->push();
  memcpy(p->data, pkt, pktSize);
  p->sequence = sequence;
  p->pktSize = pktSize;

  return;
}
/* ################################################## */

int EkaFhCmeGr::processFromQ(const EfhRunCtx *pEfhRunCtx) {
  int qPktCnt = 0;
  while (!pktQ->is_empty()) {
    PktElem *buf = pktQ->pop();
    if (qPktCnt == 0) {
      /*   EKA_LOG("%s:%u: 1st Q pkt sequence = %ju, " */
      /*           "seq_after_snapshot = %ju", */
      /*           EKA_EXCH_DECODE(exch),id,buf->sequence,
       */
      /*           seq_after_snapshot); */
      EKA_LOG("%s:%u: 1st Q pkt sequence = %ju",
              EKA_EXCH_DECODE(exch), id, buf->sequence);
    }
    qPktCnt++;

    /*     if (buf->sequence < seq_after_snapshot) continue;
     */
    /* #ifdef _EKA_CHECK_BOOK_INTEGRITY */
    /*     EKA_LOG("%s:%u: processing pkt %d, sequence = %ju
     * from Q", */
    /* 	    EKA_EXCH_DECODE(exch),id, */
    /* 	    qPktCnt,buf->sequence); */
    /*     printPkt(buf->data,buf->pktSize, qPktCnt); */
    /* #endif   */
    processPkt(pEfhRunCtx, buf->data, buf->pktSize,
               EkaFhMode::MCAST);
    expected_sequence = buf->sequence + 1;
  }
  EKA_LOG("%s:%u: After Q draining %d packets "
          "expected_sequence = %ju",
          EKA_EXCH_DECODE(exch), id, qPktCnt,
          expected_sequence);
  return 0;
}

/* ################################################## */
int EkaFhCmeGr::closeSnapshotGap(
    EfhCtx *pEfhCtx, const EfhRunCtx *pEfhRunCtx) {
  std::string threadName =
      std::string("ST_") +
      std::string(EKA_EXCH_SOURCE_DECODE(exch)) + '_' +
      std::to_string(id);
  auto attr = new EkaFhThreadAttr(
      pEfhCtx, pEfhRunCtx, this, 0, 0, EkaFhMode::SNAPSHOT);
  if (!attr)
    on_error("!attr");

  dev->createThread(
      threadName.c_str(), EkaServiceType::kFeedSnapshot,
      getCmeSnapshot, attr, dev->createThreadContext,
      (uintptr_t *)&snapshot_thread);

  return 0;
}
/* ################################################## */

void *getCmeSnapshot(void *attr) {
  auto params{
      reinterpret_cast<const EkaFhThreadAttr *>(attr)};
  if (!params)
    on_error("!params");

  auto pEfhRunCtx{params->pEfhRunCtx};
  auto gr{dynamic_cast<EkaFhCmeGr *>(params->gr)};
  if (!gr)
    on_error("!gr");

  delete params;

#ifdef FH_LAB
  gr->snapshotClosed = true;
  gr->inGap = false;
  return NULL;
#endif

  pthread_detach(pthread_self());

  EkaDev *dev = gr->dev;
  if (!dev)
    on_error("!dev");

  gr->recoveryLoop(pEfhRunCtx, EkaFhMode::SNAPSHOT);

  return NULL;
}

EkaOpResult
EkaFhCmeGr::recoveryLoop(const EfhRunCtx *pEfhRunCtx,
                         EkaFhMode op) {
#ifdef FH_LAB
  return EKA_OPRESULT__OK;
#endif
  auto ip = op == EkaFhMode::DEFINITIONS ? snapshot_ip
                                         : recovery_ip;
  auto port = op == EkaFhMode::DEFINITIONS ? snapshot_port
                                           : recovery_port;

  struct RecoveryPkt {
    size_t size;
    uint8_t data[1536];
  };

  std::vector<RecoveryPkt> recoveryPkt;

  int sock = ekaUdpMcConnect(dev, ip, port, recoverySrcIp);
  if (sock < 0)
    on_error("sock = %d", sock);

  snapshot_active = true;
  snapshotClosed = false;

  bool recovered = false;
  book->invalidate(NULL, 0, 0, 0, false);

  while (!recovered && snapshot_active) {
    recoveryPkt.clear();
    iterationsCnt = 0;
    uint32_t expectedPktSeq = 1;
    while (snapshot_active) {
      uint8_t pkt[1536] = {};
      //      EKA_LOG("Waiting for UDP pkt...");

      int size = recv(sock, pkt, sizeof(pkt), 0);
      if (size < 0)
        on_error("size = %d", size);

      if (expectedPktSeq == 1 && getPktSeq(pkt) != 1)
        // recovery Loop not started yet
        continue;

      if (expectedPktSeq != 1 && getPktSeq(pkt) == 1) {
        // end of recovery Loop
        recovered = true;
        EKA_LOG("%s:%u: recording %s %jd packets completed",
                EKA_EXCH_DECODE(exch), id,
                EkaFhMode2STR(op), recoveryPkt.size());
        break;
      }

      if (expectedPktSeq !=
          getPktSeq(pkt)) { // gap in recovery Loop
        EKA_WARN(
            "ERROR: %s:%u: Gap during %s: "
            "expectedPktSeq=%u, "
            "getPktSeq(pkt)=%u: restarting the %s loop",
            EKA_EXCH_DECODE(exch), id, EkaFhMode2STR(op),
            expectedPktSeq, getPktSeq(pkt),
            EkaFhMode2STR(op));
        break;
      }

      // normal
      RecoveryPkt pkt2push = {};
      pkt2push.size = size;
      memcpy(pkt2push.data, pkt, size);

      recoveryPkt.push_back(pkt2push);

      expectedPktSeq = getPktSeq(pkt) + 1;
    }
  }
  EKA_LOG("%s:%u: executing %s %jd packets",
          EKA_EXCH_DECODE(exch), id, EkaFhMode2STR(op),
          recoveryPkt.size());
  for (auto const &p : recoveryPkt) {
    processPkt(pEfhRunCtx, p.data, p.size, op);
  }

  EKA_LOG("%s:%u: %d / %d %s messages processed",
          EKA_EXCH_DECODE(exch), id, iterationsCnt,
          totalIterations, EkaFhMode2STR(op));

  snapshot_active = false;
  snapshotClosed = true;
  gapClosed = true;
  //  inGap           = false;

  close(sock);
  return EKA_OPRESULT__OK;
}

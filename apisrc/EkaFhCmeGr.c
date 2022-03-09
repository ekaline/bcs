#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <sched.h>
#include <time.h>
#include <vector>

#include "EkaFhThreadAttr.h"
#include "EkaFhCmeGr.h"
#include "EkaFhCmeParser.h"

using namespace Cme;

void* getCmeSnapshot(void* attr);
int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);

/* ##################################################################### */
int EkaFhCmeGr::createPktQ() {
  EKA_LOG("%s:%u: Creating PktQ",EKA_EXCH_DECODE(exch),id);
  pktQ = new PktQ(dev,exch,this,id);
  if (pktQ == NULL) on_error("pktQ == NULL");
  return 0;
}

/* ##################################################################### */

int EkaFhCmeGr::bookInit () {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  return 0;
}

/* ##################################################################### */
void EkaFhCmeGr::pushPkt2Q(const uint8_t*       pkt, 
				 uint16_t       pktSize,
				 uint64_t       sequence) {

  if ((int)pktSize > PktElem::PKT_SIZE)
    on_error("pktSize %u > EkaFhPktQElem::PKT_SIZE %d",
	     pktSize,PktElem::PKT_SIZE);

  PktElem* p = pktQ->push();
  memcpy(p->data,pkt,pktSize);
  p->sequence = sequence;
  p->pktSize  = pktSize;

  return;
}
 /* ##################################################################### */

int EkaFhCmeGr::processFromQ(const EfhRunCtx* pEfhRunCtx) {
  bool firstPkt = true;
  while (! pktQ->is_empty()) {
    PktElem* buf = pktQ->pop();
    if (firstPkt) {
      firstPkt = false;
      EKA_LOG("%s:%u: 1st Q pkt sequence = %ju, seq_after_snapshot = %ju",
	      EKA_EXCH_DECODE(exch),id,buf->sequence,seq_after_snapshot);
    }
    if (buf->sequence < seq_after_snapshot) continue;
    processPkt(pEfhRunCtx,
	       buf->data,
	       buf->pktSize,
	       EkaFhMode::MCAST);
    expected_sequence = buf->sequence + 1;
  }
  EKA_LOG("%s:%u: After Q draining expected_sequence = %ju",
	  EKA_EXCH_DECODE(exch),id,expected_sequence);
  return 0;
}

 /* ##################################################################### */
int EkaFhCmeGr::closeSnapshotGap(EfhCtx*           pEfhCtx, 
				 const EfhRunCtx*  pEfhRunCtx, 
				 uint64_t          sequence) {
  firstLifeSeq = sequence;

  std::string threadName = std::string("ST_") + 
    std::string(EKA_EXCH_SOURCE_DECODE(exch)) + 
    '_' + 
    std::to_string(id);
  EkaFhThreadAttr* attr  = new EkaFhThreadAttr(pEfhCtx, 
					       pEfhRunCtx, 
					       this, 
					       sequence, 
					       0, 
					       EkaFhMode::SNAPSHOT);
  if (attr == NULL) on_error("attr = NULL");
  
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    getCmeSnapshot,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   

  return 0;
}
 /* ##################################################################### */

void* getCmeSnapshot(void* attr) {


  auto params {reinterpret_cast<const EkaFhThreadAttr*>(attr)};
  if (params == NULL) on_error("params == NULL");

  auto pEfhRunCtx {params->pEfhRunCtx};
  auto gr {dynamic_cast<EkaFhCmeGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");

  delete params;

#ifdef FH_LAB
  gr->snapshotClosed  = true;
  gr->inGap           = false;
  return NULL;
#endif
  
  pthread_detach(pthread_self());

  EkaDev* dev = gr->dev;
  if (dev == NULL) on_error ("dev == NULL");

  gr->recoveryLoop(pEfhRunCtx, EkaFhMode::SNAPSHOT);

  return NULL;
}

EkaOpResult EkaFhCmeGr::recoveryLoop(const EfhRunCtx* pEfhRunCtx, EkaFhMode op) {
  auto ip   = op == EkaFhMode::DEFINITIONS ? snapshot_ip   : recovery_ip;
  auto port = op == EkaFhMode::DEFINITIONS ? snapshot_port : recovery_port;

  struct RecoveryPkt {
    size_t  size;
    uint8_t data[1536];
  };
  
  std::vector<RecoveryPkt>recoveryPkt;

  int sock = ekaUdpMcConnect(dev, ip, port);
  if (sock < 0) on_error ("sock = %d",sock);
    
  snapshot_active = true;
  snapshotClosed = false;

  bool recovered = false;

  while (!recovered && snapshot_active) {
    recoveryPkt.clear();
    iterationsCnt = 0; 
    uint32_t expectedPktSeq = 1;
    while (snapshot_active) {
      uint8_t pkt[1536] = {};
      int size = recvfrom(sock, pkt, sizeof(pkt), 0, NULL, NULL);
      if (size < 0) on_error("size = %d",size);
    
      if (expectedPktSeq == 1 && getPktSeq(pkt) != 1) // recovery Loop not started yet
	continue;

      if (expectedPktSeq != 1 && getPktSeq(pkt) == 1) {// end of recovery Loop
	recovered = true;
	break;
      }
    
      if (expectedPktSeq != getPktSeq(pkt)) { // gap in recovery Loop
	EKA_WARN("ERROR: %s:%u: expectedPktSeq=%u, getPktSeq(pkt)=%u, %d / %d %s messages processed",
		 EKA_EXCH_DECODE(exch),id,expectedPktSeq,getPktSeq(pkt),
		 iterationsCnt,totalIterations,EkaFhMode2STR(op));
	break;
      }

      // normal
      RecoveryPkt pkt2push = {};
      pkt2push.size = size;
      memcpy(pkt2push.data,pkt,size);
      
      recoveryPkt.push_back(pkt2push);
      
      expectedPktSeq = getPktSeq(pkt) + 1;
    }
  }
  for (auto const& p : recoveryPkt) {
      if (processPkt(pEfhRunCtx,p.data,p.size,op)) break;
  }
  
  EKA_LOG("%s:%u: %d / %d %s messages processed",
	  EKA_EXCH_DECODE(exch),id,
	  iterationsCnt,totalIterations,EkaFhMode2STR(op));
  
  snapshot_active = false;
  snapshotClosed  = true;
  //  inGap           = false;
  
  close (sock);
  return EKA_OPRESULT__OK;
}

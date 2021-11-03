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
  while (! pktQ->is_empty()) {
    PktElem* buf = pktQ->pop();

    if (buf->sequence < expected_sequence) continue;
    processPkt(pEfhRunCtx,
	       buf->data,
	       buf->pktSize,
	       EkaFhMode::MCAST);
    expected_sequence = buf->sequence + 1;
  }
  return 0;
}

 /* ##################################################################### */
int EkaFhCmeGr::closeSnapshotGap(EfhCtx*           pEfhCtx, 
				 const EfhRunCtx*  pEfhRunCtx, 
				 uint64_t          sequence) {
  firstLifeSeq = sequence;
  processedSnapshotMessages = 0;

  std::string threadName = std::string("ST_") + 
    std::string(EKA_EXCH_SOURCE_DECODE(exch)) + 
    '_' + 
    std::to_string(id);
  EkaFhThreadAttr* attr  = new EkaFhThreadAttr(pEfhCtx, 
					       (const EfhRunCtx*)pEfhRunCtx, 
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

  EfhRunCtx* pEfhRunCtx     = params->pEfhRunCtx;
  auto gr {reinterpret_cast<EkaFhCmeGr*>(params->gr)};
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

  int sock = ekaUdpMcConnect(dev, gr->recovery_ip, gr->recovery_port);
  if (sock < 0) on_error ("sock = %d",sock);

  sockaddr_in addr = {};
  addr.sin_addr.s_addr = gr->recovery_ip;
  addr.sin_port        = gr->recovery_port;
  socklen_t addrlen = sizeof(sockaddr);

  gr->snapshot_active = true;
  gr->iterationsCnt = 0;  

  uint32_t expectedPktSeq = 0;

  while (gr->snapshot_active) {
    uint8_t pkt[1536] = {};
    int size = recvfrom(sock, pkt, sizeof(pkt), 0, (sockaddr*) &addr, &addrlen);
    if (size < 0) on_error("size = %d",size);
    if (expectedPktSeq == 0)
      expectedPktSeq = getPktSeq(pkt);
    if (expectedPktSeq != getPktSeq(pkt))
      EKA_WARN("ERROR: expectedPktSeq=%u, getPktSeq(pkt)=%u",
	       expectedPktSeq,getPktSeq(pkt));
    if (gr->processPkt(pEfhRunCtx,pkt,size,EkaFhMode::SNAPSHOT)) break;
    expectedPktSeq = getPktSeq(pkt) + 1;
  }
  gr->snapshot_active = false;
  gr->snapshotClosed  = true;
  gr->inGap           = false;

  EKA_LOG("%s:%u: %d Snapshot messages processed",
	  EKA_EXCH_DECODE(gr->exch),gr->id,gr->processedSnapshotMessages);
  close (sock);

  return NULL;
}

#include "EkaFhCmeGr.h"


/* ##################################################################### */
EkaFhCmeGr::EkaFhCmeGr() {
  pktQ = new PktQ(dev,exch,this,id);
  if (pktQ == NULL) on_error("pktQ == NULL");
}

/* ##################################################################### */

int EkaFhCmeGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
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

int EkaFhCmeGr::processPktFromQ(const EfhRunCtx* pEfhRunCtx) {
  while (! pktQ->is_empty()) {
    PktElem* buf = pktQ->pop();
    //      EKA_LOG("q_len=%u,buf->sequence=%ju, expected_sequence=%ju",q->get_len(),buf->sequence,expected_sequence);

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
				 const EfhInitCtx* pEfhRunCtx, 
				 uint64_t          sequence) {
  gr->firstLifeSeq = sequence;
  gr->processedSnapshotMessages = 0;
  int sock = ekaUdpMcConnect(dev, gr->recovery_ip, gr->recovery_port);
  if (sock < 0) on_error ("sock = %d",sock);

  sockaddr_in addr = {};
  addr.sin_addr.s_addr = gr->recovery_ip;
  addr.sin_port        = gr->recovery_port;
  socklen_t addrlen = sizeof(sockaddr);

  gr->snapshot_active = true;

  while (gr->snapshot_active) {
    uint8_t pkt[1536] = {};
    int size = recvfrom(sock, pkt, sizeof(pkt), 0, (sockaddr*) &addr, &addrlen);
    if (size < 0) on_error("size = %d",size);
    if (gr->processPkt(pEfhRunCtx,pkt,size,EkaFhMode::SNAPSHOT)) break;
    
  }
  gr->snapshot_active = false;
  gr->inGap = false;

  EKA_LOG("%s:%u: %d Snapshot messages processed",
	  EKA_EXCH_DECODE(exch),gr->id,gr->processedSnapshotMessages);
  close (sock);
}

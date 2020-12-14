#include "EkaFhBoxGr.h"

/* ##################################################################### */

bool EkaFhBoxGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
			       const uint8_t*   pkt, 
			       int16_t          pktLen) {
  //  EKA_LOG("%s:%u : pktLen = %u",EKA_EXCH_DECODE(exch),id,pktLen);

  uint8_t* p = (uint8_t*)pkt;
  int idx = 0;
  while (idx < pktLen) {
    uint msgLen       = getHsvfMsgLen(&p[idx],pktLen-idx);
    uint64_t sequence = getHsvfMsgSequence(&p[idx]);
    if (sequence >= expected_sequence) {
      if (parseMsg(pEfhRunCtx,&p[idx+1],sequence,EkaFhMode::MCAST)) return true;
      expected_sequence = sequence + 1;
    }

    idx += msgLen;
    idx += trailingZeros(&p[idx],pktLen-idx );
  }
  return false;
}
 
/* ##################################################################### */
void EkaFhBoxGr::pushUdpPkt2Q(const uint8_t* pkt, int16_t pktLen) {
  uint8_t* p = (uint8_t*)pkt;
  int idx = 0;
  while (idx < pktLen) {
    uint msgLen = getHsvfMsgLen(&p[idx],pktLen - idx);
    char* msgType = ((HsvfMsgHdr*) &p[idx+1])->MsgType;
    if (memcmp(msgType,"F ",2) == 0 ||  // Quote
	memcmp(msgType,"Z ",2) == 0) {  // Time
      if (msgLen > fh_msg::MSG_SIZE) 
	on_error("msgLen %u > fh_msg::MSG_SIZE %u",msgLen,fh_msg::MSG_SIZE);
      fh_msg* n = q->push();
      memcpy (n->data,&p[idx+1],msgLen - 1);
      n->sequence = getHsvfMsgSequence(p);
      n->gr_id = id;
    }
    idx += msgLen;
    idx += trailingZeros(&p[idx],pktLen-idx);

  }
  return;
}
 
/* ##################################################################### */

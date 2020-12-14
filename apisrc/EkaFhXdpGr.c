#include "EkaFhXdpGr.h"

/* ############################################################## */

bool EkaFhXdpGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
			       uint             pktSize, 
			       uint             streamIdx, 
			       const uint8_t*   pktPtr, 
			       uint             msgInPkt, 
			       uint64_t         seq) {
  uint8_t* p = (uint8_t*)pktPtr + sizeof(XdpPktHdr);//+sizeof(XdpStreamId);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = EKA_XDP_MSG_LEN(p);
    //    if (sequence == getExpectedSeq(streamIdx)) { // = ignore back in time messages

    //    EKA_LOG("pktSize=%u, delta_p=%ju, MsgType=%u, MsgSize=%u",pktSize,p - pktPtr,((XdpMsgHdr*)p)->MsgType,msg_len);
    //-----------------------------------------------------------------------------
    parseMsg(pEfhRunCtx,p,sequence++,EkaFhMode::MCAST); // never end of MC from Msg
    //-----------------------------------------------------------------------------
    setExpectedSeq(streamIdx,sequence);
    //    } 
    p += msg_len;
  }
  return false;
}


/* ############################################################## */

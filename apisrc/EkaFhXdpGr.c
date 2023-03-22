#include "EkaFhXdpGr.h"
#include "EkaFhXdpParser.h"

using namespace Xdp;

/* ############################################################## */

bool EkaFhXdpGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
			       uint             pktSize, 
			       uint             streamIdx, 
			       const uint8_t*   pktPtr, 
			       uint             msgInPkt, 
			       uint64_t         seq) {
  const uint8_t* p = pktPtr + sizeof(XdpPktHdr);//+sizeof(XdpStreamId);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = EKA_XDP_MSG_LEN(p);
    //-----------------------------------------------------------------------------
    parseMsg(pEfhRunCtx,p,sequence++,EkaFhMode::MCAST); // never end of MC from Msg
    //-----------------------------------------------------------------------------
    setExpectedSeq(streamIdx,sequence);

    p += msg_len;
  }
  return false;
}

/* ##################################################################### */

int EkaFhXdpGr::bookInit() {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  // XDP does not provide RFQ IDs. We keep our own ID counter, using the group ID as the high byte.
  prevAuctionId = static_cast<uint64_t>(id + 1) << (64 - 8);

  return 0;
}

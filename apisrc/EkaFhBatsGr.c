#include "EkaFhBatsGr.h"
#include "eka_fh_book.h"


/* ##################################################################### */

bool EkaFhBatsGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				const uint8_t*   pkt, 
				uint             msgInPkt, 
				uint64_t         seq) {
  uint indx = sizeof(batspitch_sequenced_unit_header);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint8_t msg_len = pkt[indx];
    uint8_t* msgData = (uint8_t*)&pkt[indx];
    //-----------------------------------------------------------------------------
    if (parseMsg(pEfhRunCtx,msgData,sequence++,EkaFhMode::MCAST)) return true;
    //-----------------------------------------------------------------------------
    expected_sequence = sequence;
    indx += msg_len;
  }
  return false;
}

/* ##################################################################### */
void EkaFhBatsGr::pushUdpPkt2Q(const uint8_t* pkt, 
			       uint           msgInPkt, 
			       uint64_t       sequence) {
  uint indx = sizeof(batspitch_sequenced_unit_header);
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint8_t msg_len = pkt[indx];
    if (msg_len > fh_msg::MSG_SIZE) on_error("msg_len %u > fh_msg::MSG_SIZE %u",msg_len,fh_msg::MSG_SIZE);
    fh_msg* n = q->push();
    memcpy (n->data,&pkt[indx],msg_len);
    n->sequence = sequence++;
    n->gr_id    = id;
    indx       += msg_len;
  }
  return;
}

/* ##################################################################### */

int EkaFhBatsGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new BatsBook(pEfhCtx,pEfhInitCtx,this);
  if (book == NULL) on_error("book == NULL, &book = %p",&book);
  ((BatsBook*)book)->init();
  return 0;
}

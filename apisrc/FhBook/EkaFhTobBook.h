#ifndef _EKA_FH_TOB_BOOK_H_
#define _EKA_FH_TOB_BOOK_H_

#include <string>

#include "EkaFhBook.h"
#include "EkaFhTobSecurity.h"

template <const uint SEC_HASH_SCALE,class FhSecurity, class SecurityIdT, class PriceT, class SizeT>
  class EkaFhTobBook : public EkaFhBook  {
 public:
  //  using FhSecurity   = EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>;

  /* ####################################################### */

 EkaFhTobBook(EkaDev* _dev, EkaLSI _grId, EkaSource   _exch) 
   : EkaFhBook (_dev,_grId,_exch) {}
  /* ####################################################### */

  void            init() {
    EKA_LOG("%s:%u : TOB book with preallocated: %ju Securities Hash lines, no PLEVELS, no ORDERS",
	  EKA_EXCH_DECODE(exch),grId,SEC_HASH_LINES);
  }
  /* ####################################################### */

  FhSecurity*  findSecurity(SecurityIdT secId) {
    uint32_t index =  secId & SEC_HASH_MASK;
    if (index >= SEC_HASH_LINES) on_error("index = %u >= SEC_HASH_LINES %ju",index,SEC_HASH_LINES);
    if (sec[index] == NULL) return NULL;

    FhSecurity* sp = sec[index];

    while (sp != NULL) {
      if (sp->secId == secId) return sp;
      sp = (FhSecurity*)sp->next;
    }

    return NULL;
  }
  /* ####################################################### */

  FhSecurity*  subscribeSecurity(SecurityIdT     secId,
				    EfhSecurityType type,
				    EfhSecUserData  userData,
				    uint64_t        opaqueAttrA,
				    uint64_t        opaqueAttrB) {
    FhSecurity* s = new FhSecurity(secId,type,userData,opaqueAttrA,opaqueAttrB);
    if (s == NULL) on_error("s == NULL, new FhSecurity failed");
  
    uint32_t index = secId & SEC_HASH_MASK;
    if (index >= SEC_HASH_LINES) 
      on_error("index = %u >= SEC_HASH_LINES %ju",index,SEC_HASH_LINES);

    if (sec[index] == NULL) {
      sec[index] = s; // empty bucket
    } else {
      FhSecurity *sp = sec[index];
      while (sp->next != NULL) sp = (FhSecurity*)sp->next;
      sp->next = (EkaFhSecurity*) s;
    }
    //    numSecurities++;
    return s;
  }
  /* ####################################################### */

  int generateOnQuote (const EfhRunCtx* pEfhRunCtx, 
		       FhSecurity*      s, 
		       uint64_t         sequence, 
		       uint64_t         timestamp,
		       uint             gapNum) {

    if (s == NULL) on_error("s == NULL");

    EfhQuoteMsg msg = {};
    msg.header.msgType        = EfhMsgType::kQuote;
    msg.header.group.source   = exch;
    msg.header.group.localId  = grId;
    msg.header.securityId     = s->secId;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = timestamp;
    msg.header.queueSize      = 0; //gr->q == NULL ? 0 : gr->q->get_len();
    msg.header.gapNum         = gapNum;
    msg.tradeStatus           = s->trading_action == EfhTradeStatus::kHalted ? EfhTradeStatus::kHalted :
      s->option_open ? EfhTradeStatus::kNormal : EfhTradeStatus::kClosed;

    msg.bidSide.price         = s->bid_price;
    msg.bidSide.size          = s->bid_size;
    msg.bidSide.customerSize  = s->bid_cust_size;
    msg.bidSide.bdSize        = s->bid_bd_size;

    msg.askSide.price         = s->ask_price;
    msg.askSide.size          = s->ask_size;
    msg.askSide.customerSize  = s->ask_cust_size;
    msg.askSide.bdSize        = s->ask_bd_size;

    if (pEfhRunCtx->onEfhQuoteMsgCb == NULL) on_error("Uninitialized pEfhRunCtx->onEfhQuoteMsgCb");


#ifdef EKA_TIME_CHECK
    auto start = std::chrono::high_resolution_clock::now();
#endif
    pEfhRunCtx->onEfhQuoteMsgCb(&msg, (EfhSecUserData)s->efhUserData, pEfhRunCtx->efhRunUserData);

#ifdef EKA_TIME_CHECK
    auto finish = std::chrono::high_resolution_clock::now();
    uint duration_ms = (uint) std::chrono::duration_cast<std::chrono::milliseconds>(finish-start).count();
    if (duration_ms > 5) EKA_WARN("WARNING: onQuote Callback took %u ms",duration_ms);
#endif

    return 0;
  }
/* ####################################################### */
  void sendTobImage (const EfhRunCtx* pEfhRunCtx) {
    for (uint i = 0; i < SEC_HASH_LINES; i++) {
      if (sec[i] == NULL) continue;
      FhSecurity* s = sec[i];
      while (s != NULL) {
	generateOnQuote(pEfhRunCtx,s,0,std::max(s->bid_ts,s->ask_ts),1);
	s = (FhSecurity*)s->next;
      }
    }
    return;
  }
/* ####################################################### */

 private:

/* ####################################################### */
  
 public:
  static const uint64_t SEC_HASH_LINES = 0x1 << SEC_HASH_SCALE;
  static const uint64_t SEC_HASH_MASK  = (0x1 << SEC_HASH_SCALE) - 1;

  FhSecurity*  sec[SEC_HASH_LINES] = {}; // array of pointers to Securities

};

#endif

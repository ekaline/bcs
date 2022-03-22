#ifndef _EKA_FH_CME_BOOK_H_
#define _EKA_FH_CME_BOOK_H_

#include "EkaFhBook.h"
#include "EkaFhTypes.h"

template <const uint SEC_HASH_SCALE,
  class FhSecurity, 
  class SecurityIdT, 
  class PriceT, 
  class SizeT>
  class EkaFhCmeBook : public EkaFhBook  {
 public:
  /* ####################################################### */
 EkaFhCmeBook(EkaDev* _dev, 
	      EkaLSI _grId, 
	      EkaSource   _exch) 
   : EkaFhBook (_dev,_grId,_exch) {}
  /* ####################################################### */
  inline FhSecurity*  findSecurity(SecurityIdT secId) {
    uint32_t index =  secId & SEC_HASH_MASK;
    if (index >= SEC_HASH_LINES)
      on_error("index = %u >= SEC_HASH_LINES %ju",
	       index,SEC_HASH_LINES);
    if (! sec[index]) return NULL;

    FhSecurity* sp = sec[index];

    while (sp != NULL) {
      if (sp->secId == secId) return sp;
      sp = (FhSecurity*)sp->next;
    }

    return NULL;
  }
  /* ####################################################### */
  inline int invalidate(const EfhRunCtx* pEfhRunCtx,
			uint64_t         pktSeq, 
			uint64_t         pktTime,
			uint             gapNum) {
    EKA_LOG("%s:%u: invalidating Channel (full book)",
	    EKA_EXCH_DECODE(exch),grId);
    int secCnt = 0;
    
    for (size_t hashLine = 0; hashLine < SEC_HASH_LINES; hashLine++) {
      auto s = dynamic_cast<FhSecurity*>(sec[hashLine]);      
      while (s) {
	auto n = s->next;
	dynamic_cast<FhSecurity*>(s)->reset();
	generateOnQuote (pEfhRunCtx,
			 s,
			 pktSeq,
			 pktTime,
			 gapNum);
	secCnt++;
	s = dynamic_cast<FhSecurity*>(n);
      }
    }
    EKA_LOG("%s:%u: %d Securities invalidated",
	    EKA_EXCH_DECODE(exch),grId,secCnt);
    return secCnt;
  }
  
  /* ####################################################### */
  inline FhSecurity*  subscribeSecurity(SecurityIdT     secId,
				 EfhSecurityType type,
				 EfhSecUserData  userData,
				 uint64_t        opaqueAttrA,
				 uint64_t        opaqueAttrB) {
    FhSecurity* s = new FhSecurity(secId,type,userData,opaqueAttrA,opaqueAttrB);
    if (!s) on_error("!s, new FhSecurity failed");
  
    uint32_t index = secId & SEC_HASH_MASK;
    if (index >= SEC_HASH_LINES) 
      on_error("index = %u >= SEC_HASH_LINES %ju",index,SEC_HASH_LINES);

    if (! sec[index]) {
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
  void init() {
    EKA_LOG("%s:%u : CME book with preallocated: "
	    "%ju Securities Hash lines, no PLEVELS, no ORDERS",
	  EKA_EXCH_DECODE(exch),grId,SEC_HASH_LINES);
  }
  /* ####################################################### */

  inline int generateOnQuote (const EfhRunCtx* pEfhRunCtx, 
		       FhSecurity*      s, 
		       uint64_t         sequence, 
		       uint64_t         timestamp,
		       uint             gapNum) {

    if (!s) on_error("!s");

    EfhQuoteMsg msg = {};
    msg.header.msgType        = EfhMsgType::kQuote;
    msg.header.group.source   = exch;
    msg.header.group.localId  = grId;
    msg.header.securityId     = s->secId;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = timestamp;
    msg.header.deltaNs        = 0;
    msg.header.gapNum         = gapNum;
    msg.tradeStatus           = s->tradeStatus;

    msg.bidSide.price         = s->bid->getEntryPrice(0) / PRICE_SCALE;
    msg.bidSide.size          = s->bid->getEntrySize(0);

    msg.askSide.price         = s->ask->getEntryPrice(0) / PRICE_SCALE;
    msg.askSide.size          = s->ask->getEntrySize(0);

    if (pEfhRunCtx->onEfhQuoteMsgCb == NULL) 
      on_error("Uninitialized pEfhRunCtx->onEfhQuoteMsgCb");


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

  int generateOnOrder (const EfhRunCtx* pEfhRunCtx, 
		       FhSecurity*      s, 
		       uint64_t         sequence, 
		       uint64_t         timestamp,
		       SideT            side,
		       uint             gapNum) {

    if (!s) on_error("!s");

    if (side != SideT::BID && side != SideT::ASK)
      on_error("Unexpected side = %d",(int)side);
    
    EfhOrderMsg msg = {};
    msg.header.msgType        = EfhMsgType::kQuote;
    msg.header.group.source   = exch;
    msg.header.group.localId  = grId;
    msg.header.securityId     = s->secId;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = timestamp;
    msg.header.deltaNs        = 0;
    msg.header.gapNum         = gapNum;
    msg.tradeStatus           = s->tradeStatus;

    msg.orderSide             = side == SideT::BID ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
      
    msg.bookSide.price        = side == SideT::BID ? (s->bid->getEntryPrice(0) / PRICE_SCALE) :
      (s->ask->getEntryPrice(0) / PRICE_SCALE);
    
    msg.bookSide.size          = side == SideT::BID ? s->bid->getEntrySize(0) :
      s->ask->getEntrySize(0);

    if (pEfhRunCtx->onEfhOrderMsgCb == NULL) 
      on_error("Uninitialized pEfhRunCtx->onEfhQuoteMsgCb");


#ifdef EKA_TIME_CHECK
    auto start = std::chrono::high_resolution_clock::now();
#endif

    pEfhRunCtx->onEfhOrderMsgCb(&msg, (EfhSecUserData)s->efhUserData, pEfhRunCtx->efhRunUserData);

#ifdef EKA_TIME_CHECK
    auto finish = std::chrono::high_resolution_clock::now();
    uint duration_ms = (uint) std::chrono::duration_cast<std::chrono::milliseconds>(finish-start).count();
    if (duration_ms > 5) EKA_WARN("WARNING: onQuote Callback took %u ms",duration_ms);
#endif

    return 0;
  }
  /* ####################################################### */


  
 public:
  static const uint64_t PRICE_SCALE    = 1; // 1e8;
  static const uint64_t SEC_HASH_LINES = 0x1 << SEC_HASH_SCALE;
  static const uint64_t SEC_HASH_MASK  = (0x1 << SEC_HASH_SCALE) - 1;

  FhSecurity*  sec[SEC_HASH_LINES] = {}; // array of pointers to Securities

};

#endif

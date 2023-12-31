#include <charconv>
#include <limits>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "EkaFhParserCommon.h"
#include "EkaFhCmeParser.h"

#include "EkaFhRunGroup.h"
#include "EkaFhCmeGr.h"
#include "EkaFhCme.h"

using namespace Cme;
namespace chrono = std::chrono;

static int64_t computeFinalPriceFactor(Decimal9_T displayFactor) {
  // Returns the number we need to divide the protocol message prices by to
  // get an EFH__PRICE_SCALE scaled price.
  if (displayFactor > EFH_CME_PRICE_SCALE) {
    // We expect the display factor to always be less than or equal to 1e9,
    // i.e., that it is used to indicate a fraction. If it is not, we wouldn't
    // be able to represent the scale factor as an integer, which is what we
    // expect.
    on_error("displayFactor %ld would saturate to zero", displayFactor);
  }
  return (EFH_CME_PRICE_SCALE / displayFactor) * CMEPriceFactor;
}

template<typename T>
constexpr T replaceIntNullWith(T num, T adjusted, T fallback) {
  if (num == std::numeric_limits<T>::max()) {
    return fallback;
  }
  return adjusted;
}

template<typename T>
constexpr T replaceIntNullWith(T num, T fallback) {
  return replaceIntNullWith(num, num, fallback);
}

constexpr uint64_t DEFAULT_OPT_DEPTH = 3;
constexpr uint64_t DEFAULT_FUT_DEPTH = 10;

/* ##################################################################### */

bool EkaFhCmeGr::processPkt(const EfhRunCtx* pEfhRunCtx,
			    const uint8_t*   pkt, 
			    int16_t          pktLen,
			    EkaFhMode        op) {
#ifdef _PRINT_ALL_
  printPkt(pkt, pktLen);
#endif

  auto p {pkt};
  auto pktHdr {reinterpret_cast<const PktHdr*>(p)};
  p += sizeof(*pktHdr);
  
  uint64_t  pktTime = pktHdr->time;
  SequenceT pktSeq  = pktHdr->seq;

  while (p - pkt < pktLen) {
    auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};
    if (p - pkt + msgHdr->size > pktLen) 
      on_error("p (%p) - pkt (%p) + msgHdr->size (%u) %jd > pktLen %u",
	       p,pkt,msgHdr->size,p - pkt + msgHdr->size,pktLen);

    switch (msgHdr->templateId) {
      /* ##################################################################### */
    case MsgId::ChannelReset4 :
      process_ChannelReset4(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::SecurityStatus30 :
      process_SecurityStatus30(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::QuoteRequest39 :
      process_QuoteRequest39(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDIncrementalRefreshBook46 :
      if (op == EkaFhMode::MCAST)
	process_MDIncrementalRefreshBook46(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDIncrementalRefreshTradeSummary48 :
      process_MDIncrementalRefreshTradeSummary48(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::SnapshotFullRefresh52 : 
      if (op == EkaFhMode::SNAPSHOT)
	process_SnapshotFullRefresh52(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionFuture27 :
      //      if (op == EkaFhMode::DEFINITIONS)
	process_MDInstrumentDefinitionFuture27(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionFuture54 :
      //      if (op == EkaFhMode::DEFINITIONS)
	process_MDInstrumentDefinitionFuture54(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionOption55 :
      //      if (op == EkaFhMode::DEFINITIONS)
	process_MDInstrumentDefinitionOption55(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionSpread56 :
      process_MDInstrumentDefinitionSpread56(pEfhRunCtx,p,pktTime,pktSeq);
      break;      
      /* ##################################################################### */     
    default:
      break;
    }
    if (op == EkaFhMode::DEFINITIONS && isDefinitionMsg(msgHdr->templateId)) {
      auto root {reinterpret_cast<const Definition_commonMainBlock*>(p + sizeof(*msgHdr))};
      ++iterationsCnt;
      totalIterations = root->TotNumReports;
      /* EKA_LOG("pktSeq = %u, iteration %d out of %d", */
      /* 	      getPktSeq(pkt),iterationsCnt,totalIterations); */
      if (iterationsCnt == totalIterations) return true;
    }
    if (op == EkaFhMode::SNAPSHOT && isSnapshotMsg(msgHdr->templateId)) {
      auto root {reinterpret_cast<const Refresh_commonMainBlock*>(p + sizeof(*msgHdr))};
      ++iterationsCnt;
      totalIterations = root->TotNumReports;
      /* EKA_LOG("pktSeq = %u, iteration %d out of %d", */
      /* 	      getPktSeq(pkt),iterationsCnt,totalIterations); */
      if (iterationsCnt == totalIterations) return true;
    }

    p += msgHdr->size;
    
  }
  return false;
}

inline uint32_t encodeYYYYMMDD(const struct tm* t) {
  if (t == NULL) return 0;
  const uint32_t year = t->tm_year + 1900;
  const uint32_t month = t->tm_mon + 1;
  const uint32_t day = t->tm_mday;
  return (year * 1'00'00) + (month * 1'00) + day;
}

inline bool getExpiry(const DefinitionEventEntry& lastTradeEvent, EfhSecurityDef& commonDef,
                      char (&expiryFmtBuf)[32]) {
  const chrono::system_clock::time_point expiryTime(chrono::nanoseconds(lastTradeEvent.EventTime));
  commonDef.expiryTime = chrono::system_clock::to_time_t(expiryTime);
  struct tm timeInfoBuf;
  struct tm* timeInfo = localtime_r(&commonDef.expiryTime, &timeInfoBuf);
  commonDef.expiryDate = encodeYYYYMMDD(timeInfo);

  const static auto todayAtMidnight = systemClockAtMidnight();
  if (expiryTime < todayAtMidnight) {
    if (timeInfo == NULL || strftime(expiryFmtBuf, sizeof(expiryFmtBuf), "%F %T%z", timeInfo) == 0) {
      // Failed to write formatted expiration
      expiryFmtBuf[0] = '?';
      expiryFmtBuf[1] = '\0';
    }
    return false;
  }

  return true;
}

/* ##################################################################### */
  int EkaFhCmeGr::process_QuoteRequest39(const EfhRunCtx* pEfhRunCtx,
					 const uint8_t*   pMsg,
					 const uint64_t   pktTime,
					 const SequenceT  pktSeq) {
    auto m      {pMsg};
    auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
    m += sizeof(*msgHdr);
    auto rootBlock {reinterpret_cast<const QuoteRequest39_mainBlock*>(m)};
    uint64_t transactTime = rootBlock->TransactTime;
    m += msgHdr->blockLen;
    /* ------------------------------- */
    auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
    m += sizeof(*pGroupSize);
    /* ------------------------------- */

    EfhAuctionUpdateMsg msg{};
    msg.header.msgType        = EfhMsgType::kAuctionUpdate;
    msg.header.group.source   = EkaSource::kCME_SBE;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.sequenceNumber = pktSeq;
    msg.header.timeStamp      = pktTime;
    msg.header.transactTime   = transactTime;
    msg.header.gapNum         = gapNum;

    copySymbol(msg.auctionId, rootBlock->QuoteReqID);
    msg.auctionType = EfhAuctionType::kSpreadSolicitation;
    msg.updateType = EfhAuctionUpdateType::kNew;

    /* ------------------------------- */
    for (uint i = 0; i < pGroupSize->numInGroup; i++) {
      auto e {reinterpret_cast<const QuoteRequest39_legEntry*>(m)};

      msg.securityType      = EfhSecurityType::kRfq;
      auto s {book->findSecurity(e->SecurityID)};
      if (! s) continue;
      msg.header.securityId = e->SecurityID;
      msg.side              = getSide39(e->Side);
      msg.quantity          = replaceIntNullWith<Int32NULL_T>(e->OrderQty, 0);
      if (pEfhRunCtx->onEfhAuctionUpdateMsgCb == NULL)
	on_error("pEfhRunCtx->onEfhAuctionUpdateMsgCb == NULL");
      pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) s->efhUserData, pEfhRunCtx->efhRunUserData);

      m += pGroupSize->blockLength;    
    }
  
    return msgHdr->size;
  }

/* ##################################################################### */     
int EkaFhCmeGr::process_MDIncrementalRefreshBook46(const EfhRunCtx* pEfhRunCtx,
						   const uint8_t*   pMsg,
						   const uint64_t   pktTime,
						   const SequenceT  pktSeq) {
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);
  auto rootBlock {reinterpret_cast<const MDIncrementalRefreshBook46_mainBlock*>(m)};
  uint64_t transactTime = rootBlock->TransactTime;
  m += msgHdr->blockLen;
  /* ------------------------------- */
  auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize);
  /* ------------------------------- */
  for (uint i = 0; i < pGroupSize->numInGroup; i++) {
    auto e {reinterpret_cast<const IncrementalRefreshMdEntry*>(m)};
    m += pGroupSize->blockLength;
    auto s {book->findSecurity(e->SecurityID)};
    if (!s)
#ifdef FH_SUBSCRIBE_ALL
      s = book->subscribeSecurity(e->SecurityID,
				  (EfhSecurityType)1,
				  (EfhSecUserData)0,
				  1,3);
#else      
    continue; // next IncrementalRefreshMdEntry
#endif
    if (e->MDEntryType == MDEntryTypeBook_T::BookReset)
      on_error("MDEntryTypeBook_T::BookReset not supported for MDIncrementalRefreshBook46");

    if (s->lastMsgSeqNumProcessed != 0) { // after Snapshot
      if (pktSeq <= s->lastMsgSeqNumProcessed)
	continue; // skip buffered messages
      else
	s->lastMsgSeqNumProcessed = 0;
    }
    
    const auto side {getSide46(e->MDEntryType)};
    if (side == SideT::OTHER) return msgHdr->size;
    const int64_t finalPriceFactor = s->getFinalPriceFactor();
    bool tobChange = false;
    switch (e->MDUpdateAction) {
    case MDUpdateAction_T::New:
      tobChange = s->newPlevel(side,
			       e->MDPriceLevel,
			       e->MDEntryPx / finalPriceFactor,
			       e->MDEntrySize);
      break;
    case MDUpdateAction_T::Change:
      tobChange = s->changePlevel(side,
				  e->MDPriceLevel,
				  e->MDEntryPx / finalPriceFactor,
				  e->MDEntrySize);
      break;
    case MDUpdateAction_T::Delete:
      tobChange = s->deletePlevel(side,
				  e->MDPriceLevel);
      break;
    case MDUpdateAction_T::DeleteThru:
    case MDUpdateAction_T::DeleteFrom:
    case MDUpdateAction_T::Overlay:
    default:
      on_error("Unexpected MDUpdateAction %s (%u)",
	       MDpdateAction2STR(e->MDUpdateAction),(uint)e->MDUpdateAction);
    }
#ifdef _EKA_CHECK_BOOK_INTEGRITY
      if (! s->checkBookIntegrity()) {
	TEST_LOG("ERROR after: %s for SecurityID %d (leg %d / %d),pktSeq=%d",
		 MDpdateAction2STR(e->MDUpdateAction),e->SecurityID,
		 i,pGroupSize->numInGroup,
		 pktSeq
		 );
	print_MDIncrementalRefreshBook46(pMsg,pktSeq);
	s->printSecurityBook();
	on_error("End of test");
      }
#endif   
    /* if (tobChange && fh->print_parsed_messages) { */
    /*   fprintf(parser_log,"generateOnQuote: BP=%ju,BS=%d,AP=%ju,AS=%d\n", */
    /* 	  s->bid->getEntryPrice(0), */
    /* 	  s->bid->getEntrySize(0), */
    /* 	  s->ask->getEntryPrice(0), */
    /* 	  s->ask->getEntrySize(0) */
    /* 	  ); */
    /* } */

    if (tobChange) {
      book->generateOnQuote (pEfhRunCtx,
                             s,
                             pktSeq,
			     pktTime,
                             transactTime,
                             gapNum);
    }
    /* if (tobChange) book->generateOnOrder (pEfhRunCtx,  */
    /* 				      s,  */
    /* 				      pktSeq, */
    /* 				      pktTime, */
    /* 				      side, */
    /* 				      gapNum); */

    
    if (fh->print_parsed_messages) {
      if (pEfhRunCtx->onEfhMdCb == NULL) on_error("pEfhRunCtx->onMd == NULL");
      uint8_t msgBuf[2000] = {};
      auto hdr {reinterpret_cast<EfhMdHeader*>(msgBuf)};

      hdr->mdRawMsgType   = static_cast<decltype(hdr->mdRawMsgType)>(e->MDUpdateAction);
      hdr->group.source   = exch;
      hdr->group.localId  = id;
      hdr->sequenceNumber = pktSeq;
      hdr->timeStamp      = pktTime;
      switch (e->MDUpdateAction) {
      case MDUpdateAction_T::New: {
	auto dstMsg {reinterpret_cast<MdNewPlevel*>(msgBuf)};
	    
	hdr->mdMsgType  = EfhMdType::kNewPlevel;
	hdr->mdMsgSize  = sizeof(MdNewPlevel);
	hdr->securityId = e->SecurityID;

	dstMsg->pLvl    = e->MDPriceLevel;
	dstMsg->side    = side == SideT::BID ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
	dstMsg->price   = e->MDEntryPx / finalPriceFactor;
	dstMsg->size    = e->MDEntrySize;
	    
	pEfhRunCtx->onEfhMdCb(hdr,pEfhRunCtx->efhRunUserData);
      }
	break;
      case MDUpdateAction_T::Change: {
	auto dstMsg {reinterpret_cast<MdChangePlevel*>(msgBuf)};
	    
	hdr->mdMsgType  = EfhMdType::kChangePlevel;
	hdr->mdMsgSize  = sizeof(MdChangePlevel);
	hdr->securityId = e->SecurityID;

	dstMsg->pLvl    = e->MDPriceLevel;
	dstMsg->side    = side == SideT::BID ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
	dstMsg->price   = e->MDEntryPx / finalPriceFactor;
	dstMsg->size    = e->MDEntrySize;
	    
	pEfhRunCtx->onEfhMdCb(hdr,pEfhRunCtx->efhRunUserData);
      }

	break;
      case MDUpdateAction_T::Delete: {
	auto dstMsg {reinterpret_cast<MdDeletePlevel*>(msgBuf)};
	    
	hdr->mdMsgType  = EfhMdType::kDeletePlevel;
	hdr->mdMsgSize  = sizeof(MdDeletePlevel);
	hdr->securityId = e->SecurityID;

	dstMsg->pLvl    = e->MDPriceLevel;
	dstMsg->side    = side == SideT::BID ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
	    
	pEfhRunCtx->onEfhMdCb(hdr,pEfhRunCtx->efhRunUserData);
      }


	break;
      case MDUpdateAction_T::DeleteThru:
      case MDUpdateAction_T::DeleteFrom:
      case MDUpdateAction_T::Overlay:
      default:
	on_error("Unexpected MDUpdateAction %s (%u)",
		 MDpdateAction2STR(e->MDUpdateAction),(uint)e->MDUpdateAction);
      }
    }	
  }
  return msgHdr->size;
}

/* ##################################################################### */     
int EkaFhCmeGr::process_MDIncrementalRefreshTradeSummary48(const EfhRunCtx* pEfhRunCtx,
							   const uint8_t*   pMsg,
							   const uint64_t   pktTime,
							   const SequenceT  pktSeq) {
    auto m      {pMsg};
    auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
    m += sizeof(*msgHdr);
    auto rootBlock {reinterpret_cast<const MDIncrementalRefreshTradeSummary48_mainBlock*>(m)};
    uint64_t transactTime = rootBlock->TransactTime;
    m += msgHdr->blockLen;
    /* ------------------------------- */
    auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
    m += sizeof(*pGroupSize);
    /* ------------------------------- */
    for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	auto e {reinterpret_cast<const MDIncrementalRefreshTradeSummary48_mdEntry*>(m)};
	m += pGroupSize->blockLength;
	
	FhSecurity* s = book->findSecurity(e->SecurityID);
	if (!s) continue;

	EfhTradeMsg msg{};
        msg.header.msgType = EfhMsgType::kTrade;
        msg.header.group.source   = exch;
        msg.header.group.localId  = id;
        msg.header.underlyingId   = 0;
        msg.header.securityId     = (uint64_t) e->SecurityID;
        msg.header.sequenceNumber = pktSeq;
        msg.header.timeStamp      = pktTime;
        msg.header.transactTime   = transactTime;
        msg.header.gapNum         = gapNum;

        msg.price       = e->MDEntryPx / s->getFinalPriceFactor();
        msg.size        = (uint32_t)e->MDEntrySize;
        msg.tradeStatus = EfhTradeStatus::kNormal;
        msg.tradeCond   = EfhTradeCond::kREG;

	pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    }  
    return msgHdr->size;
}
/* ##################################################################### */     

int EkaFhCmeGr::process_ChannelReset4(const EfhRunCtx* pEfhRunCtx,
                                      const uint8_t*   pMsg,
                                      const uint64_t   pktTime,
                                      const SequenceT  pktSeq) {
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);
  book->invalidate(pEfhRunCtx,pktSeq,pktTime,gapNum,true);
  return msgHdr->size;
}

/* ##################################################################### */     

int EkaFhCmeGr::process_SecurityStatus30(const EfhRunCtx* pEfhRunCtx,
					 const uint8_t*   pMsg,
					 const uint64_t   pktTime,
					 const SequenceT  pktSeq) {
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);

  auto rootBlock {reinterpret_cast<const SecurityStatus30_mainBlock*>(m)};
  uint64_t transactTime = rootBlock->TransactTime;

  auto s {book->findSecurity(rootBlock->SecurityID)};
  if (!s) return msgHdr->size;

  s->tradeStatus = setEfhTradeStatus(rootBlock->SecurityTradingStatus);

  book->generateOnQuote (pEfhRunCtx,
			 s,
			 pktSeq,
			 pktTime,
			 transactTime,
			 gapNum);

  return msgHdr->size;
}
/* ##################################################################### */     

int EkaFhCmeGr::process_SnapshotFullRefresh52(const EfhRunCtx* pEfhRunCtx,
						   const uint8_t*   pMsg,
						   const uint64_t   pktTime,
						   const SequenceT  pktSeq) {
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);
  auto rootBlock {reinterpret_cast<const SnapshotFullRefresh52_mainBlock*>(m)};
  uint64_t transactTime = rootBlock->TransactTime;
  m += msgHdr->blockLen;

  //  seq_after_snapshot = rootBlock->LastMsgSeqNumProcessed + 1;
  
  auto s {book->findSecurity(rootBlock->SecurityID)};
  if (!s) return msgHdr->size;

  s->lastMsgSeqNumProcessed = rootBlock->LastMsgSeqNumProcessed;
  
  s->tradeStatus = setEfhTradeStatus(rootBlock->MDSecurityTradingStatus);
  /* ------------------------------- */
  auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize);
  /* ------------------------------- */
  for (uint i = 0; i < pGroupSize->numInGroup; i++) {
    auto e {reinterpret_cast<const MDSnapshotFullRefreshMdEntry*>(m)};
    
    const auto side {getSide52(e->MDEntryType)};
    if (side == SideT::OTHER) continue;
    const int64_t finalPriceFactor = s->getFinalPriceFactor();

    bool tobChange = s->changePlevel(side,
				  e->MDPriceLevel,
				  e->MDEntryPx / finalPriceFactor,
				  e->MDEntrySize);
    if (tobChange) {
      book->generateOnQuote (pEfhRunCtx,
                             s,
                             pktSeq,
			     pktTime,
                             transactTime,
                             gapNum);
    }
    m += pGroupSize->blockLength;    
  }
  return msgHdr->size;
}

/* ##################################################################### */     


int EkaFhCmeGr::process_MDInstrumentDefinitionFuture27(const EfhRunCtx* pEfhRunCtx,
						       const uint8_t*   pMsg,
						       const uint64_t   pktTime,
						       const SequenceT  pktSeq) {
  // not used
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  return msgHdr->size;
}
/* ##################################################################### */

int EkaFhCmeGr::process_MDInstrumentDefinitionFuture54(const EfhRunCtx* pEfhRunCtx,
						       const uint8_t*   pMsg,
						       const uint64_t   pktTime,
						       const SequenceT  pktSeq) {
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);
  auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionFuture54_mainBlock*>(m)};
  m += msgHdr->blockLen;
  /* ------------------------------- */
  //  auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

  const int64_t priceAdjustFactor = computeFinalPriceFactor(rootBlock->DisplayFactor);
  int64_t strikePriceFactor;
  if (memcmp(rootBlock->Asset, "SI", 3) == 0) {
    strikePriceFactor = computeFinalPriceFactor(   10'000'000); // 0.01
  } else if (memcmp(rootBlock->Asset, "GC", 3) == 0) {
    strikePriceFactor = computeFinalPriceFactor(1'000'000'000); // 1.0
  } else {
    strikePriceFactor = priceAdjustFactor;
  }

  EfhFutureDefinitionMsg msg{};
  msg.header.msgType        = EfhMsgType::kFutureDefinition;
  msg.header.group.source   = EkaSource::kCME_SBE;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0; // Stock index technically an underlying, but no id.
  msg.header.securityId     = rootBlock->SecurityID;
  msg.header.sequenceNumber = pktSeq;
  msg.header.timeStamp      = pktTime;
  msg.header.gapNum         = gapNum;

  msg.commonDef.securityType   = EfhSecurityType::kFuture;
  msg.commonDef.exchange       = EfhExchange::kCME;
  msg.commonDef.underlyingType = EfhSecurityType::kIndex;
  msg.commonDef.contractSize   = replaceIntNullWith<Decimal9NULL_T>(
      rootBlock->UnitOfMeasureQty, rootBlock->UnitOfMeasureQty / EFH_CME_PRICE_SCALE, 0);
  msg.commonDef.opaqueAttrA    = priceAdjustFactor;
  msg.commonDef.opaqueAttrB    = DEFAULT_FUT_DEPTH; // default Market Depth for Futures

  EkaFhCme *const fh = dynamic_cast<EkaFhCme*>(this->fh);
  {
    std::unique_lock lck(fh->futuresMutex);
    fh->futureStrikePriceFactors.insert(std::make_pair(rootBlock->SecurityID, strikePriceFactor));
  }

  copySymbol(msg.commonDef.underlying, rootBlock->Asset);
  copySymbol(msg.commonDef.classSymbol, rootBlock->SecurityGroup);
  copySymbol(msg.commonDef.exchSecurityName, rootBlock->Symbol);

  msg.displayFactor = static_cast<float>(rootBlock->DisplayFactor) / EFH_CME_PRICE_SCALE;
  msg.tickSize = static_cast<float>(rootBlock->MinPriceIncrement) / EFH_CME_PRICE_SCALE * msg.displayFactor;
  
  /* ------------------------------- */
  auto pGroupSize_EventType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_EventType);
  for (uint i = 0; i < pGroupSize_EventType->numInGroup; i++) {
    auto e {reinterpret_cast<const DefinitionEventEntry*>(m)};
    if (e->EventType == DefinitionEventType_T::LastTradeDate) {
      // Expiration time
      char expiryFmtBuf[32];
      if (!getExpiry(*e, msg.commonDef, expiryFmtBuf)) {
        EKA_TRACE("skipping future `%s` (%d) that expired before today (expiry = %s)",
                  msg.commonDef.exchSecurityName, rootBlock->SecurityID, expiryFmtBuf);
        return msgHdr->size;
      }
    }
    m += pGroupSize_EventType->blockLength;
  }
  /* ------------------------------- */
  /* auto pGroupSize_FeedType {reinterpret_cast<const groupSize_T*>(m)}; */
  /* m += sizeof(*pGroupSize_FeedType); */
  /* for (uint i = 0; i < pGroupSize_FeedType->numInGroup; i++) { */
  /*   auto e {reinterpret_cast<const DefinitionFeedTypeEntry*>(m)}; */
  /*   msg.commonDef.opaqueAttrB = e->MarketDepth; */
  /*   m += pGroupSize_FeedType->blockLength; */
  /* } */
  /* ------------------------------- */
  if (msg.commonDef.opaqueAttrB < 3)
    print_MDInstrumentDefinitionFuture54(pMsg);

  pEfhRunCtx->onEfhFutureDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
  
  return msgHdr->size;
}

/* ##################################################################### */     


int EkaFhCmeGr::process_MDInstrumentDefinitionOption55(const EfhRunCtx* pEfhRunCtx,
						       const uint8_t*   pMsg,
						       const uint64_t   pktTime,
						       const SequenceT  pktSeq) {
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);
  auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionOption55_mainBlock*>(m)};
  m += msgHdr->blockLen;

  if (strncmp(rootBlock->SecurityType, "OOF", sizeof(rootBlock->SecurityType)) != 0) {
    // Not an option-on-future, we don't care about this.
    EKA_WARN("found non-option-on-future security `%s` (CFI: '%.6s', SecurityType: '%.6s', UnderlyingProduct: %hhu)",
             rootBlock->Symbol, rootBlock->CFICode, rootBlock->SecurityType, rootBlock->UnderlyingProduct);
    return msgHdr->size;
  }

  //  auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

  const auto putOrCall {getEfhOptionType(rootBlock->PutOrCall)};
  const int64_t priceAdjustFactor = computeFinalPriceFactor(rootBlock->DisplayFactor);

  if (putOrCall == EfhOptionType::kErr) {
    //    hexDump("DefinitionOption55",msgHdr,msgHdr->size);	  
    EKA_WARN("Unexpected rootBlock->PutOrCall = 0x%x",(int)rootBlock->PutOrCall);
  }
  
  EfhOptionDefinitionMsg msg{};
  msg.header.msgType        = EfhMsgType::kOptionDefinition;
  msg.header.group.source   = EkaSource::kCME_SBE;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0; // Not given to us.
  msg.header.securityId     = rootBlock->SecurityID;
  msg.header.sequenceNumber = pktSeq;
  msg.header.timeStamp      = pktTime;
  msg.header.gapNum         = gapNum;

  msg.commonDef.securityType   = EfhSecurityType::kOption;
  msg.commonDef.exchange       = EfhExchange::kCME;
  msg.commonDef.underlyingType = EfhSecurityType::kFuture;
  msg.commonDef.contractSize   = replaceIntNullWith<Decimal9NULL_T>(
      rootBlock->UnitOfMeasureQty, rootBlock->UnitOfMeasureQty / EFH_CME_PRICE_SCALE, 0);
  msg.commonDef.opaqueAttrA    = priceAdjustFactor;
  copySymbol(msg.commonDef.exchSecurityName, rootBlock->Symbol);
  copySymbol(msg.commonDef.classSymbol, rootBlock->SecurityGroup);

  msg.optionType            = putOrCall;
  msg.optionStyle           = static_cast<EfhOptionStyle>(rootBlock->CFICode[2]);
  msg.segmentId             = rootBlock->MarketSegmentID;

  msg.commonDef.opaqueAttrB = DEFAULT_OPT_DEPTH; // default MarketDepth for Options

  /* ------------------------------- */
  auto pGroupSize_EventType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_EventType);
  for (uint i = 0; i < pGroupSize_EventType->numInGroup; i++) {
    auto e {reinterpret_cast<const DefinitionEventEntry*>(m)};
    if (e->EventType == DefinitionEventType_T::LastTradeDate) {
      // Expiration time
      char expiryFmtBuf[32];
      if (!getExpiry(*e, msg.commonDef, expiryFmtBuf)) {
        EKA_TRACE("skipping option `%s` (%d) that expired before today (expiry = %s)",
                  msg.commonDef.exchSecurityName, rootBlock->SecurityID, expiryFmtBuf);
        return msgHdr->size;
      }
    }
    m += pGroupSize_EventType->blockLength;
  }
  /* ------------------------------- */
  auto pGroupSize_FeedType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_FeedType);
  for (uint i = 0; i < pGroupSize_FeedType->numInGroup; i++) {
    //    auto e {reinterpret_cast<const DefinitionFeedTypeEntry*>(m)};
    // Leave the depth at its default value
    // msg.commonDef.opaqueAttrB = e->MarketDepth;
    m += pGroupSize_FeedType->blockLength;
  }
  /* ------------------------------- */
  auto pGroupSize_InstrAttribType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_InstrAttribType);
  for (uint i = 0; i < pGroupSize_InstrAttribType->numInGroup; i++)
    m += pGroupSize_InstrAttribType->blockLength;
  /* ------------------------------- */
  auto pGroupSize_LotType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_LotType);
  for (uint i = 0; i < pGroupSize_LotType->numInGroup; i++)
    m += pGroupSize_LotType->blockLength;
  /* ------------------------------- */
  auto pGroupSize_Underlyings {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_Underlyings);
  for (uint i = 0; i < pGroupSize_Underlyings->numInGroup; i++) {
    auto underlyingBlock{reinterpret_cast<const OptionDefinitionUnderlyingEntry*>(m)};
    msg.header.underlyingId = underlyingBlock->UnderlyingSecurityID;
    copySymbol(msg.commonDef.underlying, underlyingBlock->UnderlyingSymbol);
    m += pGroupSize_Underlyings->blockLength;
  }
  /* ------------------------------- */
  auto pGroupSize_RelatedInstruments {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_RelatedInstruments);
  for (uint i = 0; i < pGroupSize_RelatedInstruments->numInGroup; i++) {
    auto e {reinterpret_cast<const OptionDefinition55RelatedInstrumentEntry*>(m)};

    msg.header.securityId = e->SecurityID;
    copySymbol(msg.commonDef.underlying, e->Symbol);
    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

    m += pGroupSize_RelatedInstruments->blockLength;
  }

  int64_t strikePriceFactor = 0;
  EkaFhCme *const fh = dynamic_cast<EkaFhCme*>(this->fh);
  {
    std::shared_lock lck(fh->futuresMutex);
    const auto it = fh->futureStrikePriceFactors.find(msg.header.underlyingId);
    if (it != fh->futureStrikePriceFactors.end()) {
      strikePriceFactor = it->second;
    }
  }

  if (strikePriceFactor == 0) {
    EKA_WARN("no underlying (%d) found for option `%s` (%d); skipping",
             msg.header.underlyingId, msg.commonDef.exchSecurityName, rootBlock->SecurityID);
    return msgHdr->size;
  }

  msg.strikePrice = replaceIntNullWith<PRICENULL9_T>(
      rootBlock->StrikePrice, rootBlock->StrikePrice / strikePriceFactor, 0);

  if (msg.commonDef.opaqueAttrB < 3)
    print_MDInstrumentDefinitionOption55(pMsg);

  pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

  return msgHdr->size;
}

/* ##################################################################### */

int EkaFhCmeGr::process_MDInstrumentDefinitionSpread56(const EfhRunCtx* pEfhRunCtx,
                                                       const uint8_t*   pMsg,
                                                       const uint64_t   pktTime,
                                                       const SequenceT  pktSeq) {
  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);
  auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionSpread56_mainBlock*>(m)};
  m += msgHdr->blockLen;

  EfhSecurityType underlyingType;
  uint64_t defaultDepth;
  switch (rootBlock->CFICode[0]) {
  case 'O':
    underlyingType = EfhSecurityType::kOption;
    defaultDepth = DEFAULT_OPT_DEPTH;
    break;
  case 'F':
    underlyingType = EfhSecurityType::kFuture;
    defaultDepth = DEFAULT_FUT_DEPTH;
    break;
  default:
    EKA_WARN("found non-options-spread or -futures-spread `%d` (CFI: '%.6s', SecurityType: '%.6s', UnderlyingProduct: %hhu)",
             rootBlock->SecurityID, rootBlock->CFICode, rootBlock->SecurityType, rootBlock->UnderlyingProduct);
    return msgHdr->size;
  }
  
  /* ------------------------------- */
  
  const int64_t priceAdjustFactor = computeFinalPriceFactor(rootBlock->DisplayFactor);
  EfhComplexDefinitionMsg msg{};
  msg.header.msgType        = EfhMsgType::kComplexDefinition;
  msg.header.group.source   = EkaSource::kCME_SBE;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = rootBlock->UnderlyingProduct;
  msg.header.securityId     = rootBlock->SecurityID;
  msg.header.sequenceNumber = pktSeq;
  msg.header.timeStamp      = pktTime;
  msg.header.gapNum         = gapNum;

  msg.commonDef.securityType   = EfhSecurityType::kComplex;
  msg.commonDef.exchange       = EfhExchange::kCME;
  msg.commonDef.underlyingType = underlyingType;
  msg.commonDef.expiryDate     = 0;
  msg.commonDef.expiryTime     = 0;
  msg.commonDef.contractSize   = 0;
  msg.commonDef.opaqueAttrA    = priceAdjustFactor;
  msg.commonDef.opaqueAttrB    = defaultDepth; // Market Depth

  copySymbol(msg.commonDef.underlying, rootBlock->Asset);
  copySymbol(msg.commonDef.classSymbol, rootBlock->SecurityGroup);
  numToStrBuf(msg.commonDef.exchSecurityName, rootBlock->SecurityID);
  copySymbol(msg.commonDef.subtypeCode, rootBlock->SecuritySubType);
  // TODO: Add another field (exchSecurityDesc?) to publish Symbol for completeness
  // (make sure it's large enough to hold it; exchSecurityName isn't)
  //copySymbol(msg.commonDef.exchSecurityName, rootBlock->Symbol);

  /* ------------------------------- */
  auto pGroupSize_EventType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_EventType);
  for (uint i = 0; i < pGroupSize_EventType->numInGroup; i++) {
    auto e {reinterpret_cast<const DefinitionEventEntry*>(m)};
    if (e->EventType == DefinitionEventType_T::LastTradeDate) {
      // Expiration time
      char expiryFmtBuf[32];
      if (!getExpiry(*e, msg.commonDef, expiryFmtBuf)) {
        EKA_TRACE("skipping spread `%d` that expired before today (expiry = %s)",
                  rootBlock->SecurityID, expiryFmtBuf);
        return msgHdr->size;
      }
    }
    m += pGroupSize_EventType->blockLength;
  }
  /* ------------------------------- */		
  auto pGroupSize_FeedType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_FeedType);
  for (uint i = 0; i < pGroupSize_FeedType->numInGroup; i++) {
    //    auto e {reinterpret_cast<const DefinitionFeedTypeEntry*>(m)};
    // Leave the depth at its default value
    // msg.commonDef.opaqueAttrB = e->MarketDepth;
    m += pGroupSize_FeedType->blockLength;
  }
  /* ------------------------------- */
  auto pGroupSize_InstAttribType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_InstAttribType);
  for (uint i = 0; i < pGroupSize_InstAttribType->numInGroup; i++)
    m += pGroupSize_InstAttribType->blockLength;
  /* ------------------------------- */
  auto pGroupSize_LotTypeRules {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_LotTypeRules);
  for (uint i = 0; i < pGroupSize_LotTypeRules->numInGroup; i++)
    m += pGroupSize_LotTypeRules->blockLength;
  /* ------------------------------- */
  auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize);
  if (pGroupSize->numInGroup > EFH__MAX_COMPLEX_LEGS) {
    on_error("complex security `%s` contains %hhu legs, maximum number of %u",
             rootBlock->Symbol, pGroupSize->numInGroup, EFH__MAX_COMPLEX_LEGS);
  }
  for (uint i = 0; i < pGroupSize->numInGroup; i++) {
    auto e {reinterpret_cast<const MDInstrumentDefinitionSpread56_legEntry*>(m)};

    // We don't know the security type because we cannot look up the
    // security by ID here; the client will need to do it.
    msg.legs[i].securityId  = e->LegSecurityID;
    msg.legs[i].type        = EfhSecurityType::kInvalid;
    msg.legs[i].side        = e->LegSide == LegSide_T::BuySide ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
    msg.legs[i].ratio       = e->LegRatioQty;
    msg.legs[i].optionDelta = replaceIntNullWith<DecimalQty_T>(e->LegOptionDelta, 0);
    msg.legs[i].price       = replaceIntNullWith<PRICENULL9_T>(e->LegPrice,
                                                               e->LegPrice / priceAdjustFactor, 0);

    m += pGroupSize->blockLength;
  }
  msg.numLegs = pGroupSize->numInGroup;

  if (msg.commonDef.opaqueAttrB < 3)
    print_MDInstrumentDefinitionSpread56(pMsg);
  if (pEfhRunCtx->onEfhComplexDefinitionMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhComplexDefinitionMsgCb == NULL");
  pEfhRunCtx->onEfhComplexDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

  return msgHdr->size;
}

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <string>
#include <cmath>

#include "EkaFhCmeParser.h"

#include "EkaFhRunGroup.h"
#include "EkaFhCmeGr.h"

using namespace Cme;

std::string ts_ns2str(uint64_t ts);

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
    case MsgId::QuoteRequest39 :
      process_QuoteRequest39(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDIncrementalRefreshBook46 :
      process_MDIncrementalRefreshBook46(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::SnapshotFullRefresh52 : 
      process_SnapshotFullRefresh52(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionFuture54 :
      process_MDInstrumentDefinitionFuture54(pEfhRunCtx,p,pktTime,pktSeq);
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionOption55 :
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
    p += msgHdr->size;
    
    if (op == EkaFhMode::DEFINITIONS) {
      if (futuresDefinitionsState == DefinitionsCycleState::Done)
	return true;

      if (vanillaOptionsDefinitionsState == DefinitionsCycleState::Done &&
	  (complexOptionsDefinitionsState == DefinitionsCycleState::Done ||
	   complexOptionsDefinitionsState == DefinitionsCycleState::Init))
	return true;
    }
  }
  return false;
}

/* ##################################################################### */
  int EkaFhCmeGr::process_QuoteRequest39(const EfhRunCtx* pEfhRunCtx,
					 const uint8_t*   pMsg,
					 const uint64_t   pktTime,
					 const SequenceT  pktSeq) {
    auto m      {pMsg};
    auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
    m += sizeof(*msgHdr);
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
    msg.header.timeStamp      = pktTime; //rootBlock->LastUpdateTime;
    msg.header.gapNum         = gapNum;
  
    /* ------------------------------- */
    for (uint i = 0; i < pGroupSize->numInGroup; i++) {
      auto e {reinterpret_cast<const QuoteRequest39_legEntry*>(m)};

      msg.securityType      = EfhSecurityType::kRfq;
      msg.header.securityId = e->SecurityID;
      msg.side              = getSide39(e->Side);
      msg.quantity          = e->OrderQty;
      if (pEfhRunCtx->onEfhAuctionUpdateMsgCb == NULL)
	on_error("pEfhRunCtx->onEfhAuctionUpdateMsgCb == NULL");
      pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

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
  m += msgHdr->blockLen;
  /* ------------------------------- */
  auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize);
  /* ------------------------------- */
  for (uint i = 0; i < pGroupSize->numInGroup; i++) {
    auto e {reinterpret_cast<const IncrementalRefreshMdEntry*>(m)};
    m += pGroupSize->blockLength;
    auto s {book->findSecurity(e->SecurityID)};
    if (s == NULL) break;

    auto side {getSide46(e->MDEntryType)};
    if (side == SideT::OTHER) return msgHdr->size;
    bool tobChange = false;
    switch (e->MDUpdateAction) {
    case MDUpdateAction_T::New:
      tobChange = s->newPlevel(side,
			       e->MDPriceLevel,
			       (int64_t)std::round(e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE),
			       e->MDEntrySize);
      break;
    case MDUpdateAction_T::Change:
      tobChange = s->changePlevel(side,
				  e->MDPriceLevel,
				  (int64_t)std::round(e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE),
				  e->MDEntrySize);
      break;
    case MDUpdateAction_T::Delete:
      tobChange = s->deletePlevel(side,
				  e->MDPriceLevel);
      break;
    case MDUpdateAction_T::DeleteThru:
    case MDUpdateAction_T::DeleteFrom:
    case MDUpdateAction_T::Overlay:
      break;
    default:
      on_error("Unexpected MDUpdateActio %u",(uint)e->MDUpdateAction);
    }
    /* if (tobChange && fh->print_parsed_messages) { */
    /*   fprintf(parser_log,"generateOnQuote: BP=%ju,BS=%d,AP=%ju,AS=%d\n", */
    /* 	  s->bid->getEntryPrice(0), */
    /* 	  s->bid->getEntrySize(0), */
    /* 	  s->ask->getEntryPrice(0), */
    /* 	  s->ask->getEntrySize(0) */
    /* 	  ); */
    /* } */

    if (tobChange) book->generateOnQuote (pEfhRunCtx,
					  s,
					  pktSeq,
					  pktTime,
					  gapNum);
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
	    
	hdr->mdMsgType  = EfhMdType::NewPlevel;
	hdr->mdMsgSize  = sizeof(MdNewPlevel);
	hdr->securityId = e->SecurityID;

	dstMsg->pLvl    = e->MDPriceLevel;
	dstMsg->side    = side == SideT::BID ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
	dstMsg->price   = e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE;
	dstMsg->size    = e->MDEntrySize;
	    
	pEfhRunCtx->onEfhMdCb(hdr,pEfhRunCtx->efhRunUserData);
      }
	break;
      case MDUpdateAction_T::Change: {
	auto dstMsg {reinterpret_cast<MdChangePlevel*>(msgBuf)};
	    
	hdr->mdMsgType  = EfhMdType::ChangePlevel;
	hdr->mdMsgSize  = sizeof(MdChangePlevel);
	hdr->securityId = e->SecurityID;

	dstMsg->pLvl    = e->MDPriceLevel;
	dstMsg->side    = side == SideT::BID ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
	dstMsg->price   = e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE;
	dstMsg->size    = e->MDEntrySize;
	    
	pEfhRunCtx->onEfhMdCb(hdr,pEfhRunCtx->efhRunUserData);
      }

	break;
      case MDUpdateAction_T::Delete: {
	auto dstMsg {reinterpret_cast<MdDeletePlevel*>(msgBuf)};
	    
	hdr->mdMsgType  = EfhMdType::DeletePlevel;
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
	break;
      default:
	on_error("Unexpected MDUpdateAction %u",(uint)e->MDUpdateAction);
      }
    }	
  }
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
  m += msgHdr->blockLen;

  auto s {book->findSecurity(rootBlock->SecurityID)};
  if (s == NULL) return msgHdr->size;
  
  /* ------------------------------- */
  auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize);
  /* ------------------------------- */
  for (uint i = 0; i < pGroupSize->numInGroup; i++) {
    auto e {reinterpret_cast<const MDSnapshotFullRefreshMdEntry*>(m)};
    
    auto side {getSide52(e->MDEntryType)};
    if (side == SideT::OTHER) continue;

    bool tobChange = s->newPlevel(side,
				  e->MDPriceLevel,
				  e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE,
				  e->MDEntrySize);
    if (tobChange) book->generateOnQuote (pEfhRunCtx,
					  s,
					  pktSeq,
					  pktTime,
					  gapNum);
    m += pGroupSize->blockLength;    
  }
  return msgHdr->size;
}

/* ##################################################################### */     


int EkaFhCmeGr::process_MDInstrumentDefinitionFuture54(const EfhRunCtx* pEfhRunCtx,
						       const uint8_t*   pMsg,
						       const uint64_t   pktTime,
						       const SequenceT  pktSeq) {
  if (futuresDefinitionsState == DefinitionsCycleState::Done)
    on_error("futuresDefinitionsState == DefinitionsCycleState::Done");

  auto m      {pMsg};
  auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
  m += sizeof(*msgHdr);
  auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionFuture54_mainBlock*>(m)};
  m += msgHdr->blockLen;
  /* ------------------------------- */
  auto symbol           {std::string(rootBlock->Symbol,          sizeof(rootBlock->Symbol))};
  auto cfiCode          {std::string(rootBlock->CFICode,         sizeof(rootBlock->CFICode))};
  auto securityExchange {std::string(rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange))};
  auto asset            {std::string(rootBlock->Asset,           sizeof(rootBlock->Asset))};
  auto securityType     {std::string(rootBlock->SecurityType,    sizeof(rootBlock->SecurityType))};
  auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

  EfhOptionDefinitionMsg msg{};
  msg.header.msgType        = EfhMsgType::kOptionDefinition; // FUTURE DEFINITION!!!
  msg.header.group.source   = EkaSource::kCME_SBE;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = rootBlock->UnderlyingProduct;
  msg.header.securityId     = rootBlock->SecurityID;
  msg.header.sequenceNumber = pktSeq;
  msg.header.timeStamp      = pktTime; //rootBlock->LastUpdateTime;
  msg.header.gapNum         = gapNum;

  //    msg.secondaryGroup        = 0;
  msg.securityType          = EfhSecurityType::kFuture;
  //      msg.optionType            = putOrCall;
  msg.expiryDate            = pMaturity->year * 10000 + pMaturity->month * 100 + pMaturity->day;
  msg.contractSize          = 0;
  //      msg.strikePrice           = rootBlock->StrikePrice;
  msg.exchange              = EfhExchange::kCME;

  memcpy (&msg.underlying, rootBlock->Symbol,std::min(sizeof(msg.underlying), sizeof(rootBlock->Symbol)));
  memcpy (&msg.classSymbol,rootBlock->Symbol,std::min(sizeof(msg.classSymbol),sizeof(rootBlock->Symbol)));

  pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);


  /* ------------------------------- */
  futuresDefinitions++;
  futuresDefinitionsState = DefinitionsCycleState::InProgress;
  if (futuresDefinitions == (int)rootBlock->TotNumReports)
    futuresDefinitionsState = DefinitionsCycleState::Done;

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

  if (vanillaOptionsDefinitionsState == DefinitionsCycleState::Done)
    return msgHdr->size;

  /* ------------------------------- */
  auto symbol           {std::string(rootBlock->Symbol,	         sizeof(rootBlock->Symbol))};
  auto cfiCode          {std::string(rootBlock->CFICode,	 sizeof(rootBlock->CFICode))};
  auto securityExchange {std::string(rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange))};
  auto asset            {std::string(rootBlock->Asset,	         sizeof(rootBlock->Asset))};
  auto securityType     {std::string(rootBlock->SecurityType,    sizeof(rootBlock->SecurityType))};
  auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

  auto putOrCall {getEfhOptionType(rootBlock->PutOrCall)};

  if (putOrCall == EfhOptionType::kErr) {
    //    hexDump("DefinitionOption55",msgHdr,msgHdr->size);	  
    EKA_WARN("Unexpected rootBlock->PutOrCall = 0x%x",(int)rootBlock->PutOrCall);
  }
  
  EfhOptionDefinitionMsg msg{};
  msg.header.msgType        = EfhMsgType::kOptionDefinition;
  msg.header.group.source   = EkaSource::kCME_SBE;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = rootBlock->UnderlyingProduct;
  msg.header.securityId     = rootBlock->SecurityID;
  msg.header.sequenceNumber = pktSeq;
  msg.header.timeStamp      = pktTime; //rootBlock->LastUpdateTime;
  msg.header.gapNum         = gapNum;

  //    msg.secondaryGroup        = 0;
  msg.securityType          = EfhSecurityType::kOption;
  msg.optionType            = putOrCall;
  msg.expiryDate            = pMaturity->year * 10000 + pMaturity->month * 100 + pMaturity->day;
  msg.contractSize          = 0;
  //      msg.strikePrice           = rootBlock->StrikePrice / EFH_CME_STRIKE_PRICE_SCALE;
  msg.strikePrice           = rootBlock->StrikePrice / EFH_CME_ORDER_PRICE_SCALE / 1e9 * rootBlock->DisplayFactor;
  msg.exchange              = EfhExchange::kCME;

  memcpy (&msg.underlying, rootBlock->Asset,std::min(sizeof(msg.underlying), sizeof(rootBlock->Asset)));
  //      memcpy (&msg.classSymbol,rootBlock->Symbol,std::min(sizeof(msg.classSymbol),sizeof(rootBlock->Symbol)));
  for (size_t i = 0; i < sizeof(msg.classSymbol) && rootBlock->Symbol[i] != ' '; i++)
    msg.classSymbol[i] = rootBlock->Symbol[i];

  msg.opaqueAttrA           = rootBlock->DisplayFactor;
  //      msg.opaqueAttrB           = rootBlock->PriceDisplayFormat;
  pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

  /* ------------------------------- */
  auto pGroupSize_EventType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_EventType);
  for (uint i = 0; i < pGroupSize_EventType->numInGroup; i++)
    m += pGroupSize_EventType->blockLength;
  /* ------------------------------- */
  auto pGroupSize_FeedType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_FeedType);
  for (uint i = 0; i < pGroupSize_FeedType->numInGroup; i++)
    m += pGroupSize_FeedType->blockLength;
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
  for (uint i = 0; i < pGroupSize_Underlyings->numInGroup; i++)
    m += pGroupSize_Underlyings->blockLength;
  /* ------------------------------- */
  auto pGroupSize_RelatedInstruments {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_RelatedInstruments);
  for (uint i = 0; i < pGroupSize_RelatedInstruments->numInGroup; i++) {
    auto e {reinterpret_cast<const OptionDefinition55RelatedInstrumentEntry*>(m)};

    msg.header.securityId = e->SecurityID;
    memcpy (&msg.underlying, e->Symbol,std::min(sizeof(msg.underlying), sizeof(e->Symbol)));
    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

    m += pGroupSize_RelatedInstruments->blockLength;
  }
  /* ------------------------------- */
  vanillaOptionsDefinitions++;
  vanillaOptionsDefinitionsState = DefinitionsCycleState::InProgress;
  if (vanillaOptionsDefinitions == (int)rootBlock->TotNumReports)
    vanillaOptionsDefinitionsState = DefinitionsCycleState::Done;
  /* ------------------------------- */
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

  if (complexOptionsDefinitionsState == DefinitionsCycleState::Done)
    return msgHdr->size;

  /* ------------------------------- */
  
  EfhComplexDefinitionMsg msg{};
  msg.header.msgType        = EfhMsgType::kComplexDefinition;
  msg.header.group.source   = EkaSource::kCME_SBE;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = rootBlock->UnderlyingProduct;
  msg.header.securityId     = rootBlock->SecurityID;
  msg.header.sequenceNumber = pktSeq;
  msg.header.timeStamp      = pktTime; //rootBlock->LastUpdateTime;
  msg.header.gapNum         = gapNum;

  memcpy(msg.exchSymbolName,rootBlock->Symbol,
	 std::min(sizeof(msg.exchSymbolName),sizeof(rootBlock->Symbol)));
  memcpy(msg.exchAssetName,rootBlock->Asset,
	 std::min(sizeof(msg.exchAssetName),sizeof(rootBlock->Asset)));
  /* ------------------------------- */
  auto pGroupSize_EventType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_EventType);
  for (uint i = 0; i < pGroupSize_EventType->numInGroup; i++) 
    m += pGroupSize_EventType->blockLength;
  /* ------------------------------- */		
  auto pGroupSize_MDFeedType {reinterpret_cast<const groupSize_T*>(m)};
  m += sizeof(*pGroupSize_MDFeedType);
  for (uint i = 0; i < pGroupSize_MDFeedType->numInGroup; i++) 
    m += pGroupSize_MDFeedType->blockLength;
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
  for (uint i = 0; i < pGroupSize->numInGroup; i++) {
    auto e {reinterpret_cast<const MDInstrumentDefinitionSpread56_legEntry*>(m)};

    msg.legs[i].securityId = e->LegSecurityID;
    msg.legs[i].type       = EfhSecurityType::kComplex;
    msg.legs[i].side       = e->LegSide == LegSide_T::BuySide ? EfhOrderSide::kBid : EfhOrderSide::kAsk;
    msg.legs[i].ratio      = std::abs(e->LegRatioQty);
	  
    m += pGroupSize->blockLength;
  }
  msg.numLegs = pGroupSize->numInGroup;

  if (pEfhRunCtx->onEfhComplexDefinitionMsgCb == NULL)
    on_error("pEfhRunCtx->onEfhComplexDefinitionMsgCb == NULL");
  pEfhRunCtx->onEfhComplexDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

  /* ------------------------------- */
  complexOptionsDefinitions++;
  complexOptionsDefinitionsState = DefinitionsCycleState::InProgress;
  if (complexOptionsDefinitions == (int)rootBlock->TotNumReports)
    complexOptionsDefinitionsState = DefinitionsCycleState::Done;
  /* ------------------------------- */
  return msgHdr->size;
}

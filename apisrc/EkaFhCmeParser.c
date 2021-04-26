#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <string>

#include "EkaFhCmeParser.h"

#ifdef _PCAP_TEST_
#include "cmePcapParse.h"
#else
#include "EkaFhRunGroup.h"
#include "EkaFhCmeGr.h"
#endif


std::string ts_ns2str(uint64_t ts);
/* ##################################################################### */

bool EkaFhCmeGr::processPkt(const EfhRunCtx* pEfhRunCtx,
			    const uint8_t*   pkt, 
			    int16_t          pktLen,
			    EkaFhMode        op) {
  auto pktHdr {reinterpret_cast<const PktHdr*>(pkt)};

  uint64_t  pktTime = pktHdr->time;
  SequenceT pktSeq  = pktHdr->seq;

  if (fh->print_parsed_messages) 
    fprintf (parser_log,"pktTime=%ju,pktSeq=%u\n",pktTime,pktSeq);

  int pos = sizeof(*pktHdr);

  while (pos < pktLen) {
    int msgPos = pos;
    auto msgHdr {reinterpret_cast<const MsgHdr*>(&pkt[msgPos])};

    if (fh->print_parsed_messages) 
      fprintf (parser_log,"\tMsgId=%d,size=%u,blockLen=%u,schemaId=%u,version=%u\n",
	       (int)msgHdr->templateId,
	       msgHdr->size,
	       msgHdr->blockLen,
	       msgHdr->schemaId,
	       msgHdr->version);

    if (msgPos + msgHdr->size > pktLen) 
      on_error("msgPos %d + msgHdr->size %u > pktLen %u",
	       msgPos,msgHdr->size,pktLen);

    uint rootBlockPos = msgPos + sizeof(*msgHdr);

    switch (msgHdr->templateId) {
      /* ##################################################################### */
    case MsgId::MDIncrementalRefreshBook46 : {
      /* ------------------------------- */

      if (fh->print_parsed_messages) {
	auto rootBlock {reinterpret_cast<const MDIncrementalRefreshBook46_mainBlock*>(&pkt[rootBlockPos])};
	fprintf (parser_log,"\t\tIncrementalRefreshBook46: TransactTime=%jx, MatchEventIndicator=0x%x\n",
		  rootBlock->TransactTime,rootBlock->MatchEventIndicator);
      }
      
      /* ------------------------------- */
      uint groupSizePos = rootBlockPos + msgHdr->blockLen;
      auto pGroupSize {reinterpret_cast<const groupSize_T*>(&pkt[groupSizePos])};
      /* ------------------------------- */
      uint entryPos = groupSizePos + sizeof(*pGroupSize);
      for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	auto e {reinterpret_cast<const IncrementaRefreshMdEntry*>(&pkt[entryPos])};

	if (fh->print_parsed_messages) 
	  fprintf (parser_log,"\t\t\tsecId=%8d,%s,%s,plvl=%u,p=%16jd,s=%d\n",
		   e->SecurityID,
		   MDpdateAction2STR(e->MDUpdateAction),
		   MDEntryTypeBook2STR(e->MDEntryType),
		   e->MDPriceLevel,
		   e->MDEntryPx,
		   e->MDEntrySize);

	entryPos += pGroupSize->blockLength;
	FhSecurity* s = book->findSecurity(e->SecurityID);
	if (s == NULL) break;

	bool ignorMe = false;
	SideT  side  = SideT::UNINIT;
	switch (e->MDEntryType) {
	case MDEntryTypeBook_T::Bid :
	  side = SideT::BID;
	  break;

	case MDEntryTypeBook_T::Offer :
	  side = SideT::ASK;
	  break;

	case MDEntryTypeBook_T::ImpliedBid   :
	case MDEntryTypeBook_T::ImpliedOffer :
	  ignorMe = true;
	  break;

	case MDEntryTypeBook_T::BookReset :
	  /* TBD */
	  ignorMe = true;
	  break;
	default:
	  on_error("Unexpected MDEntryType \'%c\'",(char)e->MDEntryType);
	}

	if (ignorMe) break;
	bool tobChange = false;
	switch (e->MDUpdateAction) {
	case MDUpdateAction_T::New:
	  tobChange = s->newPlevel(side,
				   e->MDPriceLevel,
				   e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE,
				   e->MDEntrySize);
	  break;
	case MDUpdateAction_T::Change:
	  tobChange = s->changePlevel(side,
				      e->MDPriceLevel,
				      e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE,
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
	    dstMsg->side    = side == SideT::BID ? EfhOrderSideType::kBid : EfhOrderSideType::kAsk;
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
	    dstMsg->side    = side == SideT::BID ? EfhOrderSideType::kBid : EfhOrderSideType::kAsk;
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
	    dstMsg->side    = side == SideT::BID ? EfhOrderSideType::kBid : EfhOrderSideType::kAsk;
	    
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
      }
    }
      break;
      /* ##################################################################### */
    case MsgId::SnapshotFullRefresh52 : {
      processedSnapshotMessages++;
      /* ------------------------------- */
      auto rootBlock {reinterpret_cast<const SnapshotFullRefresh52_mainBlock*>(&pkt[rootBlockPos])};

      if (fh->print_parsed_messages) 
	fprintf (parser_log,"\t\tSnapshotFullRefresh52: secId=%8d,LastMsgSeqNumProcessed=%u,TotNumReports=%u,%s,%s\n",
		 rootBlock->SecurityID,
		 rootBlock->LastMsgSeqNumProcessed,
		 rootBlock->TotNumReports,
		 SecurityTradingStatus2STR(rootBlock->MDSecurityTradingStatus),
		 ts_ns2str(rootBlock->LastUpdateTime).c_str());

      /* ------------------------------- */
      uint groupSizePos = rootBlockPos + msgHdr->blockLen;
      const groupSize_T* pGroupSize = (const groupSize_T*)&pkt[groupSizePos];
      /* ------------------------------- */
      uint entryPos = groupSizePos + sizeof(*pGroupSize);
      for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	auto e {reinterpret_cast<const MDSnapshotFullRefreshMdEntry*>(&pkt[entryPos])};

	if (fh->print_parsed_messages) 
	  fprintf (parser_log,"\t\t\t%s,plvl=%u,p=%16jd,s=%d\n",
		   MDEntryType2STR(e->MDEntryType),
		   e->MDPriceLevel,
		   e->MDEntryPx,
		   e->MDEntrySize
		   );

      entryPos += pGroupSize->blockLength;
	FhSecurity* s = book->findSecurity(rootBlock->SecurityID);
	if (s == NULL) break;

	bool ignorMe = false;
	SideT  side  = SideT::UNINIT;
	switch (e->MDEntryType) {
	case MDEntryType_T::Bid :
	  side = SideT::BID;
	  break;

	case MDEntryType_T::Offer :
	  side = SideT::ASK;
	  break;

	case MDEntryType_T::Trade :
	case MDEntryType_T::OpenPrice :
	case MDEntryType_T::SettlementPrice :
	case MDEntryType_T::TradingSessionHighPrice :
	case MDEntryType_T::TradingSessionLowPrice :
	case MDEntryType_T::ClearedVolume :
	case MDEntryType_T::OpenInterest :
	case MDEntryType_T::ImpliedBid :
	case MDEntryType_T::ImpliedOffer :
	case MDEntryType_T::SessionHighBid :
	case MDEntryType_T::SessionLowOffer :
	case MDEntryType_T::FixingPrice :
	case MDEntryType_T::ElectronicVolume :
	case MDEntryType_T::ThresholdLimitsandPriceBandVariation :

	case MDEntryType_T::BookReset :
	  /* TBD */
	  ignorMe = true;
	  break;
	default:
	  on_error("Unexpected MDEntryType \'%c\'",(char)e->MDEntryType);
	} //switch MdEntryType
	if (ignorMe) break;
	bool tobChange = s->newPlevel(side,
				      e->MDPriceLevel,
				      e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE,
				      e->MDEntrySize);
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
	if (processedSnapshotMessages >= (int)rootBlock->TotNumReports) return true;

      } // for MdEntries
    } // case MsgId::SnapshotFullRefresh52
      break;


      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionFuture54 : {
      processedDefinitionMessages++;
      /* ------------------------------- */
      auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionFuture54_mainBlock*>(&pkt[rootBlockPos])};
      std::string symbol           = std::string(rootBlock->Symbol,          sizeof(rootBlock->Symbol));
      std::string cfiCode          = std::string(rootBlock->CFICode,         sizeof(rootBlock->CFICode));
      std::string securityExchange = std::string(rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange));
      std::string asset            = std::string(rootBlock->Asset,           sizeof(rootBlock->Asset));
      std::string securityType     = std::string(rootBlock->SecurityType,    sizeof(rootBlock->SecurityType));
      auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

      if (fh->print_parsed_messages) 
	fprintf (parser_log,"\t\tDefinitionFuture54: \'%s\',\'%s\',\'%s\',\'%s\',%d,\'%s\',%04u-%02u-%02u--%02u\n",
		 securityExchange.c_str(),
		 asset.c_str(),
		 symbol.c_str(),
		 securityType.c_str(),
		 rootBlock->SecurityID,
		 cfiCode.c_str(),
		 pMaturity->year,pMaturity->month,pMaturity->day,pMaturity->week
		 );

      EfhDefinitionMsg msg = {};
      msg.header.msgType        = EfhMsgType::kDefinition;
      msg.header.group.source   = EkaSource::kCME_SBE;
      msg.header.group.localId  = id;
      msg.header.underlyingId   = rootBlock->UnderlyingProduct;
      msg.header.securityId     = rootBlock->SecurityID;
      msg.header.sequenceNumber = pktSeq;
      msg.header.timeStamp      = pktTime; //rootBlock->LastUpdateTime;
      msg.header.gapNum         = gapNum;

      //    msg.secondaryGroup        = 0;
      msg.securityType          = EfhSecurityType::kFut;
      //      msg.optionType            = putOrCall;
      msg.expiryDate            = pMaturity->year * 10000 + pMaturity->month * 100 + pMaturity->day;
      msg.contractSize          = 0;
      //      msg.strikePrice           = rootBlock->StrikePrice;
      msg.exchange              = EfhExchange::kCME;

      memcpy (&msg.underlying, rootBlock->Symbol,std::min(sizeof(msg.underlying), sizeof(rootBlock->Symbol)));
      memcpy (&msg.classSymbol,rootBlock->Symbol,std::min(sizeof(msg.classSymbol),sizeof(rootBlock->Symbol)));

      pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

      if (processedDefinitionMessages >= (int)rootBlock->TotNumReports) return true;
    }
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionOption55 : {
      processedDefinitionMessages++;
      /* ------------------------------- */
      auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionOption55_mainBlock*>(&pkt[rootBlockPos])};
      std::string symbol           = std::string((const char*)&rootBlock->Symbol,          sizeof(rootBlock->Symbol));
      std::string cfiCode          = std::string((const char*)&rootBlock->CFICode,         sizeof(rootBlock->CFICode));
      std::string securityExchange = std::string((const char*)&rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange));
      std::string asset            = std::string((const char*)&rootBlock->Asset,           sizeof(rootBlock->Asset));
      std::string securityType     = std::string((const char*)&rootBlock->SecurityType,    sizeof(rootBlock->SecurityType));
      auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

      EfhOptionType putOrCall;
      switch (rootBlock->PutOrCall) {
      case PutOrCall_T::Put :
	putOrCall = EfhOptionType::kPut;
	break;
      case PutOrCall_T::Call :
	putOrCall = EfhOptionType::kCall;
	break;
      default:
	on_error("unexpected PutOrCall %d",(int)rootBlock->PutOrCall);
      }

      if (fh->print_parsed_messages) 
	fprintf(parser_log,"\t\tDefinitionOption55: \'%s\',\'%s\',\'%s\',%s,\'%s\',%d,\'%s\',%04u-%02u-%02u--%02u, %ju\n",
		securityExchange.c_str(),
		asset.c_str(),
		symbol.c_str(),
		putOrCall == EfhOptionType::kPut ? "PUT" : "CALL",
		securityType.c_str(),
		rootBlock->SecurityID,
		cfiCode.c_str(),
		pMaturity->year,pMaturity->month,pMaturity->day,pMaturity->week,
		rootBlock->StrikePrice
		);

      EfhDefinitionMsg msg = {};
      msg.header.msgType        = EfhMsgType::kDefinition;
      msg.header.group.source   = EkaSource::kCME_SBE;
      msg.header.group.localId  = id;
      msg.header.underlyingId   = rootBlock->UnderlyingProduct;
      msg.header.securityId     = rootBlock->SecurityID;
      msg.header.sequenceNumber = pktSeq;
      msg.header.timeStamp      = pktTime; //rootBlock->LastUpdateTime;
      msg.header.gapNum         = gapNum;

      //    msg.secondaryGroup        = 0;
      msg.securityType          = EfhSecurityType::kOpt;
      msg.optionType            = putOrCall;
      msg.expiryDate            = pMaturity->year * 10000 + pMaturity->month * 100 + pMaturity->day;
      msg.contractSize          = 0;
      msg.strikePrice           = rootBlock->StrikePrice / EFH_CME_STRIKE_PRICE_SCALE;
      msg.exchange              = EfhExchange::kCME;

      memcpy (&msg.underlying, rootBlock->Asset,std::min(sizeof(msg.underlying), sizeof(rootBlock->Asset)));
      //      memcpy (&msg.classSymbol,rootBlock->Symbol,std::min(sizeof(msg.classSymbol),sizeof(rootBlock->Symbol)));
      for (size_t i = 0; i < sizeof(msg.classSymbol) && rootBlock->Symbol[i] != ' '; i++)
	msg.classSymbol[i] = rootBlock->Symbol[i];

      msg.opaqueAttrA           = rootBlock->DisplayFactor;
      msg.opaqueAttrB           = rootBlock->PriceDisplayFormat;
      
      pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);

      if (processedDefinitionMessages >= (int)rootBlock->TotNumReports) return true;
    }
      break;

      /* ##################################################################### */
    default:
#ifdef _PRINT_ALL_
      //     TEST_LOG ("\t\tMsgId=%u",msgHdr->templateId);

#endif

      break;
    }
    pos += msgHdr->size;

    //    TEST_LOG("pos=%d, pktLen=%d",pos,pktLen);
    if (pktLen - pos == 4) pos += 4; // for FCS from eka_tcpdump
  }
  return false;
}

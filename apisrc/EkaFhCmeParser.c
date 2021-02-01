#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>

#include "EkaFhRunGroup.h"
#include "EkaFhCmeGr.h"

#include "EkaFhCmeParser.h"

std::string ts_ns2str(uint64_t ts);
/* ##################################################################### */

bool EkaFhCmeGr::processPkt(const EfhRunCtx* pEfhRunCtx,
			    const uint8_t*   pkt, 
			    int16_t          pktLen,
			    EkaFhMode        op) {
  const PktHdr* pktHdr = (const PktHdr*)pkt;

  uint64_t  pktTime = pktHdr->time;
  SequenceT pktSeq  = pktHdr->seq;

  int pos = sizeof(*pktHdr);

  while (pos < pktLen) {
    int msgPos = pos;
    const MsgHdr* msgHdr = (const MsgHdr*)&pkt[msgPos];

#ifdef _PRINT_ALL_
    EKA_LOG ("\tMsgId=%d,size=%u,blockLen=%u,schemaId=%u,version=%u",
	     (int)msgHdr->templateId,
	     msgHdr->size,
	     msgHdr->blockLen,
	     msgHdr->schemaId,
	     msgHdr->version);
#endif

    if (msgPos + msgHdr->size > pktLen) 
      on_error("msgPos %d + msgHdr->size %u > pktLen %u",
	       msgPos,msgHdr->size,pktLen);

    switch (msgHdr->templateId) {
      /* ##################################################################### */
    case MsgId::MDIncrementalRefreshBook46 : {
      /* ------------------------------- */
      uint rootBlockPos = msgPos + sizeof(MsgHdr);
      const MDIncrementalRefreshBook46_mainBlock* rootBlock = (const MDIncrementalRefreshBook46_mainBlock*)&pkt[rootBlockPos];
#ifdef _PRINT_ALL_
      EKA_LOG ("\t\tIncrementalRefreshBook46: TransactTime=%jx, MatchEventIndicator=0x%x",
	       rootBlock->TransactTime,rootBlock->MatchEventIndicator);
#endif
      /* ------------------------------- */
      uint groupSizePos = rootBlockPos + msgHdr->blockLen;
      const groupSize_T* pGroupSize = (const groupSize_T*)&pkt[groupSizePos];
      /* ------------------------------- */
      uint entryPos = groupSizePos + sizeof(*pGroupSize);
      for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	const IncementaRefreshMdEntry* e = (const IncementaRefreshMdEntry*) &pkt[entryPos];
	PriceT            price        = e->MDEntryPx;
	SizeT             size         = e->MDEntrySize;
	SecurityIdT       securityId   = e->SecurityID;
	SequenceT         prodSequence = e->RptSeq;
	PriceLevetT       priceLevel   = e->MDPriceLevel;
	MDUpdateAction_T  action       = e->MDUpdateAction;
	MDEntryTypeBook_T entryType    = e->MDEntryType;

#ifdef _PRINT_ALL_
	EKA_LOG("\t\t\tsecId=%8d,%s,%s,plvl=%u,p=%16jd,s=%d\n",
		securityId,
		MDpdateAction2STR(action),
		MDEntryTypeBook2STR(entryType),
		priceLevel,
		price,
		size);
#endif
	entryPos += pGroupSize->blockLength;
      }
    }
      break;
      /* ##################################################################### */
    case MsgId::SnapshotFullRefresh52 : {
      /* ------------------------------- */
      uint rootBlockPos = msgPos + sizeof(MsgHdr);
      const SnapshotFullRefresh52_mainBlock* rootBlock = (const SnapshotFullRefresh52_mainBlock*)&pkt[rootBlockPos];
      SequenceT               lastMsgSeqNumProcessed = rootBlock->LastMsgSeqNumProcessed;
      uInt32_T                totNumReports          = rootBlock->TotNumReports;
      SecurityIdT             securityID             = rootBlock->SecurityID;
      SequenceT               prodSequence           = rootBlock->RptSeq;
      uInt64_T                transactTime           = rootBlock->TransactTime;
      uInt64_T                lastUpdateTime         = rootBlock->LastUpdateTime;
      SecurityTradingStatus_T tradingStatus          = rootBlock->MDSecurityTradingStatus;
#ifdef _PRINT_ALL_
      EKA_LOG ("\t\tSnapshotFullRefresh52: securityID=%d, lastMsgSeqNumProcessed=%u,totNumReports=%u,tradingStatus=%s,lastUpdateTime=%s",
	       securityID,
	       lastMsgSeqNumProcessed,
	       totNumReports,
	       SecurityTradingStatus2STR(tradingStatus),
	       ts_ns2str(lastUpdateTime).c_str());
#endif
      /* ------------------------------- */
      uint groupSizePos = rootBlockPos + msgHdr->blockLen;
      const groupSize_T* pGroupSize = (const groupSize_T*)&pkt[groupSizePos];
      /* ------------------------------- */
      uint entryPos = groupSizePos + sizeof(*pGroupSize);
      for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	const IncementaRefreshMdEntry* e = (const IncementaRefreshMdEntry*) &pkt[entryPos];
	PriceT            price        = e->MDEntryPx;
	SizeT             size         = e->MDEntrySize;
	SecurityIdT       securityId   = e->SecurityID;
	SequenceT         prodSequence = e->RptSeq;
	PriceLevetT       priceLevel   = e->MDPriceLevel;
	MDUpdateAction_T  action       = e->MDUpdateAction;
	MDEntryTypeBook_T entryType    = e->MDEntryType;

#ifdef _PRINT_ALL_
	EKA_LOG("\t\t\tsecId=%8d,%s,%s,plvl=%u,p=%16jd,s=%d\n",
		securityId,
		MDpdateAction2STR(action),
		MDEntryTypeBook2STR(entryType),
		priceLevel,
		price,
		size
		);
#endif
	entryPos += pGroupSize->blockLength;
      }
    }
      break;


      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionFuture54 : {
      ++processedDefinitionMessages;
      /* ------------------------------- */
      uint rootBlockPos = msgPos + sizeof(MsgHdr);
      const MDInstrumentDefinitionFuture54_mainBlock* rootBlock = (const MDInstrumentDefinitionFuture54_mainBlock*)&pkt[rootBlockPos];
      SecurityIdT securityId       = rootBlock->SecurityID;
      int32_t     totNumReports    = rootBlock->TotNumReports;
      std::string symbol           = std::string((const char*)&rootBlock->Symbol,          sizeof(rootBlock->Symbol));
      std::string cfiCode          = std::string((const char*)&rootBlock->CFICode,         sizeof(rootBlock->CFICode));
      std::string securityExchange = std::string((const char*)&rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange));
      std::string asset            = std::string((const char*)&rootBlock->Asset,           sizeof(rootBlock->Asset));
      std::string securityType     = std::string((const char*)&rootBlock->SecurityType,    sizeof(rootBlock->SecurityType));

      const MaturityMonthYear_T* pMaturity = (MaturityMonthYear_T*)&rootBlock->MaturityMonthYear;
#ifdef _PRINT_ALL_
      EKA_LOG ("\t\tDefinitionFuture54: report %d of %d,\'%s\',\'%s\',\'%s\',\'%s\',%d,\'%s\',%04u-%02u-%02u--%02u",
	       processedDefinitionMessages,totNumReports,
	       securityExchange.c_str(),
	       asset.c_str(),
	       symbol.c_str(),
	       securityType.c_str(),
	       securityId,
	       cfiCode.c_str(),
	       pMaturity->year,pMaturity->month,pMaturity->day,pMaturity->week
	       );

#endif
      if (processedDefinitionMessages >= totNumReports) return true;
    }
      break;
      /* ##################################################################### */
    case MsgId::MDInstrumentDefinitionOption55 : {
      ++processedDefinitionMessages;
      /* ------------------------------- */
      uint rootBlockPos = msgPos + sizeof(MsgHdr);
      const MDInstrumentDefinitionOption55_mainBlock* rootBlock = (const MDInstrumentDefinitionOption55_mainBlock*)&pkt[rootBlockPos];
      SecurityIdT securityId       = rootBlock->SecurityID;
      int32_t     totNumReports    = rootBlock->TotNumReports;
      std::string symbol           = std::string((const char*)&rootBlock->Symbol,          sizeof(rootBlock->Symbol));
      std::string cfiCode          = std::string((const char*)&rootBlock->CFICode,         sizeof(rootBlock->CFICode));
      std::string securityExchange = std::string((const char*)&rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange));
      std::string asset            = std::string((const char*)&rootBlock->Asset,           sizeof(rootBlock->Asset));
      std::string securityType     = std::string((const char*)&rootBlock->SecurityType,    sizeof(rootBlock->SecurityType));

      const MaturityMonthYear_T* pMaturity = (MaturityMonthYear_T*)&rootBlock->MaturityMonthYear;
      uint8_t     putOrCall        = (uint8_t)rootBlock->PutOrCall;
#ifdef _PRINT_ALL_
      EKA_LOG ("\t\tDefinitionOption55: report %d of %d,\'%s\',\'%s\',\'%s\',%s (0x%x),\'%s\',%d,\'%s\',%04u-%02u-%02u--%02u",
	       processedDefinitionMessages,totNumReports,
	       securityExchange.c_str(),
	       asset.c_str(),
	       symbol.c_str(),
	       putOrCall == PutOrCall_T::Put ? "PUT" : putOrCall == PutOrCall_T::Call ? "CALL" : "UNEXPECTED",putOrCall,
	       securityType.c_str(),
	       securityId,
	       cfiCode.c_str(),
	       pMaturity->year,pMaturity->month,pMaturity->day,pMaturity->week
	       );

#endif
      if (processedDefinitionMessages >= totNumReports) return true;
    }
      break;

      /* ##################################################################### */
    default:
#ifdef _PRINT_ALL_
      //     EKA_LOG ("\t\tMsgId=%u",msgHdr->templateId);

#endif

      break;
    }
  }
  return false;
}

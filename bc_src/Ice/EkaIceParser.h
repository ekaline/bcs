#ifndef _EKA_ICE_H
#define _EKA_ICE_H

#include "EkaIce.h"
#include "EkaParser.h"
#include "EkaIceBook.h"
#include "EkaStrat.h"

//####################################################


class EkaIceParser : public EkaParser {
 public:
  using ProductId      = EkaIce::ProductId;
  using ExchSecurityId = EkaIce::ExchSecurityId;
  using Price          = EkaIce::Price;
  using Size           = EkaIce::Size;

  using PriceLevelIdx  = EkaIce::PriceLevelIdx;

  using OrderId        = EkaIce::OrderId;

 EkaIceParser(EkaExch* exch, EkaStrat* strat) : EkaParser(exch, strat) {}
  //--------------------------------------------------------

  inline uint processPkt(MdOut* mdOut, 
			 ProcessAction processAction, 
			 uint8_t* pkt, 
			 uint pktPayloadSize) {
    if (pkt == NULL) on_error("pkt == NULL");

    uint8_t* p = pkt;
#ifdef _EKA_PARSER_PRINT_ALL_
    printf("pktPayloadSize=%u: ",pktPayloadSize);
#endif
    p += processPktHdr(mdOut,p,processAction);

    for (uint i = 0; i < mdOut->msgNum; i++) {
      p += processMsg(mdOut,p,processAction);
    }
    return pktPayloadSize;
  }
  //--------------------------------------------------------

  inline uint processPktHdr(MdOut* mdOut, uint8_t* pktStart, ProcessAction processAction) {
    mdOut->sessNum      = be16toh(((PktHdrLayout*)pktStart)->sessNum);
    mdOut->pktSeq       = be32toh(((PktHdrLayout*)pktStart)->seqNum);
    mdOut->msgNum       = be16toh(((PktHdrLayout*)pktStart)->msgNum);
    mdOut->transactTime = be64toh(((PktHdrLayout*)pktStart)->pktTime);

#ifdef _EKA_PARSER_PRINT_ALL_
    printf("sessNum=%u,pktSeq=%u,msgNum=%u,transactTime=%ju\n",mdOut->sessNum,mdOut->pktSeq,mdOut->msgNum,mdOut->transactTime);
#endif

    return sizeof(PktHdrLayout);
  }
  
  //--------------------------------------------------------

  inline uint  processMsg(MdOut* mdOut, uint8_t* msg, ProcessAction processAction) {
    MsgId      msgId    = static_cast<MsgId>(((MsgHdrLayout*) msg)->msgType);
    uint16_t   msgSize  = be16toh(((MsgHdrLayout*) msg)->msgSize);
    uint8_t*   msgStart = msg + sizeof(MsgHdrLayout);

    switch (msgId) {
      /* ----------------------------------------------------------- */
    case MsgId::AddModifyOrder : {
      uint32_t securityId = be32toh(((AddModifyLayout*) msgStart)->marketId);
      SIDE     side       = ((AddModifyLayout*) msgStart)->side == '1' ? BID : ASK;
      uint64_t orderId    = be64toh(((AddModifyLayout*) msgStart)->orderId);
      uint64_t price      = be64toh(((AddModifyLayout*) msgStart)->price);
      uint32_t size       = be32toh(((AddModifyLayout*) msgStart)->size);
      bool     isRFQ      = ((AddModifyLayout*) msgStart)->isRFQ == 'Y';
      bool     isAddOrder = (((AddModifyLayout*) msgStart)->extraFlags & 0x01) != 0;

#ifdef _EKA_PARSER_PRINT_ALL_
      printf("\tAddModifyOrder: %u,%c,%ju (0x%jx),%ju,%u,isRFQ:%c\n",
	     securityId,
	     side == SIDE::BID ? 'B' : 'A',
	     orderId,
	     orderId,
	     price,
	     size,
	     isRFQ
	     );	  
#endif
      EkaIceBook* b = reinterpret_cast<EkaIceBook*>(strat->findBook(securityId));
      if (b == NULL) break;
      if (unlikely(processAction == ProcessAction::PrintOnly)) break;
      if (isRFQ) break;
      SIDE s = side;
      b->addModifyOrder((OrderId)orderId,
			(SIDE)   side,
			(Price)  price,
			(Size)   size,
			isAddOrder);

      if (unlikely(processAction == ProcessAction::UpdateBookAndCheckIntegrity)) {
	if (! b->checkIntegrity()) {
	  EKA_LOG("Book Corrupted after: AddModifyOrder: %u,%c,%ju (0x%jx),%ju,%u,isRFQ:%c",
		  (ExchSecurityId) securityId,
		  (SIDE)           side == SIDE::BID ? 'B' : 'A',
		  (OrderId)        orderId,
		  (OrderId)        orderId,
		  (Price)          price,
		  (Size)           size,
		  isRFQ ? 'Y' : 'N'
		  );
	  b->printBook();
	  on_error("book corrupted");
	}
      }
      exch->onTobChange(mdOut,b,s);
    }
      break;
      /* ----------------------------------------------------------- */
    case MsgId::DeleteOrder : {
      uint32_t securityId = be32toh(((DeleteOrderLayout*) msgStart)->marketId);
      uint64_t orderId    = be64toh(((DeleteOrderLayout*) msgStart)->orderId);

#ifdef _EKA_PARSER_PRINT_ALL_
      printf("\tDeleteOrder: %u,%ju (0x%jx),\n",
	     securityId,
	     orderId,
	     orderId
	     );	  
#endif
      EkaIceBook* b = reinterpret_cast<EkaIceBook*>(strat->findBook(securityId));
      if (b == NULL) break;
      if (unlikely(processAction == ProcessAction::PrintOnly)) break;

      SIDE s = b->deleteOrder(static_cast<OrderId>(orderId));
      if (unlikely(processAction == ProcessAction::UpdateBookAndCheckIntegrity)) {
	if (! b->checkIntegrity()) {
	  EKA_LOG("Book Corrupted after: DeleteOrder: %u,%ju (0x%jx)",
		  securityId,
		  orderId,
		  orderId
		  );
	  b->printBook();
	  on_error("book corrupted");
	}
      }
      exch->onTobChange(mdOut,b,s);
    }
      break;
      /* ----------------------------------------------------------- */
    case MsgId::Trade : {
      uint32_t securityId = be32toh(((TradeLayout*) msgStart)->marketId);
      uint64_t orderId    = be64toh(((TradeLayout*) msgStart)->tradeId);

#ifdef _EKA_PARSER_PRINT_ALL_
      SIDE     side       = ((TradeLayout*) msgStart)->aggressorSide == '1' ? BID : ((TradeLayout*) msgStart)->aggressorSide == '2' ? ASK : IRRELEVANT;
      uint64_t price      = be64toh(((TradeLayout*) msgStart)->price);
      uint32_t size       = be32toh(((TradeLayout*) msgStart)->quantity);

      printf("\tTrade: secid %u, price %ju, size %u, side %c\n",
	     securityId,
	     price,
	     size,
	     side
	     );	  
#endif
      EkaIceBook* b = reinterpret_cast<EkaIceBook*>(strat->findBook(securityId));
      if (b == NULL) break;
      if (unlikely(processAction == ProcessAction::PrintOnly)) break;

      SIDE s = b->deleteOrder((OrderId)orderId);
      if (unlikely(processAction == ProcessAction::UpdateBookAndCheckIntegrity)) {
	if (! b->checkIntegrity()) {
	  //	  EKA_LOG("Book Corrupted after: Trade: %u,%ju",
	  //		  Trade::getSecurityId(msgBodyStart),
	  //		  Trade::getOrderId   (msgBodyStart)
	  //		  );
	  b->printBook();
	  on_error("book corrupted");
	}
      }
      exch->onTobChange(mdOut,b,s);
    }
      break;
      /* ----------------------------------------------------------- */
    default:
#ifdef _EKA_PARSER_PRINT_ALL_
      printf("\t\'%c\':\n", (char)msgId);
#endif
      break;
    }
    return sizeof(MsgHdrLayout) + msgSize;
  }

  //####################################################

  uint createAddOrder(uint8_t* dst,
		      uint64_t secId,
		      SIDE     s,
		      uint     priceLevel,
		      uint64_t basePrice, 
		      uint64_t priceMultiplier, 
		      uint64_t priceStep, 
		      uint32_t size, 
		      uint64_t sequence, 
		      uint64_t ts) {

    uint8_t* p = (uint8_t*) dst;
    uint16_t currSize = 0;
    ((PktHdrLayout*)p)->seqNum  = be32toh(static_cast<uint32_t>(sequence));
    ((PktHdrLayout*)p)->pktTime = be64toh(static_cast<uint64_t>(ts));
    ((PktHdrLayout*)p)->sessNum = be16toh(static_cast<uint16_t>(0));
    ((PktHdrLayout*)p)->msgNum  = be16toh(static_cast<uint16_t>(1));

    p        += sizeof(PktHdrLayout);
    currSize += sizeof(PktHdrLayout);

    ((MsgHdrLayout*)p)->msgType  = (uint8_t)MsgId::AddModifyOrder;
    ((MsgHdrLayout*)p)->msgSize  = be16toh(sizeof(MsgHdrLayout) + sizeof(AddModifyLayout));

    p        += sizeof(MsgHdrLayout);
    currSize += sizeof(MsgHdrLayout);

    uint64_t price = s == BID ? 
      (basePrice - priceLevel * priceStep) * priceMultiplier : 
      (basePrice + priceLevel * priceStep) * priceMultiplier ;

    ((AddModifyLayout*)p)->marketId   = be32toh(secId);
    ((AddModifyLayout*)p)->side       = s == BID ? '1' : '2';
    ((AddModifyLayout*)p)->price      = be64toh(price);
    ((AddModifyLayout*)p)->size       = be32toh(size);
    ((AddModifyLayout*)p)->extraFlags = 0x0;
    ((AddModifyLayout*)p)->orderId    = be64toh(sequence); //just unique number

    //    uint64_t orderId;
    //    uint16_t orderSequenceID;
    //    char     isImplied;
    //    char     isRFQ;
    //    uint64_t orderEntryDateTime;
    //    uint32_t sequenceWithinMillis;
    //    uint64_t modificationTimeStamp;
    //    uint64_t requestTradingEngineReceivedTimestamp;

    p        += sizeof(AddModifyLayout);
    currSize += sizeof(AddModifyLayout);

    return currSize;
  }

  uint createTrade(uint8_t* dst,
		   uint64_t secId,
		   SIDE     s,
		   uint64_t basePrice, 
		   uint64_t priceMultiplier, 
		   uint32_t size, 
		   uint64_t sequence,
		   uint64_t ts) {

    uint8_t* p = (uint8_t*) dst;
    uint16_t currSize = 0;
    ((PktHdrLayout*)p)->seqNum  = be32toh(static_cast<uint32_t>(sequence));
    ((PktHdrLayout*)p)->pktTime = be64toh(static_cast<uint64_t>(ts));
    ((PktHdrLayout*)p)->sessNum = be16toh(static_cast<uint16_t>(0));
    ((PktHdrLayout*)p)->msgNum  = be16toh(static_cast<uint16_t>(1));

    p        += sizeof(PktHdrLayout);
    currSize += sizeof(PktHdrLayout);

    ((MsgHdrLayout*)p)->msgType  = (uint8_t)MsgId::Trade;
    ((MsgHdrLayout*)p)->msgSize  = be16toh(sizeof(MsgHdrLayout) + sizeof(TradeLayout));

    p        += sizeof(MsgHdrLayout);
    currSize += sizeof(MsgHdrLayout);

    ((TradeLayout*)p)->marketId      = be32toh(secId);
    ((TradeLayout*)p)->price         = be64toh(basePrice * priceMultiplier);
    ((TradeLayout*)p)->quantity      = be32toh(size);
    ((TradeLayout*)p)->aggressorSide = s == BID ? '1' : '2';

    /* uint64_t tradeId; */
    /* char     isSystemPricedLeg; */
    /* char     oldOffMarketTradeType; */
    /* uint64_t transactDateTime; */
    /* char     systemPricedLegType; */
    /* char     isImpliedSpreadAtMarketOpen; */
    /* char     isAdjustedTrade; */
    /* uint8_t  extraFlags; */
    /* char     OffMarketTradeType[3]; */
    /* uint32_t sequenceWithinMillis; */
    /* uint64_t requestTradingEngineReceivedTimestamp; */

    p        += sizeof(TradeLayout);
    currSize += sizeof(TradeLayout);

    return currSize;
  }

 private:
  enum class MsgId : uint8_t {
    AddModifyOrder                = 'E', 
      DeleteOrder                 = 'F', 
      Trade                       = 'G',
      };

  struct PktHdrLayout {
    uint16_t sessNum;
    uint32_t seqNum;
    uint16_t msgNum;
    uint64_t pktTime;
  } __attribute__((packed));

  struct MsgHdrLayout {
    uint8_t  msgType;
    uint16_t msgSize;
    //	uint8_t  pad;
  } __attribute__((packed));

  struct AddModifyLayout {
    uint32_t marketId; // = securityID
    uint64_t orderId;
    uint16_t orderSequenceID;
    char     side; // '1' = Bid, '2' = Ask
    uint64_t price;
    uint32_t size; // = Quantity
    char     isImplied;
    char     isRFQ;
    uint64_t orderEntryDateTime;
    uint8_t  extraFlags;
    uint32_t sequenceWithinMillis;
    uint64_t modificationTimeStamp;
    uint64_t requestTradingEngineReceivedTimestamp;
  } __attribute__((packed));

  struct DeleteOrderLayout {
    uint32_t marketId; // = securityID
    uint64_t orderId;
    uint64_t orderDeleteDateTime;
    uint32_t sequenceWithinMillis;
    uint64_t requestTradingEngineReceivedTimestamp;
  } __attribute__((packed));

 struct TradeLayout {
    uint32_t marketId; // = securityID
    uint64_t tradeId;
    char     isSystemPricedLeg;
    uint64_t price;
    uint32_t quantity;
    char     oldOffMarketTradeType;
    uint64_t transactDateTime;
    char     systemPricedLegType;
    char     isImpliedSpreadAtMarketOpen;
    char     isAdjustedTrade;
    char     aggressorSide; // ‘ ‘  No Aggressor ‘1’  Buy ‘2’  Sell
    uint8_t  extraFlags;
    char     OffMarketTradeType[3];
    uint32_t sequenceWithinMillis;
    uint64_t requestTradingEngineReceivedTimestamp;
  } __attribute__((packed));


};


#endif

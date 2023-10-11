#ifndef _EKA_EOBI_PARSER_H_
#define _EKA_EOBI_PARSER_H_

#include "EkaEobi.h"
#include "EkaParser.h"
#include "EOBILayouts.h"

class EkaEobiParser : public EkaParser {
 public:
  using GlobalExchSecurityId = EkaExch::GlobalExchSecurityId;

  using ProductId      = EkaEobi::ProductId;
  using ExchSecurityId = EkaEobi::ExchSecurityId;
  using Price          = EkaEobi::Price;
  using Size           = EkaEobi::Size;
  using NormPrice      = EkaEobi::NormPrice;

 EkaEobiParser(EkaExch* exch, EkaStrat* strat) : EkaParser(exch,strat) {}
  //--------------------------------------------------------


  uint processPkt(MdOut* mdOut, 
		  ProcessAction processAction, 
		  uint8_t* pkt, 
		  uint pktPayloadSize);

  //--------------------------------------------------------
  uint printPkt(uint8_t* pkt,uint pktPayloadSize);
  uint printMsg(uint8_t* msgStart);
  //--------------------------------------------------------


 private:
  typedef enum {NEW = 0, CHANGE = 1, DELETE = 2, UNDEFINED = 3} ACTION;

  //--------------------------------------------------------
  inline uint processMsg(MdOut* mdOut, ProcessAction processAction, uint8_t* msgStart);

  //--------------------------------------------------------

#define entryTypeDecode(x)				\
  x == '0' ? "Bid" :					\
    x == '1' ? "Offer" :				\
    x == '2' ? "Trade" :				\
    x == '4' ? "OpenPrice" :				\
    x == '6' ? "SettlementPrice" :			\
    x == '7' ? "TradingSessionHighPrice" :		\
    x == '8' ? "TradingSessionLowPrice" :		\
    x == 'B' ? "ClearedVolume" :			\
    x == 'C' ? "OpenInterest" :				\
    x == 'E' ? "ImpliedBid" :				\
    x == 'F' ? "ImpliedOffer" :				\
    x == 'J' ? "BookReset" :				\
    x == 'N' ? "SessionHighBid" :			\
    x == 'O' ? "SessionLowOffer" :			\
    x == 'W' ? "FixingPrice" :				\
    x == 'e' ? "ElectronicVolume" :			\
    x == 'g' ? "ThresholdLimitsandPriceBandVariation":	\
    "UNKNOWN"

#define updateAction2str(x)			\
  x == 0 ? "New" :				\
    x == 1 ? "Change" :				\
    x == 2 ? "Delete" :				\
    x == 3 ? "DeleteThru" :			\
    x == 4 ? "DeleteFrom" :			\
    x == 5 ? "Overlay" :			\
    "UNKNOWN"


  //###################################################
  inline ExchSecurityId getSecurityId(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    return static_cast<ExchSecurityId>(p->SecurityId);
  }
  //###################################################
  inline Price getPrice(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    return static_cast<Price>(p->price);
  }
  //###################################################
  inline Size getSize(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    return static_cast<Size>(p->size);
  }
  //###################################################
  inline uint32_t getSequence(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    return p->seq;
  }
  //###################################################

  inline  ACTION getAction(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    switch (p->MDUpdateAction) {
    case 0: return ACTION::NEW;
    case 1: return ACTION::CHANGE;
    case 2: return ACTION::DELETE;
    default: on_error("Unexpected ACTION %u",p->MDUpdateAction);
    }
    return ACTION::UNDEFINED;
  }

  //###################################################
  inline SIDE getSide(uint8_t* msgPtr) {
    if (((MdEntryLayout*) msgPtr)->MDEntryType == '0') return SIDE::BID;
    if (((MdEntryLayout*) msgPtr)->MDEntryType == '1') return SIDE::ASK;
    return SIDE::IRRELEVANT;
  }

  struct MdEntryLayout { // 32 
    uint64_t price;                 // 8
    uint32_t size;                  // 4
    uint32_t SecurityId;            // 4
    uint32_t seq;                   // 4
    uint32_t numOrders;             // 4
    uint8_t  priceLevel;            // 1
    uint8_t  MDUpdateAction;        // 1
    char     MDEntryType;           // 1
    uint8_t  pad[5];                // 5
  } __attribute__((packed));

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
    ((PacketHeaderT*)p)->MessageHeader.BodyLen    = sizeof(PacketHeaderT);
    ((PacketHeaderT*)p)->MessageHeader.TemplateID = TID_PACKETHEADER;
    ((PacketHeaderT*)p)->ApplSeqNum               = static_cast<uint32_t>(sequence);
    ((PacketHeaderT*)p)->TransactTime             = static_cast<uint64_t>(ts);

    p += sizeof(PacketHeaderT);
    currSize += sizeof(PacketHeaderT);

    uint64_t price = s == BID ? 
      (basePrice - priceLevel * priceStep) * priceMultiplier : 
      (basePrice + priceLevel * priceStep) * priceMultiplier ;

    OrderAddT* msg = (OrderAddT*) p;
    msg->MessageHeader.BodyLen      = sizeof(OrderAddT);
    msg->MessageHeader.TemplateID   = TID_ORDERADD;

    msg->SecurityID   = secId;
    msg->OrderDetails.DisplayQty = size;
    msg->OrderDetails.Side       = s == BID ? 1 : 2;
    msg->OrderDetails.Price      = price;
    
    currSize += sizeof(OrderAddT);

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
    ((PacketHeaderT*)p)->MessageHeader.BodyLen    = sizeof(PacketHeaderT);
    ((PacketHeaderT*)p)->MessageHeader.TemplateID = TID_PACKETHEADER;
    ((PacketHeaderT*)p)->ApplSeqNum               = static_cast<uint32_t>(sequence);
    ((PacketHeaderT*)p)->TransactTime             = static_cast<uint64_t>(ts);

    p += sizeof(PacketHeaderT);
    currSize += sizeof(PacketHeaderT);

    uint64_t price = basePrice * priceMultiplier;

    ExecutionSummaryT* msg = (ExecutionSummaryT*) p;
    msg->MessageHeader.BodyLen      = sizeof(ExecutionSummaryT);
    msg->MessageHeader.TemplateID   = TID_EXECUTIONSUMMARY;

    msg->SecurityID     = secId;
    msg->RequestTime    = static_cast<uint64_t>(ts);
    msg->LastQty        = size;
    msg->AggressorSide  = s == BID ? 1 : 2;
    msg->LastPx         = price;

    currSize += sizeof(ExecutionSummaryT);

    return currSize;
  }

};
//--------------------------------------------------------

#endif

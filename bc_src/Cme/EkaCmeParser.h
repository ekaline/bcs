#ifndef _EKA_CME_PARSER_H_
#define _EKA_CME_PARSER_H_

#include "EkaCme.h"
#include "EkaParser.h"

class EkaCmeParser : public EkaParser {
 public:
  using GlobalExchSecurityId = EkaExch::GlobalExchSecurityId;

  using ProductId      = EkaCme::ProductId;
  using ExchSecurityId = EkaCme::ExchSecurityId;
  using Price          = EkaCme::Price;
  using Size           = EkaCme::Size;
  using NormPrice      = EkaCme::NormPrice;

  using PriceLevelIdx  = EkaCme::PriceLevelIdx;

  
 EkaCmeParser(EkaExch* _exch, EkaStrat* _strat) : EkaParser(_exch,_strat) {}
  //--------------------------------------------------------

  uint processPkt(MdOut* mdOut, 
		  ProcessAction processAction, 
		  uint8_t* pkt, 
		  uint pktPayloadSize);

  //--------------------------------------------------------

  // private:
  typedef enum {NEW = 0, CHANGE = 1, DELETE = 2, UNDEFINED = 3} ACTION;

  //--------------------------------------------------------

  struct PktHdrLayout {
    uint32_t seq;
    uint64_t time;
  } __attribute__((packed));
  
  inline uint processPktHdr(MdOut* mdOut, uint8_t* pktStart);

  //--------------------------------------------------------

  enum class MsgId : uint16_t {
    MDIncrementalRefreshBook                = 46, 
      MDIncrementalRefreshOrderBook         = 47, 
      MDIncrementalRefreshVolume            = 37, 
      MDIncrementalRefreshTradeSummary      = 48, 
      MDIncrementalRefreshSessionStatistics = 51,
      Unknown                               = 101
      };
  //--------------------------------------------------------
  inline uint processMsg(MdOut* mdOut, ProcessAction processAction, uint8_t* msgStart);

  inline uint processMDIncrementalRefreshBook46(MdOut* mdOut, 
						ProcessAction processAction, 
						uint8_t* msgStart, 
						uint16_t rootBlockLen);

  inline uint processMDIncrementalRefreshTradeSummary48(MdOut* mdOut, 
						ProcessAction processAction, 
						uint8_t* msgStart, 
						uint16_t rootBlockLen);

  //--------------------------------------------------------

  struct GroupSize {
    uint16_t blockLen;
    uint8_t  numInGroup;
  } __attribute__((packed));

  inline uint  getMsgSize(uint8_t* msgStart) {
    return ((MsgHdrLayout*)msgStart)->size;
  }
  inline uint  getBlockLen(uint8_t* msgStart) {
    return ((MsgHdrLayout*)msgStart)->blockLen;
  }
  inline MsgId getMsgId(uint8_t* msgStart) {
    return static_cast<MsgId>(((MsgHdrLayout*)msgStart)->templateId);
  }

  struct MsgHdrLayout {
    uint16_t       size;
    uint16_t       blockLen;
    MsgId          templateId;
    uint16_t       schemaId;
    uint16_t       version;
  } __attribute__((packed));


  /* ############################################### */
#define printTemplateName(x)						\
  x == MsgId::MDIncrementalRefreshBook  ? "MDIncrementalRefreshBook46" : \
    x == MsgId::MDIncrementalRefreshOrderBook  ? "MDIncrementalRefreshOrderBook47" : \
    x == MsgId::MDIncrementalRefreshVolume  ? "MDIncrementalRefreshVolume37" : \
    x == MsgId::MDIncrementalRefreshTradeSummary  ? "MDIncrementalRefreshTradeSummary48" : \
    x == MsgId::MDIncrementalRefreshSessionStatistics  ? "MDIncrementalRefreshSessionStatistics51" : \
    "UNKNOWN"

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


  struct RefreshBookRoot { // 11
    uint64_t    TransactTime;       // 8
    uint8_t     MatchEventIndicator;// 1
    uint8_t     pad[2];             // 2
  } __attribute__((packed));

  //####################################################

  void printFieldsMDIncrementalRefreshBook46(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    printf ("\n\tMDIncrementalRefreshBook46: price=%ju,size=%u,SecurityId=%u,%u,%u,priceLevel=%u,%s,%s\n",
	    p->price,
	    p->size,
	    p->SecurityId,
	    p->seq,
	    p->numOrders,
	    p->priceLevel,
	    updateAction2str(p->MDUpdateAction),
	    entryTypeDecode(p->MDEntryType)
	    );
  }

  //###################################################
  inline ExchSecurityId getTickerSecurityId(uint8_t* msgPtr) {
    TickerEntryLayout* p = (TickerEntryLayout*) msgPtr;
    return static_cast<ExchSecurityId>(p->SecurityId);
  }
  //###################################################
  inline ExchSecurityId getSecurityId(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    return static_cast<ExchSecurityId>(p->SecurityId);
  }
  //###################################################
  inline Price getTickerPrice(uint8_t* msgPtr) {
    TickerEntryLayout* p = (TickerEntryLayout*) msgPtr;
    return static_cast<Price>(p->price);
  }
  //###################################################
  inline Price getPrice(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    return static_cast<Price>(p->price);
  }
  //###################################################
  inline Size getTickerSize(uint8_t* msgPtr) {
    TickerEntryLayout* p = (TickerEntryLayout*) msgPtr;
    return static_cast<Size>(p->size);
  }
  //###################################################
  inline Size getSize(uint8_t* msgPtr) {
    MdEntryLayout* p = (MdEntryLayout*) msgPtr;
    return static_cast<Size>(p->size);
  }
  //###################################################
  inline uint32_t getTickerSequence(uint8_t* msgPtr) {
    TickerEntryLayout* p = (TickerEntryLayout*) msgPtr;
    return p->seq;
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
  inline PriceLevelIdx getPriceLevel(uint8_t* msgPtr) {
    return static_cast<PriceLevelIdx>(((MdEntryLayout*) msgPtr)->priceLevel);
  }

  //###################################################
  inline SIDE getSide(uint8_t* msgPtr) {
    if (((MdEntryLayout*) msgPtr)->MDEntryType == '0') return SIDE::BID;
    if (((MdEntryLayout*) msgPtr)->MDEntryType == '1') return SIDE::ASK;
    return SIDE::IRRELEVANT;
  }
  //###################################################
  inline SIDE getTickerSide(uint8_t* msgPtr) {
    if (((TickerEntryLayout*) msgPtr)->AggressorSide == 1) return SIDE::BID;
    if (((TickerEntryLayout*) msgPtr)->AggressorSide == 2) return SIDE::ASK;
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

struct TickerEntryLayout { // 32 
  uint64_t    price;                 // 8
  uint32_t    size;                  // 4
  uint32_t    SecurityId;            // 4
  uint32_t    seq;                   // 4
  uint32_t    numOrders;             // 4
  uint8_t     AggressorSide;         // 1
  uint8_t     MDUpdateAction;        // 1
  char        MDEntryType;           // 1
  uint32_t    MDTradeEntryID;        // 4
  uint8_t     pad;                   // 1
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
    ((PktHdrLayout*)p)->seq  = static_cast<uint32_t>(sequence);
    ((PktHdrLayout*)p)->time = static_cast<uint64_t>(ts);

    p += sizeof(PktHdrLayout);

    MsgHdrLayout* msgHdr = (MsgHdrLayout*) p;

    msgHdr->size       = 0; // place holder, replaced at the end
    msgHdr->blockLen   = sizeof(RefreshBookRoot); // = 11
    msgHdr->templateId = MsgId::MDIncrementalRefreshBook;
    msgHdr->schemaId   = 0xdead;
    msgHdr->version    = 0xbeaf;
    
    currSize += sizeof(MsgHdrLayout);
    p        += sizeof(MsgHdrLayout);
  
    RefreshBookRoot* rootBlock = reinterpret_cast<RefreshBookRoot*>(p);
    rootBlock->TransactTime = ts;
    rootBlock->MatchEventIndicator = 0x04;
  
    currSize += sizeof(RefreshBookRoot);
    p        += sizeof(RefreshBookRoot);

    GroupSize* groupSize = reinterpret_cast<GroupSize*>(p);
    groupSize->blockLen   = sizeof(MdEntryLayout);

    groupSize->numInGroup = 1;

    currSize += sizeof(GroupSize);
    p        += sizeof(GroupSize);

    uint64_t price = s == BID ? 
      (basePrice - priceLevel * priceStep) * priceMultiplier : 
      (basePrice + priceLevel * priceStep) * priceMultiplier ;


    MdEntryLayout* mdEntry = reinterpret_cast<MdEntryLayout*>(p);
    mdEntry->price          = price;
    mdEntry->size           = size;
    mdEntry->SecurityId     = secId;
    mdEntry->seq            = sequence; 
    mdEntry->numOrders      = size;     
    mdEntry->priceLevel     = ++priceLevel; // cme tob: pricelevel==1
    mdEntry->MDUpdateAction = 0;          // = NEW
    mdEntry->MDEntryType    = s == BID ? '0' : '1';

    currSize += sizeof(MdEntryLayout);
    p        += sizeof(MdEntryLayout);

    msgHdr->size = currSize;

    return sizeof(PktHdrLayout) + currSize;
  }


  uint createTrade(uint8_t*    dst,
		   uint64_t secId,
		   SIDE     s,
		   uint64_t basePrice, 
		   uint64_t priceMultiplier, 
		   uint32_t size, 
		   uint64_t sequence,
		   uint64_t ts) {

    uint8_t* p = (uint8_t*) dst;
    uint16_t currSize = 0;

    ((PktHdrLayout*)p)->seq  = static_cast<uint32_t>(sequence);
    ((PktHdrLayout*)p)->time = static_cast<uint64_t>(ts);

    p += sizeof(PktHdrLayout);

    MsgHdrLayout* msgHdr = reinterpret_cast<MsgHdrLayout*>(p);
    msgHdr->size       = 0; // place holder, replaced at the end
    msgHdr->blockLen   = sizeof(RefreshBookRoot); // = 11
    msgHdr->templateId = MsgId::MDIncrementalRefreshTradeSummary;
    msgHdr->schemaId   = 0xdead;
    msgHdr->version    = 9;

    currSize += sizeof(MsgHdrLayout);
    p        += sizeof(MsgHdrLayout);
  
    RefreshBookRoot* rootBlock = reinterpret_cast<RefreshBookRoot*>(p);
    rootBlock->TransactTime = ts;
    rootBlock->MatchEventIndicator = 0x80;
  
    currSize += sizeof(RefreshBookRoot);
    p        += sizeof(RefreshBookRoot);

    GroupSize* groupSize = reinterpret_cast<GroupSize*>(p);
    groupSize->blockLen   = sizeof(TickerEntryLayout);
    groupSize->numInGroup = 1;

    currSize += sizeof(GroupSize);
    p        += sizeof(GroupSize);

    TickerEntryLayout* tickerEntry = reinterpret_cast<TickerEntryLayout*>(p);
    tickerEntry->price          = basePrice * priceMultiplier;
    tickerEntry->size           = size;
    tickerEntry->SecurityId     = secId;
    tickerEntry->seq            = 0x11223344; // just a number
    tickerEntry->numOrders      = size;       // just a number
    tickerEntry->AggressorSide  = s == BID ? 1 : 2;

    currSize += sizeof(TickerEntryLayout);
    p        += sizeof(TickerEntryLayout);

    msgHdr->size = currSize;
    //  EKA_LOG ("sending ticker msgHdr->blockLen = %d groupSize->blockLen = %d",msgHdr->blockLen,groupSize->blockLen);

    return sizeof(PktHdrLayout) + currSize;
  }

};
//--------------------------------------------------------


#endif

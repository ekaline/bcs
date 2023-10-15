#ifndef _EKA_NYSE_PARSER_H_
#define _EKA_NYSE_PARSER_H_

#include "string"

#include "EkaNyse.h"
#include "EkaParser.h"


enum class XDP_MSG_TYPE : uint16_t {
  TimeReference = 2,
    AddOrder = 100,
    ModifyOrder    = 101,
    DeleteOrder    = 102,
    ExecutionOrder = 103
    };


inline std::string msgType2str(uint16_t msgType) {
  switch ((XDP_MSG_TYPE)msgType) {
  case XDP_MSG_TYPE::TimeReference :
    return std::string("TimeReference");
  case XDP_MSG_TYPE::AddOrder :
    return std::string("AddOrder");
  case XDP_MSG_TYPE::ModifyOrder :
    return std::string("ModifyOrder");
  case XDP_MSG_TYPE::DeleteOrder :
    return std::string("DeleteOrder");
  case XDP_MSG_TYPE::ExecutionOrder :
    return std::string("ExecutionOrder");
  default:
    return std::to_string(msgType);
  }
}

class EkaNyseParser : public EkaParser {
 public:
  using GlobalExchSecurityId = EkaExch::GlobalExchSecurityId;

  using ProductId      = EkaNyse::ProductId;
  using ExchSecurityId = EkaNyse::ExchSecurityId;
  using Price          = EkaNyse::Price;
  using Size           = EkaNyse::Size;
  using NormPrice      = EkaNyse::NormPrice;

  using PriceLevelIdx  = EkaNyse::PriceLevelIdx;
  using OrderID        = EkaNyse::OrderID;

  
 EkaNyseParser(EkaExch* _exch, EkaStrat* _strat) : EkaParser(_exch,_strat) {}
  //--------------------------------------------------------
  enum class XDP_DELIVERY_FLAG : uint8_t {
    Heartbeat = 1,
      Failover = 10,// (see XDP Publisher Failover)
      OriginalMessage = 11, 
      SequenceNumberReset = 12,
      OnlyOnePacketInRetransmission = 13,
      PartOfRetransmission = 15,
      OnlyOnePacketInRefresh = 17,
      StartOfRefresh = 18,
      PartOfRefresh = 19,
      EndOfRefresh = 20,
      MessageUnavailable = 21
      };

  struct XdpTimeHdr {
    uint32_t    SourceTime;        // 4 4 Binary The time when this data was generated in the order book, in seconds since Jan 1, 1970 00:00:00 UTC. 
    uint32_t    SourceTimeNS;      // 8 4 Binary The nanosecond offset from the SourceTime. 
  } __attribute__((packed));

  struct XdpPktHdr {
    uint16_t             PktSize;           // 0 2 Binary Size of the message
    uint8_t              DeliveryFlag;      // 2 1 Binary Type of message
    uint8_t              NumberMsgs;        // 3 1
    uint32_t             SeqNum;            // 4 4
    XdpTimeHdr           time;              // 8 8
  } __attribute__((packed));

struct XdpMsgHdr {
  uint16_t      MsgSize;           // 0 2 Binary Size of the message
  uint16_t      MsgType;           // 2 2 Binary Type of message
} __attribute__((packed));

  struct XdpAddOrder { // 100
    uint16_t    MsgSize;             // 0  2 Binary Size of the message: 31 Bytes 
    uint16_t    MsgType;             // 2  2 Binary This field identifies the type of message ‘100’ – Add Order Message 
    uint32_t    SourceTimeNS;        // 4  4 Binary This field represents the nanosecond offset from the time reference second in UTC time (EPOCH) 
    uint32_t    SymbolIndex;         // 8  4 Binary This field identifies the numerical representation of the symbol. 
    uint32_t    SymbolSeqNum;        // 12 4 Binary This field contains the symbol sequence number 
    uint32_t    OrderID;             // 16 4 Binary The Order ID identifies a unique order. 
    uint32_t    Price;               // 20 4 Binary This field contains the price point. Use the Price scale from the symbol-mapping index. 
    uint32_t    Volume;              // 24 4 Binary This field contains the order quantity in shares. 
    char        Side;                // 28 1 ASCII This field indicates the side of the order Buy/Sell. Valid values: ■ ‘B’ – Buy ■ ‘S’ – Sell 
    uint8_t     OrderIDGTCIndicator; // 29 1 Binary This field specifies if Trade Order ID is a GTC order 0 – Day Order 1 – GTC Order 
    uint8_t     TradeSession;        // 30 1 Binary Valid values: 
                                     //       1 – Ok for morning hours 
                                     //       2 – Ok for national hours (core) 
                                     //       3 – OK for morning and core 
                                     //       4 – Ok for late hours 
                                     //       6 – OK for core and late 
                                     //       7 – OK for morning, core, and late
  } __attribute__((packed));

  struct XdpModifyOrder { // 101
    uint16_t    MsgSize;             // 0  2 Binary Size of the message: 31 Bytes 
    uint16_t    MsgType;             // 2  2 Binary This field identifies the type of message ‘100’ – Add Order Message 
    uint32_t    SourceTimeNS;        // 4  4 Binary This field represents the nanosecond offset from the time reference second in UTC time (EPOCH) 
    uint32_t    SymbolIndex;         // 8  4 Binary This field identifies the numerical representation of the symbol. 
    uint32_t    SymbolSeqNum;        // 12 4 Binary This field contains the symbol sequence number 
    uint32_t    OrderID;             // 16 4 Binary The Order ID identifies a unique order. 
    uint32_t    Price;               // 20 4 Binary This field contains the price point. Use the Price scale from the symbol-mapping index. 
    uint32_t    Volume;              // 24 4 Binary This field contains the order quantity in shares. 
    char        Side;                // 28 1 ASCII This field indicates the side of the order Buy/Sell. Valid values: ■ ‘B’ – Buy ■ ‘S’ – Sell 
    uint8_t     OrderIDGTCIndicator; // 29 1 Binary This field specifies if Trade Order ID is a GTC order 0 – Day Order 1 – GTC Order 
    uint8_t     ReasonCode;          // 30 1 Binary Modify Reason: 
                                     //      ■ 5 – Change (lost position in book) 
                                     //      ■ 6 – Routed (keep position in book) 
                                     //      ■ 7 – Modify Fill keep position Note: This field may a future enhancement. 
                                     //      The default value is currently 0, and should be disregarded.
  } __attribute__((packed));

  struct XdpDeleteOrder { // 102
    uint16_t    MsgSize;             // 0  2 Binary Size of the message: 23 Bytes 
    uint16_t    MsgType;             // 2  2 Binary This field identifies the type of message ‘100’ – Add Order Message 
    uint32_t    SourceTimeNS;        // 4  4 Binary This field represents the nanosecond offset from the time reference second in UTC time (EPOCH) 
    uint32_t    SymbolIndex;         // 8  4 Binary This field identifies the numerical representation of the symbol. 
    uint32_t    SymbolSeqNum;        // 12 4 Binary This field contains the symbol sequence number 
    uint32_t    OrderID;             // 16 4 Binary The Order ID identifies a unique order. 
    char        Side;                // 20 1 ASCII This field indicates the side of the order Buy/Sell. Valid values: ■ ‘B’ – Buy ■ ‘S’ – Sell 
    uint8_t     OrderIDGTCIndicator; // 21 1 Binary This field specifies if Trade Order ID is a GTC order 0 – Day Order 1 – GTC Order 
    uint8_t     ReasonCode;          // 23 1 Binary Delete Reason: 
                                     //      ■ 1 – User Cancel 
                                     //      ■ 2 – Modify (taken off book, Order ID may add again) 
                                     //      ■ 3 – Delete Filled
                                     //      The default value is currently 0, and should be disregarded.
  } __attribute__((packed));


  struct XdpExecutionOrder { // 103
    uint16_t    MsgSize;             // 0  2 Binary Size of the message: 34 Bytes 
    uint16_t    MsgType;             // 2  2 Binary This field identifies the type of message ‘100’ – Add Order Message 
    uint32_t    SourceTimeNS;        // 4  4 Binary This field represents the nanosecond offset from the time reference second in UTC time (EPOCH) 
    uint32_t    SymbolIndex;         // 8  4 Binary This field identifies the numerical representation of the symbol. 
    uint32_t    SymbolSeqNum;        // 12 4 Binary This field contains the symbol sequence number 
    uint32_t    OrderID;             // 16 4 Binary The Order ID identifies a unique order. 
    uint32_t    Price;               // 20 4 Binary This field contains the price point. Use the Price scale from the symbol-mapping index. 
    uint32_t    Volume;              // 24 4 Binary This field contains the order quantity in shares. 
    uint8_t     OrderIDGTCIndicator; // 28 1 Binary This field specifies if Trade Order ID is a GTC order 0 – Day Order 1 – GTC Order 
    uint8_t     ReasonCode;          // 29 1 Binary Modify Reason: 
                                     //      ■ 0 – Default
                                     //      ■ 3 – Filled
                                     //      ■ 7 – Partial Fill (Did not lose position)
                                     //      The default value is currently 0, and should be disregarded.
    uint32_t    TradeID;             // 30 4 Binary The TradeID identifies a unique transaction in the matching engine and allows clients to correlate execution reports to the last sale.
  } __attribute__((packed));

  //--------------------------------------------------------
  inline SIDE decodeSide(char _side) {
    if (_side == 'B') return SIDE::BID;
    if (_side == 'S') return SIDE::ASK;
    on_error("unexpected side = 0x%x",_side);
  }
  //--------------------------------------------------------

  uint processPkt(MdOut* mdOut, 
		  ProcessAction processAction, // not used
		  uint8_t* pkt, 
		  uint pktPayloadSize);

  //};
//--------------------------------------------------------

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
    ((XdpPktHdr*)p)->PktSize      = sizeof(XdpPktHdr) + sizeof(XdpAddOrder) ;
    ((XdpPktHdr*)p)->DeliveryFlag = 11;
    ((XdpPktHdr*)p)->NumberMsgs   = 1;
    ((XdpPktHdr*)p)->SeqNum       = static_cast<uint32_t>(sequence);
    ((XdpPktHdr*)p)->time.SourceTime   = 0; //TBD(static_cast<uint64_t>(ts));
    ((XdpPktHdr*)p)->time.SourceTimeNS   = 0; //TBD(static_cast<uint64_t>(ts));

    p        += sizeof(XdpPktHdr);
    currSize += sizeof(XdpPktHdr);


    uint64_t price = s == BID ? 
      (basePrice - priceLevel * priceStep) * priceMultiplier : 
      (basePrice + priceLevel * priceStep) * priceMultiplier ;
    uint32_t orderid = priceLevel*(uint)s+priceLevel; //Must be unique
    //  EKA_LOG("\nprice=%ju,basePrice=%ju,priceLevel=%ju,priceStep=%ju,orderid=%u\n",price,basePrice,priceLevel,priceStep,orderid);
    ((XdpAddOrder*)p)->MsgSize    = sizeof(XdpAddOrder) ;
    ((XdpAddOrder*)p)->MsgType    = (uint16_t)XDP_MSG_TYPE::AddOrder ;

    ((XdpAddOrder*)p)->SourceTimeNS    = 0; //TBD
    ((XdpAddOrder*)p)->SymbolIndex    = secId ;
    ((XdpAddOrder*)p)->SymbolSeqNum    =  sequence;
    ((XdpAddOrder*)p)->OrderID   =  orderid; 
    ((XdpAddOrder*)p)->Price    =  price;
    ((XdpAddOrder*)p)->Volume    =  size;
    ((XdpAddOrder*)p)->Side    =  s == BID ? 'B' : 'S';
    ((XdpAddOrder*)p)->OrderIDGTCIndicator    =  '0';
    ((XdpAddOrder*)p)->TradeSession    =  '2';

    currSize += sizeof(XdpAddOrder);
    p        += sizeof(XdpAddOrder);


    return currSize;
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
    ((XdpPktHdr*)p)->PktSize      = sizeof(XdpPktHdr) + sizeof(XdpExecutionOrder)  ;
    ((XdpPktHdr*)p)->DeliveryFlag = 11;
    ((XdpPktHdr*)p)->NumberMsgs   = 1;
    ((XdpPktHdr*)p)->SeqNum       = be32toh(static_cast<uint32_t>(sequence));
    ((XdpPktHdr*)p)->time.SourceTime   = 0; //TBDbe64toh(static_cast<uint64_t>(ts));
    ((XdpPktHdr*)p)->time.SourceTimeNS   = 0; //TBDbe64toh(static_cast<uint64_t>(ts));

    p        += sizeof(XdpPktHdr);
    currSize += sizeof(XdpPktHdr);

    ((XdpExecutionOrder*)p)->MsgSize    = sizeof(XdpExecutionOrder)  ;
    ((XdpExecutionOrder*)p)->MsgType    = (uint16_t)XDP_MSG_TYPE::ExecutionOrder ;

    ((XdpExecutionOrder*)p)->SourceTimeNS    = 0; //TBD
    ((XdpExecutionOrder*)p)->SymbolIndex    = secId ;
    ((XdpExecutionOrder*)p)->SymbolSeqNum    =  sequence;
    ((XdpExecutionOrder*)p)->OrderID   =  0; //not used by hw
    ((XdpExecutionOrder*)p)->Price    =  basePrice * priceMultiplier;
    ((XdpExecutionOrder*)p)->Volume    =  size;
    ((XdpExecutionOrder*)p)->OrderIDGTCIndicator    =  '0';
    ((XdpExecutionOrder*)p)->ReasonCode    =  '0';

    currSize += sizeof(XdpExecutionOrder);
    p        += sizeof(XdpExecutionOrder);


    return currSize;
  }

};
//--------------------------------------------------------

#endif

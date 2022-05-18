#ifndef _EKA_FH_NASDAQ_COMMON_PARSER_H_
#define _EKA_FH_NASDAQ_COMMON_PARSER_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "eka_macros.h"

namespace EfhNasdaqCommon {
  template <class T>
  inline uint32_t getInstrumentId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->instrumentId);
  }

  template <class T>
  inline uint32_t getOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->orderId);
  }
  
  template <class T>
  inline uint32_t getBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->bidOrderId);
  }
  
  template <class T>
  inline uint32_t getAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->askOrderId);
  }
  
  template <class T>
  inline uint32_t getOldOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->oldOrderId);
  }
  
  template <class T> inline uint32_t getNewOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->newOrderId);
  }
  template <class T>
  inline uint32_t getOldBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->oldBidOrderId);
  }
  
  template <class T>
  inline uint32_t getOldAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->oldAskOrderId);
  }
  template <class T> inline uint32_t getNewBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->newBidOrderId);
  }
  
  template <class T>
  inline uint32_t getNewAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->newAskOrderId);
  }
  
  inline uint32_t interpretVolume(uint32_t v) {
    return be32toh(v);
  }

  inline uint32_t interpretVolume(uint16_t v) {
    return be16toh(v);
  }
  
  template <class T>
  inline uint32_t getSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->volume);
  }
  
  template <class T>
  inline uint32_t getBidSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->bidSize);
  }
  
  template <class T>
  inline uint32_t getAskSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->askSize);
  }
  
  inline uint32_t interpretPrice(uint32_t v) {
    return be32toh(v);
  }

  inline uint32_t interpretPrice(uint16_t v) {
    return be16toh(v) * 100;
  }
  
  template <class T>
  inline uint32_t getPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice(msg->price);
  }
  
  template <class T>
  inline uint32_t getBidPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice(msg->bidPrice);
  }
  
  template <class T>
  inline uint32_t getAskPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice(msg->askPrice);
  }
  
  template <class T>
  inline SideT getSide(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->side) {
    case 'B' :
    case 'X' :
      return SideT::BID;
    case 'S' :
    case 'Y' :
      return SideT::ASK;
    default :
      on_error("Unexpected side \'%c\'",msg->side);
    }
  }

  template <class T>
  inline FhOrderType getOrderType(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->side) {
    case 'B' :
    case 'S' :
      return FhOrderType::CUSTOMER;
    case 'X' :
    case 'Y' :
      return FhOrderType::CUSTOMER_AON;
    default :
      on_error("Unexpected side \'%c\'",msg->side);
    }
  }

  template <class T>
  inline EfhAuctionUpdateType getAuctionUpdateType(const char* m) {
    switch (reinterpret_cast<const T*>(m)->auctionType) {
    case 'I' : // “I” = Order Exposure
      return EfhAuctionUpdateType::kNew; 
    case 'O' : // “O” = Opening
    case 'R' : // “R” = Reopening
    case 'P' : // “P” = Price Improvement (PRISM) Auction
    case 'B' : // “B” = Block Auction
      return EfhAuctionUpdateType::kUnknown;
    default :
      on_error("Unexpected auctionType \'%c\'",
	       reinterpret_cast<const T*>(m)->auctionType);
    }
  }

  template <class T>
  inline uint32_t getAuctionId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->auctionId);
  }

  template <class T>
  inline SideT getAuctionSide(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->imbalanceDirection) {
    case 'B' :
      return SideT::BID;
    case 'S' :
      return SideT::ASK;
    default :
      on_error("Unexpected side \'%c\'",msg->side);
    }
  }

   
  template <class T>
  inline uint32_t getAuctionPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->imbalancePrice);
  }
  
  template <class T>
  inline uint32_t getAuctionSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->imbalanceVolume);
  }

  template <class T>
  inline EfhOrderCapacity getAuctionOrderCapacity(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->customerFirmIndicator) {
    case 'C' : // “C” = Customer
      return EfhOrderCapacity::kCustomer;
    case 'F' : // “F” = Firm/ Joint Back Office (JBO)
      return EfhOrderCapacity::kAgency;
    case 'M' : // “M” = On-floor Market Maker
      return EfhOrderCapacity::kMarketMaker;
    case 'P' : // “P” = Professional Customer
      return EfhOrderCapacity::kProfessionalCustomer;
    case 'B' : // “B” = Broker Dealer/ Non Registered Market Maker
      return EfhOrderCapacity::kBrokerDealer;
    default :
      on_error("Unexpected customerFirmIndicator \'%c\'",msg->customerFirmIndicator);
    }
  }
  

  template <class T>
  inline size_t printGenericMsg(const uint8_t* m) {
    printf ("\n");
    return sizeof(T);
  }
  
  inline int printMsg(uint64_t sequence, const uint8_t* m) {
#if 0    
    auto genericHdr {reinterpret_cast<const GenericHdr*>(m)};
    char enc = genericHdr->type;
    uint64_t msgTs = be64toh(genericHdr->ts);

    printf("\t");
    printf("%s,",ts_ns2str(msgTs).c_str());
    printf ("%-8ju,",sequence);
    switch (enc) {
      //--------------------------------------------------------------
    case 'H':  // TradingAction
      return printGenericMsg<TradingAction>(m);
      //      return printTradingAction<TradingAction>(m);
    case 'a':  // AddOrderShort
      return printGenericMsg<AddOrderShort>(m);
      //      return printAddOrder<AddOrderShort>(m);
    case 'A':  // AddOrderLong
      return printGenericMsg<AddOrderLong>(m);
      //      return printAddOrder<AddOrderLong>(m);
    case 'j':  // AddQuoteShort
      return printGenericMsg<AddQuoteShort>(m);
      //      return printAddQuote<AddQuoteShort>(m);
    case 'J':  // AddQuoteLong
      return printGenericMsg<AddQuoteLong>(m);
      //      return printAddQuote<AddQuoteLong>(m);
    case 'E':  // OrderExecuted
      return printGenericMsg<OrderExecuted>(m);
      //      return printOrderExecuted<OrderExecuted>(m);
    case 'C':  // OrderExecutedPrice
      return printGenericMsg<OrderExecutedPrice>(m);
      //      return printOrderExecuted<OrderExecutedPrice>(m);
    case 'X':  // OrderCancel
      return printGenericMsg<OrderCancel>(m);
      //      return printOrderExecuted<OrderCancel>(m);
    case 'u':  // ReplaceOrderShort
      return printGenericMsg<ReplaceOrderShort>(m);
      //      return printReplaceOrder<ReplaceOrderShort>(m);
    case 'U':  // ReplaceOrderLong
      return printGenericMsg<ReplaceOrderLong>(m);
      //      return printReplaceOrder<ReplaceOrderLong>(m);
    case 'G':  // SingleSideUpdate
      return printGenericMsg<SingleSideUpdate>(m);
      //      return printSingleSideUpdate<SingleSideUpdate>(m);
    case 'k':  // QuoteReplaceShort
      return printGenericMsg<QuoteReplaceShort>(m);
      //      return printReplaceQuote<QuoteReplaceShort>(m);
    case 'K':  // QuoteReplaceLong
      return printGenericMsg<QuoteReplaceLong>(m);
      //      return printReplaceQuote<QuoteReplaceLong>(m);
    case 'Y':  // QuoteDelete
      return printGenericMsg<QuoteDelete>(m);
      //      return printDeleteQuote<QuoteDelete>(m);
    case 'Q':  // Trade
      return printGenericMsg<Trade>(m);
      //      return printTrade<Trade>(m);
    case 'S':  // SystemEvent
      return printGenericMsg<SystemEvent>(m);
    case 'B':  // BrokenTrade
      return printGenericMsg<BrokenTrade>(m);
    case 'I':  // NOII
      return printGenericMsg<NOII>(m);
    case 'M':  // EndOfSnapshot
      return printGenericMsg<EndOfSnapshot>(m);
    case 'R':  // Directory
      return printGenericMsg<Directory>(m);
    default: 
      on_error("UNEXPECTED Message type: enc=\'%c\'",enc);
    }
#endif    
    return 0;
  }
      
  inline ssize_t printPkt(const uint8_t* pkt) {
    auto p {pkt};
    auto moldHdr {reinterpret_cast<const mold_hdr*>(p)};
    auto sequence = be64toh(moldHdr->sequence);
    auto msgCnt   = be16toh(moldHdr->message_cnt);
    p += sizeof(*moldHdr);
    for (auto i = 0; i < msgCnt; i++) {
      auto msgLenRaw = reinterpret_cast<const uint16_t*>(p);
      auto msgLen = be16toh(*msgLenRaw);
      p += sizeof(*msgLenRaw);
      auto parsedMsgLen = printMsg(sequence,p);
      if (parsedMsgLen != msgLen)
	on_error("parsedMsgLen %d != msgLen %d",parsedMsgLen,msgLen);
      sequence++;
      p += msgLen;
    }
    return p - pkt;
  }  
}  // namespace EfhNasdaqCommon
#endif

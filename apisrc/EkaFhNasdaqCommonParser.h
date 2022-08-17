#ifndef _EKA_FH_NASDAQ_COMMON_PARSER_H_
#define _EKA_FH_NASDAQ_COMMON_PARSER_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "EkaFhTypes.h"
#include "eka_macros.h"

#define ITTO_NOM_MSG(x)					\
  x == 'S' ? "SYSTEM_EVENT" :				\
    x == 'R' ? "DIRECTORY"       :			\
    x == 'H' ? "TRADING_ACTION"  :			\
    x == 'O' ? "OPTION_OPEN"     :			\
    x == 'a' ? "ADD_ORDER_SHORT" :			\
    x == 'A' ? "ADD_ORDER_LONG"  :			\
    x == 'j' ? "ADD_QUOTE_SHORT" :			\
    x == 'J' ? "ADD_QUOTE_LONG"  :			\
    x == 'E' ? "SINGLE_SIDE_EXEC"  :			\
    x == 'C' ? "SINGLE_SIDE_EXEC_PRICE"  :		\
    x == 'u' ? "SINGLE_SIDE_REPLACE_SHORT" :		\
    x == 'U' ? "SINGLE_SIDE_REPLACE_LONG"  :		\
    x == 'D' ? "SINGLE_SIDE_DELETE" :			\
    x == 'G' ? "SINGLE_SIDE_UPDATE" :			\
    x == 'k' ? "QUOTE_REPLACE_SHORT" :			\
    x == 'K' ? "QUOTE_REPLACE_LONG" :			\
    x == 'Y' ? "QUOTE_DELETE"  :			\
    x == 'P' ? "TRADE"  :				\
    x == 'Q' ? "CROSS_TRADE"  :				\
    x == 'B' ? "BROKEN_TRADE"  :			\
    x == 'I' ? "NOII"  :				\
    x == 'M' ? "END_OF_SNAPSHOT"  :			\
    "UNKNOWN"

namespace EfhNasdaqCommon {
  using DefaultPriceT = uint32_t;
  
  template <class T>
  inline uint32_t getInstrumentId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->instrumentId);
  }

  template <class T>
  inline uint64_t getOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->orderId);
  }
  
  template <class T>
  inline uint64_t getBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    //    printf("bidOrderId = 0x%016jx\n",msg->bidOrderId);
    return be64toh(msg->bidOrderId);
  }
  
  template <class T>
  inline uint64_t getAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->askOrderId);
  }
  
  template <class T>
  inline uint64_t getOldOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->oldOrderId);
  }
  
  template <class T>
  inline uint64_t getNewOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->newOrderId);
  }
  template <class T>
  inline uint64_t getOldBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->oldBidOrderId);
  }
  
  template <class T>
  inline uint64_t getOldAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->oldAskOrderId);
  }
  template <class T>
  inline uint64_t getNewBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->newBidOrderId);
  }
  
  template <class T>
  inline uint64_t getNewAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be64toh(msg->newAskOrderId);
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
  inline uint32_t getCustSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->custSize);
  }
    
  template <class T>
  inline uint32_t getProCustSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->proCustSize);
  }
  
  template <class T>
  inline uint32_t getBidSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->bidSize);
  }
  
  template <class T>
  inline uint32_t getBidCustSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->bidCustSize);
  }
  
  template <class T>
  inline uint32_t getBidProCustSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->bidProCustSize);
  }
  
  template <class T>
  inline uint32_t getAskSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->askSize);
  }

  template <class T>
  inline uint32_t getAskCustSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->askCustSize);
  }
  
  template <class T>
  inline uint32_t getAskProCustSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->askProCustSize);
  }
  
  inline uint32_t interpretPrice(uint32_t v) {
    return be32toh(v);
  }

  inline uint32_t interpretPrice(int32_t v) {
    return be32toh(v);
  }
  
  inline uint32_t interpretPrice(uint16_t v) {
    return be16toh(v) * 100;
  }

  template <class PriceT>
  inline PriceT interpretPrice(uint32_t v) {
    return static_cast<PriceT>(be32toh(v));
  }

  template <class PriceT>
  inline PriceT interpretPrice(int32_t v) {
    return static_cast<PriceT>(be32toh(v));
  }

    
  template <class MsgT, class BookPriceT>
  inline uint32_t getPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const MsgT*>(m)};
    return interpretPrice<BookPriceT>(msg->price);
  }
  
  template <class MsgT>
  inline uint32_t getPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const MsgT*>(m)};
    return interpretPrice<DefaultPriceT>(msg->price);
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

  template <class T, class BookPriceT>
  inline uint32_t getBidPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice<BookPriceT>(msg->bidPrice);
  }
  
  template <class T, class BookPriceT>
  inline uint32_t getAskPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice<BookPriceT>(msg->askPrice);
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

  template <class MsgT,class PriceT>
  inline PriceT getStrikePrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const MsgT*>(m)};
    return interpretPrice<PriceT>(msg->strikePrice);
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
  inline EfhAuctionType getAuctionType(const uint8_t* m) {
    switch (reinterpret_cast<const T*>(m)->auctionType) {
    case 'I' : // “I” = Order Exposure
      return EfhAuctionType::kExposed;
    case 'O' : // “O” = Opening
      return EfhAuctionType::kOpen;
    case 'R' : // “R” = Reopening
      return EfhAuctionType::kReopen;
    case 'P' : // “P” = Price Improvement (PRISM) Auction
      return EfhAuctionType::kPrism;
    case 'B' : // “B” = Block Auction
      return EfhAuctionType::kFacilitation;
    default :
      on_error("Unexpected auctionType \'%c\'",
	       reinterpret_cast<const T*>(m)->auctionType);
    }
  }

  inline EfhOptionType decodeOptionType(char c) {
    switch (c) {
    case 'C' : return EfhOptionType::kCall;
    case 'P' : return EfhOptionType::kPut;
    default :
      on_error("Unexpected Option Type \'%c\'",c);
    }
  }

  template <class T>
  inline uint32_t getAuctionId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->auctionId);
  }

  template <class T>
  inline EfhOrderSide getAuctionSide(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->imbalanceDirection) {
    case 'B' :
      return EfhOrderSide::kBid;
    case 'S' :
      return EfhOrderSide::kAsk;
    default :
      on_error("Unexpected imbalanceDirection \'%c\'",
	       msg->imbalanceDirection);
    }
  }

   
  template <class T>
  inline uint32_t getAuctionPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->imbalancePrice);
  }
  
  template <class T>
  inline uint32_t getImbalanceSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->imbalanceVolume);
  }
  
  template <class T>
  inline uint32_t getPairedSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->pairedQuantity);
  }
  
  template <class T>
  inline EfhOrderCapacity
  getAuctionOrderCapacity(const uint8_t* m) {
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
      // for all non EfhAuctionType::kExposed RFQs
      return EfhOrderCapacity::kUnknown;
      // on_error("Unexpected customerFirmIndicator \'%c\'",
      // 	       msg->customerFirmIndicator);
    }
  }

  

  template <class T>
  inline uint32_t getRatio(const uint8_t* m) {
    return be32toh(reinterpret_cast<const T*>(m)->ratio);
  }
  
  template <class MsgT,class PriceT>
  inline PriceT getLegStrikePrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const MsgT*>(m)};
    return interpretPrice<PriceT>(msg->strikePrice);
  }
  
  template <class Msg>
  inline size_t printGenericMsg(FILE* fd, const uint8_t* m) {
    fprintf (fd,"\n");
    return sizeof(Msg);
  }

  template <class Msg>
  inline int printTradingAction(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%u,",getInstrumentId<Msg>(m));
    fprintf(fd,"\'%c\',",reinterpret_cast<const Msg*>(m)->state);
    fprintf (fd,"\n");
    return sizeof(Msg);
  }


  template <class Msg>
  inline int printOptionOpen(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%u,",getInstrumentId<Msg>(m));
    fprintf(fd,"\'%c\',",reinterpret_cast<const Msg*>(m)->state);
    fprintf (fd,"\n");
    return sizeof(Msg);
  }
  
  template <class Msg, class PriceT>
  inline int printAddOrder(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%u,",getInstrumentId<Msg>(m));
    fprintf(fd,"%ju,",getOrderId<Msg>(m));
    fprintf(fd,"\'%c\',",reinterpret_cast<const Msg*>(m)->side);
    fprintf(fd,"%u,",getPrice<Msg,PriceT>(m));
    fprintf(fd,"%u,",getSize<Msg>(m));
    fprintf (fd,"\n");
    return sizeof(Msg);
  }

  template <class Msg>
  inline int printAddQuote(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%u,",getInstrumentId<Msg>(m));
    fprintf(fd,"%ju,",getBidOrderId<Msg>(m));
    fprintf(fd,"%u,",getBidPrice<Msg>(m));
    fprintf(fd,"%u,",getBidSize<Msg>(m));
    fprintf(fd,"%ju,",getAskOrderId<Msg>(m));
    fprintf(fd,"%u,",getAskPrice<Msg>(m));
    fprintf(fd,"%u,",getAskSize<Msg>(m));
    fprintf (fd,"\n");
    return sizeof(Msg);
  }

  template <class Msg, class PriceT>
  inline int printReplaceOrder(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%ju,",getOldOrderId<Msg>(m));
    fprintf(fd,"%ju,",getNewOrderId<Msg>(m));
    fprintf(fd,"%u,",getPrice<Msg,PriceT>(m));
    fprintf(fd,"%u,",getSize<Msg>(m));
    fprintf (fd,"\n");
    return sizeof(Msg);
  }

  template <class Msg>
  inline int printReplaceQuote(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%ju,",getOldBidOrderId<Msg>(m));
    fprintf(fd,"%ju,",getNewBidOrderId<Msg>(m));
    fprintf(fd,"%u,",getBidPrice<Msg>(m));
    fprintf(fd,"%u,",getBidSize<Msg>(m));
    fprintf(fd,"%ju,",getOldAskOrderId<Msg>(m));
    fprintf(fd,"%ju,",getNewAskOrderId<Msg>(m));
    fprintf(fd,"%u,",getAskPrice<Msg>(m));
    fprintf(fd,"%u,",getAskSize<Msg>(m));    fprintf (fd,"\n");
    return sizeof(Msg);
  }
  
  template <class Msg>
  inline int printOrderExecuted(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%ju,",getOrderId<Msg>(m));
    fprintf(fd,"%u,",getSize<Msg>(m));
    fprintf (fd,"\n");
    return sizeof(Msg);
  }
  
  template <class Msg>
  inline int printDeleteOrder(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%ju,",getOrderId<Msg>(m));
    fprintf (fd,"\n");
    return sizeof(Msg);
  }
  
  template <class Msg, class PriceT>
  inline int printSingleSideUpdate(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%ju,",getOrderId<Msg>(m));
    fprintf(fd,"%u,",getPrice<Msg,PriceT>(m));
    fprintf(fd,"%u,",getSize<Msg>(m));
    fprintf (fd,"\n");
    return sizeof(Msg);
  }

  template <class Msg>
  inline int printDeleteQuote(FILE* fd, const uint8_t* m) {
    fprintf(fd,"%ju,",getBidOrderId<Msg>(m));
    fprintf(fd,"%ju,",getAskOrderId<Msg>(m));
    fprintf (fd,"\n");
    //   hexDump("DeleteQuote",m,sizeof(Msg));
    return sizeof(Msg);
  }
  
  template <class Feed>
  inline int
  printMsg(FILE* fd, uint64_t sequence, const uint8_t* m) {
    using PriceT = uint32_t; // Price to display
    auto genericHdr {reinterpret_cast
		     <const typename Feed::GenericHdr*>(m)};
    char enc = genericHdr->type;
    uint64_t msgTs = Feed::getTs(m);

    fprintf(fd,"\t");
    fprintf(fd,"%s,",ts_ns2str(msgTs).c_str());
    fprintf (fd,"%-8ju,",sequence);
    fprintf (fd,"\'%c\',",enc);
    fprintf (fd,"%s,",ITTO_NOM_MSG(enc));
    switch (enc) {
      //--------------------------------------------------------------
    case 'H':  // TradingAction
      return printTradingAction<typename Feed::TradingAction>(fd,m);
    case 'O':  // OptionOpen -- NOM only
      return printOptionOpen<typename Feed::OptionOpen>(fd,m);
    case 'a':  // AddOrderShort
      return printAddOrder<typename Feed::AddOrderShort,PriceT>(fd,m);
    case 'A':  // AddOrderLong
      return printAddOrder<typename Feed::AddOrderLong,PriceT>(fd,m);
    case 'j':  // AddQuoteShort
      return printAddQuote<typename Feed::AddQuoteShort>(fd,m);
    case 'J':  // AddQuoteLong
      return printAddQuote<typename Feed::AddQuoteLong>(fd,m);
    case 'E':  // OrderExecuted
      return printOrderExecuted<typename Feed::OrderExecuted>(fd,m);
    case 'C':  // OrderExecutedPrice
      return printOrderExecuted<typename Feed::OrderExecutedPrice>(fd,m);
    case 'X':  // OrderCancel
      return printOrderExecuted<typename Feed::OrderCancel>(fd,m);
    case 'u':  // ReplaceOrderShort
      return printReplaceOrder<typename Feed::ReplaceOrderShort,PriceT>(fd,m);
    case 'U':  // ReplaceOrderLong
      return printReplaceOrder<typename Feed::ReplaceOrderLong,PriceT>(fd,m);
    case 'D':  // DeleteOrder
      return printDeleteOrder<typename Feed::SingleSideDelete>(fd,m);
    case 'G':  // SingleSideUpdate
      return printSingleSideUpdate<typename Feed::SingleSideUpdate,PriceT>(fd,m);
    case 'k':  // QuoteReplaceShort
      return printReplaceQuote<typename Feed::QuoteReplaceShort>(fd,m);
    case 'K':  // QuoteReplaceLong
      return printReplaceQuote<typename Feed::QuoteReplaceLong>(fd,m);
    case 'Y':  // QuoteDelete
      return printDeleteQuote<typename Feed::QuoteDelete>(fd,m);
    case 'Q':  // Trade
      return printGenericMsg<typename Feed::Trade>(fd,m);
      //      return printTrade<typename Feed::Trade>(fd,m);
    case 'S':  // SystemEvent
      return printGenericMsg<typename Feed::SystemEvent>(fd,m);
    case 'B':  // BrokenTrade
      return printGenericMsg<typename Feed::BrokenTrade>(fd,m);
    case 'I':  // NOII
      return printGenericMsg<typename Feed::NOII>(fd,m);
    case 'M':  // EndOfSnapshot
      return printGenericMsg<typename Feed::EndOfSnapshot>(fd,m);
    case 'R':  // Directory
      return printGenericMsg<typename Feed::Directory>(fd,m);
    default: 
      on_error("UNEXPECTED Message type: enc=\'%c\'",enc);
    }
    return 0;
  }

  template <class T>
  inline ssize_t printPkt(FILE* fd, const uint8_t* pkt) {
    auto p {pkt};
    auto moldHdr {reinterpret_cast<const mold_hdr*>(p)};
    auto sequence = be64toh(moldHdr->sequence);
    auto msgCnt   = be16toh(moldHdr->message_cnt);
    p += sizeof(*moldHdr);
    for (auto i = 0; i < msgCnt; i++) {
      auto msgLenRaw = reinterpret_cast<const uint16_t*>(p);
      auto msgLen = be16toh(*msgLenRaw);
      p += sizeof(*msgLenRaw);
      auto parsedMsgLen = printMsg<T>(fd,sequence,p);
      if (parsedMsgLen != msgLen)
	on_error("parsedMsgLen %d != msgLen %d",
		 parsedMsgLen,msgLen);
      sequence++;
      p += msgLen;
    }
    return p - pkt;
  }  
}  // namespace EfhNasdaqCommon
#endif

#ifndef _EKA_FH_PLR_PARSER_H_
#define _EKA_FH_PLR_PARSER_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "eka_macros.h"

namespace Plr {
  struct PktHdr {
    uint16_t pktSize;
    uint8_t  deliveryFlag;
    uint8_t  numMsgs;
    uint32_t seqNum;
    uint32_t seconds;
    uint32_t ns;
  };

  enum class DeliveryFlag : uint8_t  {
    Heartbeat = 1,
      Failover = 10,
      Original = 11,
      SeqReset = 12,
      SinglePktRetransmit = 13,
      PartOfRetransmit = 15,
      SinglePktRefresh = 17,
      StratOfRefresh = 18,
      PartOfRefresh = 19,
      EndOfRefresh = 20,
      MsgUnavail = 21
      };

  inline std::string deliveryFlag2str (uint8_t flag) {
    switch (static_cast<DeliveryFlag>(flag)) {
    case DeliveryFlag::Heartbeat :
      return std::string("Heartbeat");
    case DeliveryFlag::Failover :
      return std::string("Failover");
    case DeliveryFlag::Original :
      return std::string("Original");
    case DeliveryFlag::SeqReset :
      return std::string("SeqReset");
    case DeliveryFlag::SinglePktRetransmit :
      return std::string("SinglePktRetransmit");
    case DeliveryFlag::PartOfRetransmit :
      return std::string("PartOfRetransmit");
    case DeliveryFlag::SinglePktRefresh :
      return std::string("SinglePktRefresh");
    case DeliveryFlag::StratOfRefresh :
      return std::string("StratOfRefresh");
    case DeliveryFlag::PartOfRefresh :
      return std::string("PartOfRefresh");
    case DeliveryFlag::EndOfRefresh :
      return std::string("EndOfRefresh");
    case DeliveryFlag::MsgUnavail :
      return std::string("MsgUnavail");
    default:
      on_error("Unexpected DeliveryFlag %u",flag);
    }
  }


  enum class MsgType : uint16_t {
    // Control
    SequenceNumberReset = 1,
      TimeReferenceMessage = 2,
      SymbolIndexMapping = 3,
      RetransmissionRequestMessage = 10,
      RequestResponseMessage = 11,
      HeartbeatResponseMessage = 12,
      SymbolIndexMappingRequestMessage = 13,
      RefreshRequestMessage = 15,
      MessageUnavailable = 31,
      SymbolClear = 32,
      SecurityStatusMessage = 34,
      RefreshHeaderMessage = 35,
      OutrightSeriesIndexMapping = 50,
      OptionsStatusMessage = 51,

      // Top
      Quote = 340,
      Trade = 320,
      TradeCancel = 321,
      TradeCorrection = 322,
      Imbalance = 305,
      SeriesRfq = 307,
      SeriesSummary = 323
      };

  inline std::string msgType2str (uint16_t type) {
    switch (static_cast<MsgType>(type)) {
    case MsgType::SequenceNumberReset :
      return std::string("SequenceNumberReset");
    case MsgType::TimeReferenceMessage :
      return std::string("TimeReferenceMessage");
    case MsgType::SymbolIndexMapping :
      return std::string("SymbolIndexMapping");
    case MsgType::RetransmissionRequestMessage :
      return std::string("RetransmissionRequestMessage");
    case MsgType::RequestResponseMessage :
      return std::string("RequestResponseMessage");            
    case MsgType::SymbolIndexMappingRequestMessage :
      return std::string("SymbolIndexMappingRequestMessage");
    case MsgType::RefreshRequestMessage :
      return std::string("RefreshRequestMessage");
    case MsgType::MessageUnavailable :
      return std::string("MessageUnavailable");
    case MsgType::SymbolClear :
      return std::string("SymbolClear");            
    case MsgType::SecurityStatusMessage :
      return std::string("SecurityStatusMessage");            
    case MsgType::RefreshHeaderMessage :
      return std::string("RefreshHeaderMessage");            
    case MsgType::OutrightSeriesIndexMapping :
      return std::string("OutrightSeriesIndexMapping");            
    case MsgType::OptionsStatusMessage :
      return std::string("OptionsStatusMessage");            
    case MsgType::Quote :
      return std::string("Quote");            
    case MsgType::Trade :
      return std::string("Trade");            
    case MsgType::TradeCancel :
      return std::string("TradeCancel");
    case MsgType::TradeCorrection :
      return std::string("TradeCorrection");
    case MsgType::Imbalance :
      return std::string("Imbalance");
    case MsgType::SeriesRfq :
      return std::string("SeriesRfq");
    case MsgType::SeriesSummary :
      return std::string("SeriesSummary");
    default:
      on_error("Unexpected MsgType %u",type);
    }

  }
  struct MsgHdr {
    uint16_t size;
    uint16_t type;
  };

  inline uint32_t printPkt(const uint8_t* pkt) {
    auto p {pkt};
    auto pktHdr {reinterpret_cast<const PktHdr*>(p)};

    uint64_t ts = pktHdr->seconds * 1e9 + pktHdr->ns;
    printf ("%u,",pktHdr->seqNum);
    printf ("%s,",ts_ns2str(ts).c_str());
    printf ("%s,",deliveryFlag2str(pktHdr->deliveryFlag).c_str());
    printf("\n");
    
    p += sizeof(*pktHdr);

    for (auto i = 0; i < pktHdr->numMsgs; i++) {
      auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};
      printf ("\t%s\n",msgType2str(msgHdr->type).c_str());
      p += msgHdr->size;
    }
    return p - pkt;
  }
} // namespace Plr


#endif

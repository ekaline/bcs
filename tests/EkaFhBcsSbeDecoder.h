#ifndef __EKA_FH_BCS_DECODER_H__
#define __EKA_FH_BCS_DECODER_H__

#include <bitset>
#include <string>
#include <vector>

namespace BcsSbe {
enum class MsgId : uint16_t {
  Heartbeat = 1,
  SequenceReset = 2,
  BestPrices = 3,
  EmptyBook = 4,
  OrderUpdate = 5,
  OrderExecution = 6,
  OrderBookSnapshot = 7,
  SecurityDefinition = 8,
  SecurityStatus = 9,
  TradingSessionStatus = 11,
  Trade = 16,
  Logon = 1000,
  Logout = 1001,
  MarketDataRequest = 1002,
  MarketDataDummyMessage = 1003,
};

static inline const char *decodeMsgId(MsgId id) {
  switch (id) {
  case MsgId::Heartbeat:
    return "Heartbeat";
  case MsgId::SequenceReset:
    return "SequenceReset";
  case MsgId::BestPrices:
    return "BestPrices";
  case MsgId::EmptyBook:
    return "EmptyBook";
  case MsgId::OrderUpdate:
    return "OrderUpdate";
  case MsgId::OrderExecution:
    return "OrderExecution";
  case MsgId::OrderBookSnapshot:
    return "OrderBookSnapshot";
  case MsgId::SecurityDefinition:
    return "SecurityDefinition";
  case MsgId::SecurityStatus:
    return "SecurityStatus";
  case MsgId::TradingSessionStatus:
    return "TradingSessionStatus";
  case MsgId::Trade:
    return "Trade";
  case MsgId::Logon:
    return "Logon";
  case MsgId::Logout:
    return "Logout";
  case MsgId::MarketDataRequest:
    return "MarketDataRequest";
  case MsgId::MarketDataDummyMessage:
    return "MarketDataDummyMessage";
  default:
    return "Unexpected";
  }
}

typedef uint8_t uint8;
typedef int8_t int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;

typedef uint8 uInt8_T;
typedef uint8 uInt8NULL_T;
typedef uint16 uInt16_T;
typedef uint16 uInt16NULL_T;
typedef uint32 uInt32_T;
typedef uint32 uInt32NULL_T;
typedef uint64 uInt64_T;
typedef uint64 uInt64NULL_T;
typedef int32 Int32_T;
typedef int32 Int32NULL_T;
typedef int64 Int64_T;
typedef int64 Int64NULL_T;
typedef uint64 UTCTimestamp_T;
typedef uint64 UTCTimeOnly_T;
typedef char Char_T;
typedef char String4_T[4];
typedef char String5_T[5];
typedef char String6_T[6];
typedef char String8_T[8];
typedef char String12_T[12];
typedef char String20_T[20];
typedef char String256_T[256];
typedef char BoardID_T[4];
typedef char SecurityID_T[12];
typedef uint32 ExchangeTradingSessionID_T;
typedef char MarketID_T[4];
typedef char SecurityIDSource_T;

enum class MDEntryType : char {
  Bid = '0',
  Offer = '1',
  EmptyBook = 'J'
};

static inline const char *decodeMdEntryType(MDEntryType t) {
  switch (t) {
  case MDEntryType::Bid:
    return "Bid";
  case MDEntryType::Offer:
    return "Offer";
  case MDEntryType::EmptyBook:
    return "EmptyBook";
  default:
    on_error("Unexpected MDEntryType \'%c\'", (char)t);
  }
}

enum class MDFlagSet : uint32_t {
  Order = 0,
  Quote = 1,
  MarketInAuction = 2,
  LastFragment = 3,
  Negotiated = 4,
  Confirmed = 5,
  DarkPool = 6,
  AuctionResult = 7,
};

static inline const char *decodeMDFlagSet(MDFlagSet f) {
  switch (f) {
  case MDFlagSet::Order:
    return "Order";
  case MDFlagSet::Quote:
    return "Quote";
  case MDFlagSet::MarketInAuction:
    return "MarketInAuction";
  case MDFlagSet::LastFragment:
    return "LastFragment";
  case MDFlagSet::Negotiated:
    return "Negotiated";
  case MDFlagSet::Confirmed:
    return "Confirmed";
  case MDFlagSet::DarkPool:
    return "DarkPool";
  case MDFlagSet::AuctionResult:
    return "AuctionResult";
  default:
    on_error("Unexpected MDFlagSet \'%u\'", (uint)f);
  }
}

static std::string MDFlagSetToString(MDFlagSet flagSet) {
  auto bits = static_cast<uint32_t>(flagSet);
  std::vector<std::string> flagNames = {
      "Order",        "Quote",        "MarketInAuction",
      "LastFragment", "Negotiated",   "Confirmed",
      "DarkPool",     "AuctionResult"};

  std::string result;

  for (uint32_t i = 0; i < flagNames.size(); ++i) {
    if ((bits & (1 << i)) != 0) {
      if (!result.empty()) {
        result += ", ";
      }
      result += flagNames[i];
    }
  }
  return result;
}

enum class MDUpdateAction : uint8_t {
  New = 0,
  Change = 1,
  Delete = 2,
};

static inline const char *
decodeMDUpdateAction(MDUpdateAction f) {
  switch (f) {
  case MDUpdateAction::New:
    return "New";
  case MDUpdateAction::Change:
    return "Change";
  case MDUpdateAction::Delete:
    return "Delete";
  default:
    on_error("Unexpected MDUpdateAction \'%u\'", (uint)f);
  }
}

struct monthYearNull_T {
  uint16 year_T;
  uint8 month_T;
  uint8 day_T;
} __attribute__((packed));

struct Decimal9NULL_T {
  int64 mantissa;
  // int8 exponent;
} __attribute__((packed));
struct PktHdr {
  uint32_t
      msgSeqNum; // Packet sequence number. Next packet is
                 // assigned sequence number incremented by
                 // one. Incremental Packets sequence
                 // numbers are always reset to 1 at SIMBA
                 // ASTS gateway start or re-start.
  uint16_t pktSize; // Packet length in bytes, including
                    // packet header length.
  uint16_t
      pktFlags; // Packet flags:
                //    0x1 – Last data fragment
                //          (LastFragment): 0 – not the last
                //          data fragment, expect
                //          continuation in next packet; 1 –
                //          last data fragment, next packet
                //          will contain data for later
                //          event.
                //    0x2 – indicates the first packet in
                //          snapshot per instrument
                //          (StartOfSnapshot);
                //    0x4 – indicates the first packet in
                //          snapshot per instrument
                //          (EndOfSnapshot);
                //    0x8 – IncrementalPacket flag:
                //          0 – indicates the Snapshot
                //          packet,
                //          1 – indicates the
                //          Incremental packet.
  uint64_t sendTimeUTC; // Timestamp of sending packet.
                        // Number of nanoseconds since
                        // Unix epoch, UTC.
} __attribute__((packed));

struct PktIncrHdr {
  uint64_t
      transactTime; // Timestamp of transaction. Number of
                    // nanoseconds since Unix epoch, UTC.
  int32_t
      exchTradingSessionId; // Management field. Changing
                            // value within trading day has
                            // a meaning of Trading System
                            // full restart.
} __attribute__((packed));

struct MsgHdr { // Template ID and length of
                // message root

  uint16 blockLength; // A length of root part of message in
                      // bytes. Does not include SBE header,
                      // NoMDEntries field and repeating
                      // group of fields.
  uint16 templateId;
  uint16 schemaId;
  uint16 version;
} __attribute__((packed));

struct GroupSize { // Repeating group dimensions
  uint16 blockLength;
  uint8 numInGroup;
} __attribute__((packed));

struct SequenceResetMsg {
  uint32 NewSeqNo; // New sequence number
} __attribute__((packed));

struct BestPricesMsg_MdEntry {
  Decimal9NULL_T MktBidPx;
  Decimal9NULL_T MktOfferPx;
  Int64NULL_T MktBidSize;
  Int64NULL_T MktOfferSize;
  BoardID_T Board;
  SecurityID_T Symbol;
} __attribute__((packed));

struct OrderBookSnapshotMsg_root {
  uInt32_T LastMsgSeqNumProcessed;
  uInt32_T RptSeq;
  BoardID_T Board;
  SecurityID_T Symbol;
} __attribute__((packed));

struct OrderBookSnapshotMsg_MdEntry {
  Int64NULL_T MDEntryID;
  UTCTimestamp_T TransactTime;
  Decimal9NULL_T MDEntryPx;
  Int64NULL_T MDEntrySize;
  MDFlagSet mdFlags;
  MDEntryType mdEntryType;
} __attribute__((packed));

struct OrderUpdateMsg {
  Int64NULL_T MDEntryID;
  Decimal9NULL_T MDEntryPx;
  Int64NULL_T MDEntrySize;
  MDFlagSet MDFlags;
  uInt32NULL_T RptSeq;
  MDUpdateAction mdUpdateAction;
  MDEntryType mdEntryType;
  BoardID_T Board;
  SecurityID_T Symbol;
} __attribute__((packed));

struct OrderExecutionMsg {
  Int64NULL_T MDEntryID;
  Decimal9NULL_T MDEntryPx;
  Int64NULL_T MDEntrySize;
  Decimal9NULL_T LastPx;
  Int64NULL_T LastQty;
  uInt64NULL_T TradeID;
  MDFlagSet MDFlags;
  uInt32NULL_T RptSeq;
  MDUpdateAction mdUpdateAction;
  MDEntryType mdEntryType;
  BoardID_T Board;
  SecurityID_T Symbol;
} __attribute__((packed));

const uint8_t *printMsg(const void *msg,
                        FILE *outFile = stdout);

const uint8_t *printMsg(const void *msg, FILE *outFile) {
  auto p = reinterpret_cast<const uint8_t *>(msg);
  auto msgHdr = reinterpret_cast<const MsgHdr *>(msg);

  auto msgId = static_cast<MsgId>(msgHdr->templateId);
  fprintf(outFile, "\t%s,", decodeMsgId(msgId));

  p += sizeof(*msgHdr);
  p += msgHdr->blockLength;

  switch (msgId) {
  case MsgId::Heartbeat:
  case MsgId::SequenceReset:
  case MsgId::EmptyBook:
    break;
    // ====================================================
  case MsgId::OrderUpdate: {
    auto m = reinterpret_cast<const OrderUpdateMsg *>(
        p - msgHdr->blockLength);
    fprintf(
        outFile, "%s: ",
        std::string(m->Symbol, sizeof(m->Symbol)).c_str());
    fprintf(outFile, "%s ",
            decodeMDUpdateAction(m->mdUpdateAction));
    fprintf(outFile,
            "%s: ", decodeMdEntryType(m->mdEntryType));
    fprintf(outFile, "MDEntryPx: %jde-9,",
            m->MDEntryPx.mantissa);
    fprintf(outFile, "MDEntrySize: %jd, ", m->MDEntrySize);
    fprintf(outFile, "%s ",
            MDFlagSetToString(m->MDFlags).c_str());

  } break;
    // ====================================================

  case MsgId::OrderExecution: {
    auto m = reinterpret_cast<const OrderExecutionMsg *>(
        p - msgHdr->blockLength);
    fprintf(
        outFile, "%s: ",
        std::string(m->Symbol, sizeof(m->Symbol)).c_str());
    fprintf(outFile, "%s: ",
            decodeMDUpdateAction(m->mdUpdateAction));
    fprintf(outFile,
            "%s: ", decodeMdEntryType(m->mdEntryType));
    fprintf(outFile, "MDEntryPx: %jde-9,",
            m->MDEntryPx.mantissa);
    fprintf(outFile, "MDEntrySize: %jd, ", m->MDEntrySize);
    fprintf(outFile, "LastPx: %jde-9,", m->LastPx.mantissa);
    fprintf(outFile, "LastQty: %jd, ", m->LastQty);
    fprintf(outFile, "%s ",
            MDFlagSetToString(m->MDFlags).c_str());

  } break;

    // ====================================================

  case MsgId::SecurityDefinition:
  case MsgId::SecurityStatus:
  case MsgId::TradingSessionStatus:
  case MsgId::Trade:
  case MsgId::Logon:
  case MsgId::Logout:
  case MsgId::MarketDataRequest:
  case MsgId::MarketDataDummyMessage:
    break;
    // ====================================================
  case MsgId::OrderBookSnapshot: {
    auto msgRoot =
        reinterpret_cast<const OrderBookSnapshotMsg_root *>(
            p);
    fprintf(outFile, "%s: ",
            std::string(msgRoot->Symbol,
                        sizeof(msgRoot->Symbol))
                .c_str());
    auto grSize = reinterpret_cast<const GroupSize *>(p);
    p += sizeof(*grSize);

    for (auto i = 0; i < grSize->numInGroup; i++) {
      auto gr = reinterpret_cast<
          const OrderBookSnapshotMsg_MdEntry *>(p);
      fprintf(outFile,
              "%s: ", decodeMdEntryType(gr->mdEntryType));
      p += grSize->blockLength;
    }
  } break;
    // ====================================================

  case MsgId::BestPrices: {
    auto grSize = reinterpret_cast<const GroupSize *>(p);
    p += sizeof(*grSize);
    for (auto i = 0; i < grSize->numInGroup; i++) {
      auto gr =
          reinterpret_cast<const BestPricesMsg_MdEntry *>(
              p);
      printf("\n\t\t");
      fprintf(outFile, "%s: ",
              std::string(gr->Symbol, sizeof(gr->Symbol))
                  .c_str());
      fprintf(outFile, "MktBidPx: %jde-9,",
              gr->MktBidPx.mantissa);
      fprintf(outFile, "MktOfferPx: %jde-9,",
              gr->MktOfferPx.mantissa);
      fprintf(outFile, "MktBidSize: %jd, ", gr->MktBidSize);
      fprintf(outFile, "MktOfferSize: %jd, ",
              gr->MktOfferSize);
      p += grSize->blockLength;
    }
  } break;
  default:
    on_error("Unexpected Message %u", (uint)msgId);
  }
  return p;
}

void printPkt(const void *pkt, FILE *outFile = stdout);

void printPkt(const void *pkt, FILE *outFile) {
  auto startOfPkt = reinterpret_cast<const uint8_t *>(pkt);
  auto p = startOfPkt;
  auto pktHdr = reinterpret_cast<const PktHdr *>(p);
  fprintf(outFile, "%8u,", pktHdr->msgSeqNum);
  fprintf(outFile, "%3u,", pktHdr->pktSize);
  fprintf(outFile, "%s,",
          ts_ns2str(pktHdr->sendTimeUTC).c_str());
  // fprintf(outFile,"\n");

  p += sizeof(*pktHdr);

  if (pktHdr->pktFlags & 0x8) {
    auto incrPktHdr =
        reinterpret_cast<const PktIncrHdr *>(p);
    fprintf(outFile, "%s:\n",
            ts_ns2str(incrPktHdr->transactTime).c_str());
    // fprintf(outFile,"%3d,",
    // incrPktHdr->exchTradingSessionId);
    p += sizeof(*incrPktHdr);
  }
  while (p - startOfPkt < pktHdr->pktSize) {
    p = printMsg(p);
    fprintf(outFile, "\n");
  }
  if (p - startOfPkt != pktHdr->pktSize)
    on_error("p - startOfPkt %jd != pktHdr->pktSize %u",
             p - startOfPkt, pktHdr->pktSize);
}
} // namespace BcsSbe

#endif

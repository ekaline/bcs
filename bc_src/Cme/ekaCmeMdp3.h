#ifndef _EKA_CME_MDP3_H
#define _EKA_CME_MDP3_H

#include "eka_bc_book.h"

struct Mdp3PktHdr {
  uint32_t seq;
  uint64_t time;
} __attribute__((packed));

enum class Mdp3TemplateId : uint16_t {
  MDIncrementalRefreshBook = 46, 
    MDIncrementalRefreshOrderBook = 47, 
    MDIncrementalRefreshVolume = 37, 
    MDIncrementalRefreshTradeSummary = 48, 
    MDIncrementalRefreshSessionStatistics = 51
    };

#define printTemplateName(x)						\
  x == Mdp3TemplateId::MDIncrementalRefreshBook  ? "MDIncrementalRefreshBook46" : \
    x == Mdp3TemplateId::MDIncrementalRefreshOrderBook  ? "MDIncrementalRefreshOrderBook47" : \
    x == Mdp3TemplateId::MDIncrementalRefreshVolume  ? "MDIncrementalRefreshVolume37" : \
    x == Mdp3TemplateId::MDIncrementalRefreshTradeSummary  ? "MDIncrementalRefreshTradeSummary48" : \
    x == Mdp3TemplateId::MDIncrementalRefreshSessionStatistics  ? "MDIncrementalRefreshSessionStatistics51" : \
    "UNKNOWN"

struct Mdp3MsgHdr {
  uint16_t       size;
  uint16_t       blockLen;
  Mdp3TemplateId templateId;
  uint16_t       schemaId;
  uint16_t       version;
} __attribute__((packed));

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
    x == 'W' ? "FixingPrice" :				  \
    x == 'e' ? "ElectronicVolume" :			  \
    x == 'g' ? "ThresholdLimitsandPriceBandVariation":	  \
    "UNKNOWN"

#define updateAction2str(x)			\
  x == 0 ? "New" :				\
    x == 1 ? "Change" :				\
    x == 2 ? "Delete" :				\
    x == 3 ? "DeleteThru" :			\
    x == 4 ? "DeleteFrom" :			\
    x == 5 ? "Overlay" :			\
    "UNKNOWN"

#define orderUpdateAction2str(x)		\
  x == 0 ? "New" :				\
    x == 1 ? "Change" :				\
    x == 2 ? "Delete" :				\
    "UNKNOWN"



struct NumInGroup {
  uint16_t blockLen;
  uint8_t  numInGroup;
} __attribute__((packed));

struct NumInGroup8 {
  uint16_t blockLen;
  uint8_t  pad[5];
  uint8_t  numInGroup;
} __attribute__((packed));

struct RefreshBookRoot { // 11
  uint64_t    TransactTime;       // 8
  uint8_t     MatchEventIndicator;// 1
  uint8_t     pad[2];             // 2
} __attribute__((packed));

//####################################################
class MdEntryIncrementalRefreshBook46 {
 public:
  static bool isBookUpdate(uint8_t* p);
  static void print(uint num, uint8_t* p);
  static uint64_t getPrice(uint8_t* p);
  static uint32_t getSize(uint8_t* p);
  static uint32_t getSecurityId(uint8_t* p);
  static uint32_t getSequence(uint8_t* p);

  static CmeBook::ACTION getAction(uint8_t* p);
  static CmeBook::SIDE getSide(uint8_t* p);

 private:
  struct MsgLayout { // 32 
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

};
//####################################################

struct MdEntryIncrementalRefreshOrderBook47 { // 32 
  uint64_t orderId;               // 8
  uint64_t orderPriority;         // 8
  uint64_t price;                 // 8
  uint32_t size;                  // 4
  uint32_t SecurityId;            // 4
  uint8_t  MDUpdateAction;        // 1
  char     MDEntryType;           // 1
  uint8_t  pad[6];                // 6
} __attribute__((packed));


struct MdEntryIncrementalRefreshVolume37 { // 16 
  uint32_t size;                  // 4
  uint32_t SecurityId;            // 4
  uint32_t seq;                   // 4
  uint8_t  MDUpdateAction;        // 1
  char     MDEntryType;           // 1
  uint8_t  pad[3];                // 3
} __attribute__((packed));


struct MdEntryIncrementalRefreshSessionStatistics51 { // 24 
  uint64_t price;                 // 8
  uint32_t SecurityId;            // 4
  uint32_t seq;                   // 4
  uint8_t  OpenCloseSettlFlag;    // 1
  uint8_t  MDUpdateAction;        // 1
  char     MDEntryType;           // 1
  uint32_t MDEntrySize;           // 1
  uint8_t  pad[4];                // 4
} __attribute__((packed));


struct OrderEntry { // 24
  uint64_t orderId;               // 8
  uint64_t MdOrderPriority;       // 8
  uint32_t Qty;                   // 4
  uint8_t  RefId;                 // 1
  uint8_t  OrderUpdAction;        // 1
  uint8_t  pad[2];                // 2
} __attribute__((packed));

#endif

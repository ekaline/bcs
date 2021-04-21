#ifndef _EKA_EFC_DATA_STRUCTS_H_
#define _EKA_EFC_DATA_STRUCTS_H_

#define P4_CTX_PRICE_SCALE 100

#define EFC_CTX_SANITY_CHECK 1

enum {
    MAX_SEC_CTX                 = 768*1024,
    MAX_SESSION_CTX_PER_CORE    = 128,
};

struct global_params {
    uint8_t     enable_strategy;
    uint8_t     report_only;
    uint8_t     debug_always_fire_on_unsubscribed; // relevant only if debug_always_fire is enabled
    uint8_t     debug_always_fire; // if strategy_enable was on prior to fire,
                                   // it will not be disabled due to a fire
                                   // produced by always_fire.
                                   // to activate, write from CLI
                                   // "efh_tool write 4 0x0200000000f00f00 0xefa0beda 8"
    uint32_t    max_size;
    uint64_t    watchdog_timeout_sec;
};

// FixedPrice is uint16_t


/* struct EkaHwSecCtx { */
/*   uint8_t    lowerBytesOfSecId; */
/*   uint8_t    verNum; */
/*   uint8_t    size; */
/*   FixedPrice askMaxPrice; */
/*   FixedPrice bidMinPrice; */
/* } __attribute__ ((packed)); */

struct SqfShortQuoteBlockMsg {
  char     typeSub[2]; // = {'Q','Q'};
  uint32_t badge;
  uint64_t messageId;
  uint16_t quoteCount;  // = 1
  uint32_t optionId;
  uint32_t bidPrice;
  uint32_t bidSize;
  uint32_t askPrice;
  uint32_t askSize;
  char     reentry;
} __attribute__((packed));

/* { */
/*   "shortDescription"  : "NewOrder", */
/*   "messageType"       : "0x38", */
/*   "messageDescription": "", */
/*   "fields"            : [ */
/*     {"name": "StartOfMessage", "size": 2, "type": "UnsignedNumeric", "desc": "Binary Must be 0xBA 0xBA."}, */
/*     {"name": "MessageLength", "size": 2, "type": "UnsignedNumeric", "desc": "Binary Number of bytes for the message, including this field but not including the two bytes for the StartOfMessage field."}, */
/*     {"name": "MessageType", "size": 1, "type": "UnsignedNumeric", "desc": "Binary message type"}, */
/*     {"name": "MatchingUnit", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Always 0 for inbound (Member to Cboe) messages."}, */
/*     {"name": "SequenceNumber", "size": 4, "type": "UnsignedNumeric", "desc": "Binary The sequence number for this message."}, */
/*     {"name": "ClOrdID", "size": 20, "type": "String", "desc": "Text Corresponds to ClOrdID (11) in Cboe FIX. ID chosen by the client. Characters in the ASCII range 33-126 are allowed, except for comma, semicolon, and pipe. If the ClOrdID matches a live order, the order will be rejected as duplicate. Note: Cboe only enforces uniqueness of ClOrdID values among currently live orders, which includes long-lived, persisting GTC/GTD orders. However, we strongly recommend that you keep your ClOrdID values unique."}, */
/*     {"name": "Side", "size": 1, "type": "String", "desc": "Alphanumeric Corresponds to Side (54) in Cboe FIX. 1 = Buy 2 = Sell"}, */
/*     {"name": "OrderQty", "size": 4, "type": "UnsignedNumeric", "desc": "Binary Corresponds to OrderQty (38) in Cboe FIX. Order quantity. System limit is 999,999 contracts. NumberOf NewOrder"}, */
/*     {"name": "Bitfields1", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Bitfield identifying which bitfields are set. Field values must be appended to the end of the message."}, */
/*     {"name": "Bitfields2", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Bitfield identifying which bitfields are set. Field values must be appended to the end of the message."}, */
/*     {"name": "Bitfields3", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Bitfield identifying which bitfields are set. Field values must be appended to the end of the message."}, */
/*     {"name": "Bitfields4", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Bitfield identifying which bitfields are set. Field values must be appended to the end of the message."}, */
/*     {"name": "Bitfields5", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Bitfield identifying which bitfields are set. Field values must be appended to the end of the message."}, */
/*     {"name": "Bitfields6", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Bitfield identifying which bitfields are set. Field values must be appended to the end of the message."}, */
/*     {"name": "Bitfields7", "size": 1, "type": "UnsignedNumeric", "desc": "Binary Bitfield identifying which bitfields are set. Field values must be appended to the end of the message."}, */
/*     {"name": "ClearingFirm", "size": 4, "type": "String", "desc": ""}, */
/*     {"name": "ClearingAccount", "size": 4, "type": "String", "desc": ""}, */
/*     {"name": "Price", "size": 8, "type": "UnsignedNumeric", "desc": ""}, */
/*     {"name": "OrdType", "size": 1, "type": "String", "desc": ""}, */
/*     {"name": "TimeInForce", "size": 1, "type": "String", "desc": ""}, */
/*     {"name": "Symbol", "size": 8, "type": "String", "desc": ""}, */
/*     {"name": "Capacity", "size": 1, "type": "String", "desc": ""}, */
/*     {"name": "Account", "size": 16, "type": "String", "desc": ""} */
/*   ] */
/* } */

struct BoeNewOrderMsg {
  uint16_t      StartOfMessage; // 0xBABA
  uint16_t      MessageLength;  // sizeof(BoeNewOrderMsg) - 2
  uint8_t       MessageType;    // 0x38
  uint8_t       MatchingUnit;   // 0
  uint32_t      SequenceNumber; // 0
  char          ClOrdID[20];
  char          Side;           // '1'-Bid, '2'-Ask
  uint32_t      OrderQty;
  uint8_t       NumberOfBitfields; // 0x07
  uint8_t       NewOrderBitfield1; // 0x37 (ClearingFirm,ClearingAccount,Price,OrdType)
  uint8_t       NewOrderBitfield2; // 0x41 (Symbol,Capacity)
  uint8_t       NewOrderBitfield3; // 0x1  (Account)
  uint8_t       NewOrderBitfield4; // 0
  uint8_t       NewOrderBitfield5; // 0
  uint8_t       NewOrderBitfield6; // 0
  uint8_t       NewOrderBitfield7; // 0
  char          ClearingFirm[4];
  char          ClearingAccount[4];
  uint64_t      Price; 
  char          OrdType;        // '1' = Market,'2' = Limit (default),'3' = Stop,'4' = Stop Limit
  char          TimeInForce;    // '3'
  char          Symbol[8];
  char          Capacity;       // 'C','M','F',etc.
  char          Account[16];
} __attribute__((packed));


/* FPGA code: */
/* typedef struct packed { */
/* 	bit     [7:0]   SecID; */
/* 	bit     [7:0]   Version; */
/* 	bit     [7:0]   BidAskMaxSize; */
/* 	bit     [15:0]  AskMaxPrice; */
/* 	bit	[15:0]	BidMinPrice; */
/* } emc_sec_ctx_s; // you should have this struct in any case to configure SecCtx */

struct EkaHwSecCtx {
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t    size;
  uint8_t    verNum;
  uint8_t    lowerBytesOfSecId;
} __attribute__ ((packed));


/* FPGA code: */
/* typedef struct packed { */
/*   bit             SourceFeed; */
/*   bit             IsNotTradable; */
/*   bit             Side; */
/*   bit             isAON; */
/*   bit     [2:0]   CoreID; */
/*   bit             reserved; */
/* } sp4_order_bitparams_t; // !!! keep in byte */

typedef union  {
  uint8_t bits;
  struct  {
    uint8_t Reserved       : 1;
    uint8_t CoreID         : 3;
    uint8_t isAON          : 1;
    uint8_t Side           : 1;
    uint8_t IsNotTradable  : 1; 
    uint8_t SourceFeed     : 1;
  } __attribute__((packed)) bitmap; //must be in 1B resolution
} __attribute__((packed)) EfcFiredOrderBitmap;


/* FPGA code: */
/* typedef struct packed { */
/*   bit     [63:0]  Timestamp; */
/*   bit     [63:0]  Sequence; */
/*   bit     [7:0]   GroupID; */
/*   bit     [31:0]  SecurityID; */
/*   bit     [7:0]   Counter; */
/*   bit     [31:0]  Size; */
/*   bit     [31:0]  Price; */
/*   sp4_order_bitparams_t Bitparams; */
/* } sp4_order_t;  */


struct EfcControllerReport {
  uint8_t fireReason;
} __attribute__((packed));


struct EfcFiredOrder {
  EfcFiredOrderBitmap   attr;
  uint64_t              price;
  uint32_t              size;
  uint8_t               counter;
  uint64_t              securityId;
  uint8_t               groupId;
  uint64_t              sequence;
  uint64_t              timestamp;
} __attribute__((packed));

struct EfcNormalizedFireReport {
  char                pad[5];          // = bit [5*8-1:0] padx8
  EfcControllerReport controllerState; //
  EfcFiredOrder       triggerOrder;    // = sp4_order_t   order_trigger_data 
  uint32_t            securityCtxAddr; // = bit [31:0]    security_context_addr
  EkaHwSecCtx         securityCtx;     // = emc_sec_ctx_s security_context_entry
} __attribute__((packed));

#endif


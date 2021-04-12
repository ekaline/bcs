#ifndef _EHP_CONF_H_
#define _EHP_CONF_H_

#define EhpMaxMsgTypes 8

enum class EhpOpcode : int8_t {
  NOP = 0,    // copy value as is
    MUL100 = 1  // multiply value by 100
};

struct EhpSideEncode {
  uint8_t ask; 
  uint8_t bid;
} __attribute__((packed));

enum class EhpSidePresence : int8_t {
  HARD_BID = 0,     // Side is BID for this message type
    HARD_ASK = 1,   // Side is ASK for this message type
    NONE     = 2,   // no Side at this message type
    EXPLICIT = 3    // Side is derived from a field
};

enum class EhpHwProtocol :  uint8_t {
  NOM = 1,     
  PITCH = 2
};

/**
* Configuration defining how to extract a field
*
*/
struct EhpParseTemplate {
  uint16_t      msgId;       // message Id as appears in the protocol
  EhpOpcode     opcode;      // nop/mul/etc...
  uint8_t       byteOffs_0;  // 
  uint8_t       byteOffs_1;  // 
  uint8_t       byteOffs_2;  // 
  uint8_t       byteOffs_3;  // 
  uint8_t       byteOffs_4;  // 
  uint8_t       byteOffs_5;  // 
  uint8_t       byteOffs_6;  // 
  uint8_t       byteOffs_7;  // 
} __attribute__((packed));



/**
* Configuration defining how to determine Side
*
*/
struct EhpParseTemplateSide {
  uint16_t        msgId;        // message Id as appears in the protocol
  EhpSideEncode   encode;       // byte value encoding Ask and Bid
  EhpSidePresence presence;     // how Side field appearse in the message
  uint8_t         byteOffs;     // offset of the Side field (if relevant)
} __attribute__((packed));

/**
* Set of parsing templates used to extract "relevant" fields
*
*/
struct EhpFieldParams {
  EhpParseTemplate     sequence[EhpMaxMsgTypes];
  EhpParseTemplateSide side[EhpMaxMsgTypes];
  EhpParseTemplate     securityId[EhpMaxMsgTypes];
  EhpParseTemplate     price[EhpMaxMsgTypes];
  EhpParseTemplate     size[EhpMaxMsgTypes];
  EhpParseTemplate     msgId[EhpMaxMsgTypes];
  EhpParseTemplate     msgLen[EhpMaxMsgTypes];
} __attribute__((packed));

/* struct EhpHeaderParams { */
/*   EhpParseTemplate sequence[EhpMaxMsgTypes];; */
/* } __attribute__((packed)); */

#define EhpBlankByte 0xFF

struct EhpBytesBeforeReady {
  uint16_t            msgId;        // message Id as appears in the protocol
  uint8_t             byteOffs;
} __attribute__((packed));

struct EhpProtocolParams {
  EhpBytesBeforeReady bytes4SecLookup[EhpMaxMsgTypes]; // bytes to receive before Sec Lookup
  EhpBytesBeforeReady bytes4Strategy[EhpMaxMsgTypes];  // bytes to receive before Strategy eval
  uint8_t             bytes4StartMsgProc  : 4;         // message bytes 
  uint8_t             msgDeltaSize        : 4;         // extra bytes to offset per message
  uint8_t             pktHdrLen;                       // packet header bytes
  uint8_t             protocolID          : 4;         // 
} __attribute__((packed));


/**
* Configuration parameters for Market Data protocols
*
* - params: define how to parse the market data packet payload
*
* - fields: define how to extract every field for every message type
*/
struct EhpProtocolConf {
  /* EhpHeaderParams   header; */
  EhpFieldParams    fields;
  EhpProtocolParams params;
} __attribute__((packed));

#endif

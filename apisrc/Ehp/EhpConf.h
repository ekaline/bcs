#ifndef _EKA_HW_PARSER_CONF_H_
#define _EKA_HW_PARSER_CONF_H_


enum class EhpMultFactor : uint8_t {
  NOP = 0,    // copy value as is
    MUL100 = 1  // multiply value by 100
};

/**
* Configuration def
*
*/
struct EhpParseTemplate {
  uint16_t      msgId;       // message Id as appears in the protocol
  EhpMultFactor multFactor;  // multiply value by const
  uint8_t       byteOffs_7;  // 
  uint8_t       byteOffs_6;  // 
  uint8_t       byteOffs_5;  // 
  uint8_t       byteOffs_4;  // 
  uint8_t       byteOffs_3;  // 
  uint8_t       byteOffs_2;  // 
  uint8_t       byteOffs_1;  // 
  uint8_t       byteOffs_0;  // 
} __attribute__((packed));

/**
* Set of parsing templates used to extract "relevant" fields
*
*/
struct EhpFieldParams {
  EhpParseTemplateSide side[EhpMaxMsgTypes];
  EhpParseTemplate     securityId[EhpMaxMsgTypes];
  EhpParseTemplate     price[EhpMaxMsgTypes];
  EhpParseTemplate     size[EhpMaxMsgTypes];
  EhpParseTemplate     msgId[EhpMaxMsgTypes];
  EhpParseTemplate     msgLen[EhpMaxMsgTypes];
} __attribute__((packed));


struct EhpProtocolParams {
  EhpBytesBeforeReady bytes4SecLookup;  // bytes to receive before Sec Lookup
  EhpBytesBeforeReady bytes4Strategy;   // bytes to receive before Strategy eval
  uint8_t             msgHdrSize   : 4; // message header bytes
  uint8_t             msgDeltaSize : 4; // extra bytes to offset per message
  uint8_t             pktHdrLen;        // packet header bytes
} __attribute__((packed));


/**
* Configuration parameters for Market Data protocols
*
* - params: define how to parse the market data packet payload
*
* - fields: define how to extract every field for every message type
*/
struct EhpProtocolConf {
  EhpProtocolParams params;
  EhpFieldParams    fields;
} __attribute__((packed));

#endif

#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpMoex.h"
#include "EkaFhBcsSbeParser.h"

EhpBcsMoex::EhpBcsMoex(EkaStrategy *strat)
    : EhpProtocol(strat) {
  EKA_LOG("EhpBcsMoex is created");

  conf.params.protocolID =
      static_cast<decltype(conf.params.protocolID)>(
          EhpHwProtocol::BCSMOEX);
  
  conf.params.pktHdrLen = 39; //pkthdr,incheader,msgheadr,group
  
  conf.params.msgDeltaSize = EhpNoMsgSize;
  conf.params.msgSizeImpl  = sizeof(BcsSbe::BestPricesMsg_MdEntry);

  conf.params.bytes4StartMsgProc = 0;

  conf.fields.sequence[0].msgId = 0; // Not relevant
  conf.fields.sequence[0].opcode = EhpOpcode::NOP;
  conf.fields.sequence[0].byteOffs_0 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_1 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_2 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_3 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_4 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_5 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_6 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_7 = EhpBlankByte;

  //templateID
  conf.fields.hgeneric0[0].msgId = 0; // Not relevant
  conf.fields.hgeneric0[0].opcode = EhpOpcode::NOP;
  conf.fields.hgeneric0[0].byteOffs_0 = 30;
  conf.fields.hgeneric0[0].byteOffs_1 = 31;
  conf.fields.hgeneric0[0].byteOffs_2 = EhpBlankByte;
  conf.fields.hgeneric0[0].byteOffs_3 = EhpBlankByte;
  conf.fields.hgeneric0[0].byteOffs_4 = EhpBlankByte;
  conf.fields.hgeneric0[0].byteOffs_5 = EhpBlankByte;
  conf.fields.hgeneric0[0].byteOffs_6 = EhpBlankByte;
  conf.fields.hgeneric0[0].byteOffs_7 = EhpBlankByte;
  
  
}

int EhpBcsMoex::init() {
  createBestPrice();

  return 0;
}

//pkthdr 16B
//00..03     uint32 seqnum
//04..05     uint16 pktsize
//06..07     uint16 flags pktHdr->pktFlags & 0x8 for increment
//08..15     uint64 sendtime

//incrheader 12B
//16..23     uint64 transacttime
//24..27     int32  sessionid

//msgheader/sbe 8B
//28..29     uint16 blocklength //should be zero for templateid3
//30..31     uint16 templateid //3
//32..33     uint16 schema
//34..35     uint16 version

//group 3B
//36..37     uint16 blocklength //size of each small leg
//38         uint8  numinroup

//struct BestPricesMsg_MdEntry {
//00..07  Decimal9NULL_T MktBidPx;
//08..15  Decimal9NULL_T MktOfferPx;
//16..23  Int64NULL_T MktBidSize;
//24..31  Int64NULL_T MktOfferSize;
//32..35  BoardID_T Board;
//36..47  SecurityID_T Symbol;
//} __attribute__((packed));

int EhpBcsMoex::createBestPrice() {
  uint16_t msgId = EhpNoMsgID; //only msg0 is active
  int msgType = BestPriceMsg;

  conf.params.bytes4Strategy[msgType].msgId = msgId;
  conf.params.bytes4Strategy[msgType].byteOffs = 48;

  conf.params.bytes4SecLookup[msgType].msgId = msgId;
  conf.params.bytes4SecLookup[msgType].byteOffs =
      255; // no lookup

  // NA
  conf.fields.side[msgType].msgId = msgId;
  conf.fields.side[msgType].presence =
      EhpSidePresence::EXPLICIT;
  conf.fields.side[msgType].byteOffs = 255;
  conf.fields.side[msgType].encode.ask = 'S';
  conf.fields.side[msgType].encode.bid = 'B';

  // NA
  conf.fields.isAON[msgType].msgId = msgId;
  conf.fields.isAON[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.isAON[msgType].mask = 0x0;
  conf.fields.isAON[msgType].expected = 0xff;

  // NA
  conf.fields.miscEnable[msgType].msgId = msgId;
  conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.miscEnable[msgType].mask = 0x00; 
  conf.fields.miscEnable[msgType].expected = 0xFF; 


  // BidPrice
  conf.fields.price[msgType].msgId = msgId;
  conf.fields.price[msgType].opcode = EhpOpcode::NOP;
  conf.fields.price[msgType].byteOffs_0 = 0;
  conf.fields.price[msgType].byteOffs_1 = 1;
  conf.fields.price[msgType].byteOffs_2 = 2;
  conf.fields.price[msgType].byteOffs_3 = 3;
  conf.fields.price[msgType].byteOffs_4 = 4;
  conf.fields.price[msgType].byteOffs_5 = 5;
  conf.fields.price[msgType].byteOffs_6 = 6;
  conf.fields.price[msgType].byteOffs_7 = 7;

  // BidSize
  conf.fields.size[msgType].msgId = msgId;
  conf.fields.size[msgType].opcode = EhpOpcode::NOP;
  conf.fields.size[msgType].byteOffs_0 = 16;
  conf.fields.size[msgType].byteOffs_1 = 17;
  conf.fields.size[msgType].byteOffs_2 = 18;
  conf.fields.size[msgType].byteOffs_3 = 19;
  conf.fields.size[msgType].byteOffs_4 = 20;
  conf.fields.size[msgType].byteOffs_5 = 21;
  conf.fields.size[msgType].byteOffs_6 = 22;
  conf.fields.size[msgType].byteOffs_7 = 23;

  // AspPrice
  conf.fields.generic0[msgType].msgId = msgId;
  conf.fields.generic0[msgType].opcode = EhpOpcode::NOP;
  conf.fields.generic0[msgType].byteOffs_0 = 8;
  conf.fields.generic0[msgType].byteOffs_1 = 9;
  conf.fields.generic0[msgType].byteOffs_2 = 10;
  conf.fields.generic0[msgType].byteOffs_3 = 11;
  conf.fields.generic0[msgType].byteOffs_4 = 12;
  conf.fields.generic0[msgType].byteOffs_5 = 13;
  conf.fields.generic0[msgType].byteOffs_6 = 14;
  conf.fields.generic0[msgType].byteOffs_7 = 15;
  
  // AskSize
  conf.fields.generic1[msgType].msgId = msgId;
  conf.fields.generic1[msgType].opcode = EhpOpcode::NOP;
  conf.fields.generic1[msgType].byteOffs_0 = 24;
  conf.fields.generic1[msgType].byteOffs_1 = 25;
  conf.fields.generic1[msgType].byteOffs_2 = 26;
  conf.fields.generic1[msgType].byteOffs_3 = 27;
  conf.fields.generic1[msgType].byteOffs_4 = 28;
  conf.fields.generic1[msgType].byteOffs_5 = 29;
  conf.fields.generic1[msgType].byteOffs_6 = 30;
  conf.fields.generic1[msgType].byteOffs_7 = 31;

  // SecurityIDLow
  conf.fields.securityId[msgType].msgId = msgId;
  conf.fields.securityId[msgType].opcode = EhpOpcode::NOP;
  conf.fields.securityId[msgType].byteOffs_0 = 47;
  conf.fields.securityId[msgType].byteOffs_1 = 46;
  conf.fields.securityId[msgType].byteOffs_2 = 45;
  conf.fields.securityId[msgType].byteOffs_3 = 44;
  conf.fields.securityId[msgType].byteOffs_4 = 43;
  conf.fields.securityId[msgType].byteOffs_5 = 42;
  conf.fields.securityId[msgType].byteOffs_6 = 41;
  conf.fields.securityId[msgType].byteOffs_7 = 40;

  // SecIDHigh(0,1,2,3)+BoardID(4,5,6,7)
  conf.fields.generic2[msgType].msgId = msgId;
  conf.fields.generic2[msgType].opcode = EhpOpcode::NOP;
  conf.fields.generic2[msgType].byteOffs_0 = 39;
  conf.fields.generic2[msgType].byteOffs_1 = 38;
  conf.fields.generic2[msgType].byteOffs_2 = 37;
  conf.fields.generic2[msgType].byteOffs_3 = 36;
  conf.fields.generic2[msgType].byteOffs_4 = 35;
  conf.fields.generic2[msgType].byteOffs_5 = 34;
  conf.fields.generic2[msgType].byteOffs_6 = 33;
  conf.fields.generic2[msgType].byteOffs_7 = 32;
  
  // NA
  conf.fields.msgId[msgType].msgId = msgId;
  conf.fields.msgId[msgType].opcode = EhpOpcode::NOP;
  conf.fields.msgId[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte;

  // no length
  conf.fields.msgLen[msgType].msgId = msgId;
  conf.fields.msgLen[msgType].opcode = EhpOpcode::NOP;
  conf.fields.msgLen[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte;

  return 0;
}

#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpNom.h"


EhpNom::EhpNom(EkaDev* dev) : EhpProtocol(dev) {
  EKA_LOG("EhpNom is created");

  conf.params.protocolID         = static_cast<decltype(conf.params.protocolID)>(EfhFeedVer::kNASDAQ);
  conf.params.pktHdrLen          = sizeof(mold_hdr);
  conf.params.msgDeltaSize       = 2; // msgLen of NOM
  conf.params.bytes4StartMsgProc = 3; // msgLen + msgType

  conf.fields.sequence[0].msgId      = 0; //Not relevant
  conf.fields.sequence[0].opcode     = EhpOpcode::NOP;
  conf.fields.sequence[0].byteOffs_0 = 17;
  conf.fields.sequence[0].byteOffs_1 = 16;
  conf.fields.sequence[0].byteOffs_2 = 15;
  conf.fields.sequence[0].byteOffs_3 = 14;
  conf.fields.sequence[0].byteOffs_4 = 13;
  conf.fields.sequence[0].byteOffs_5 = 12;
  conf.fields.sequence[0].byteOffs_6 = 11;
  conf.fields.sequence[0].byteOffs_7 = 10;
}

int EhpNom::init() {
  createAddOrderShort();
  createAddOrderLong();

  return 0;
}


int EhpNom::createAddOrderShort() {
  uint16_t msgId   = 'a';
  int     msgType = AddOrderShortMsg;

  conf.params.bytes4Strategy[msgType].msgId    = msgId;
  conf.params.bytes4Strategy[msgType].byteOffs = 26+2;

  conf.params.bytes4SecLookup[msgType].msgId    = msgId;
  conf.params.bytes4SecLookup[msgType].byteOffs = 22+2;

  conf.fields.side[msgType].msgId      = msgId;
  conf.fields.side[msgType].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[msgType].byteOffs   = 17+2;
  conf.fields.side[msgType].encode.ask = 'S';
  conf.fields.side[msgType].encode.bid = 'B';

  conf.fields.isAON[msgType].msgId      = msgId;
  conf.fields.isAON[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.isAON[msgType].mask       = 0x0;    //no AON field
  conf.fields.isAON[msgType].expected   = 0xFFFF; //no AON field

  conf.fields.miscEnable[msgType].msgId      = msgId;
  conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.miscEnable[msgType].mask       = 0x0; // no special enable fields, so always enable
  conf.fields.miscEnable[msgType].expected   = 0x0; // no special enable fields, so always enable

  conf.fields.securityId[msgType].msgId      = msgId;
  conf.fields.securityId[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.securityId[msgType].byteOffs_0 = 21+2;
  conf.fields.securityId[msgType].byteOffs_1 = 20+2;
  conf.fields.securityId[msgType].byteOffs_2 = 19+2;
  conf.fields.securityId[msgType].byteOffs_3 = 18+2;
  conf.fields.securityId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.price[msgType].msgId      = msgId;
  conf.fields.price[msgType].opcode     = EhpOpcode::MUL100;
  conf.fields.price[msgType].byteOffs_0 = 23+2;
  conf.fields.price[msgType].byteOffs_1 = 22+2;
  conf.fields.price[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.size[msgType].msgId      = msgId;
  conf.fields.size[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.size[msgType].byteOffs_0 = 25+2;
  conf.fields.size[msgType].byteOffs_1 = 24+2;
  conf.fields.size[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.msgId[msgType].msgId      = msgId;
  conf.fields.msgId[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.msgId[msgType].byteOffs_0 = 2;
  conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.msgLen[msgType].msgId      = msgId;
  conf.fields.msgLen[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.msgLen[msgType].byteOffs_0 = 1;
  conf.fields.msgLen[msgType].byteOffs_1 = 0;
  conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte;

  return 0;

}

int EhpNom::createAddOrderLong() {
  uint16_t msgId   = 'A';
  int     msgType = AddOrderLongMsg;

  conf.params.bytes4Strategy[msgType].msgId    = msgId;
  conf.params.bytes4Strategy[msgType].byteOffs = 30+2;

  conf.params.bytes4SecLookup[msgType].msgId    = msgId;
  conf.params.bytes4SecLookup[msgType].byteOffs = 22+2;

  conf.fields.side[msgType].msgId      = msgId;
  conf.fields.side[msgType].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[msgType].byteOffs   = 17+2;
  conf.fields.side[msgType].encode.ask = 'S';
  conf.fields.side[msgType].encode.bid = 'B';

  conf.fields.isAON[msgType].msgId      = msgId;
  conf.fields.isAON[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.isAON[msgType].mask       = 0x0;    //no AON field
  conf.fields.isAON[msgType].expected   = 0xFFFF; //no AON field

  conf.fields.miscEnable[msgType].msgId      = msgId;
  conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.miscEnable[msgType].mask       = 0x0; // no special enable fields, so always enable
  conf.fields.miscEnable[msgType].expected   = 0x0; // no special enable fields, so always enable

  conf.fields.securityId[msgType].msgId      = msgId;
  conf.fields.securityId[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.securityId[msgType].byteOffs_0 = 21+2;
  conf.fields.securityId[msgType].byteOffs_1 = 20+2;
  conf.fields.securityId[msgType].byteOffs_2 = 19+2;
  conf.fields.securityId[msgType].byteOffs_3 = 18+2;
  conf.fields.securityId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.price[msgType].msgId      = msgId;
  conf.fields.price[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.price[msgType].byteOffs_0 = 25+2;
  conf.fields.price[msgType].byteOffs_1 = 24+2;
  conf.fields.price[msgType].byteOffs_2 = 23+2;
  conf.fields.price[msgType].byteOffs_3 = 22+2;
  conf.fields.price[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.size[msgType].msgId      = msgId;
  conf.fields.size[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.size[msgType].byteOffs_0 = 29+2;
  conf.fields.size[msgType].byteOffs_1 = 28+2;
  conf.fields.size[msgType].byteOffs_2 = 27+2;
  conf.fields.size[msgType].byteOffs_3 = 26+2;
  conf.fields.size[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.msgId[msgType].msgId      = msgId;
  conf.fields.msgId[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.msgId[msgType].byteOffs_0 = 2;
  conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte;

  conf.fields.msgLen[msgType].msgId      = msgId;
  conf.fields.msgLen[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.msgLen[msgType].byteOffs_0 = 1;
  conf.fields.msgLen[msgType].byteOffs_1 = 0;
  conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte;

  return 0;
}

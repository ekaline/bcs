#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpNom.h"


EhpNom::EhpNom(EkaDev* dev) : EhpProtocol(dev) {
  EKA_LOG("EhpNom is created");

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
  conf.params.bytes4Strategy[AddOrderShortMsg].msgId    = 'a';
  conf.params.bytes4Strategy[AddOrderShortMsg].byteOffs = 26+2;

  conf.params.bytes4SecLookup[AddOrderShortMsg].msgId    = 'a';
  conf.params.bytes4SecLookup[AddOrderShortMsg].byteOffs = 22+2;

  conf.fields.side[AddOrderShortMsg].msgId      = 'a';
  conf.fields.side[AddOrderShortMsg].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[AddOrderShortMsg].byteOffs   = 17+2;
  conf.fields.side[AddOrderShortMsg].encode.ask = 'S';
  conf.fields.side[AddOrderShortMsg].encode.bid = 'B';

  conf.fields.securityId[AddOrderShortMsg].msgId      = 'a';
  conf.fields.securityId[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_0 = 21+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_1 = 20+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_2 = 19+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_3 = 18+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.price[AddOrderShortMsg].msgId      = 'a';
  conf.fields.price[AddOrderShortMsg].opcode     = EhpOpcode::MUL100;
  conf.fields.price[AddOrderShortMsg].byteOffs_0 = 23+2;
  conf.fields.price[AddOrderShortMsg].byteOffs_1 = 22+2;
  conf.fields.price[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.size[AddOrderShortMsg].msgId      = 'a';
  conf.fields.size[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.size[AddOrderShortMsg].byteOffs_0 = 25+2;
  conf.fields.size[AddOrderShortMsg].byteOffs_1 = 24+2;
  conf.fields.size[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgId[AddOrderShortMsg].msgId      = 'a';
  conf.fields.msgId[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_0 = 2;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgLen[AddOrderShortMsg].msgId      = 'a';
  conf.fields.msgLen[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_0 = 1;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_1 = 0;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  return 0;

}

int EhpNom::createAddOrderLong() {
  conf.params.bytes4Strategy[AddOrderLongMsg].msgId    = 'A';
  conf.params.bytes4Strategy[AddOrderLongMsg].byteOffs = 30+2;

  conf.params.bytes4SecLookup[AddOrderLongMsg].msgId    = 'A';
  conf.params.bytes4SecLookup[AddOrderLongMsg].byteOffs = 22+2;

  conf.fields.side[AddOrderLongMsg].msgId      = 'A';
  conf.fields.side[AddOrderLongMsg].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[AddOrderLongMsg].byteOffs   = 17+2;
  conf.fields.side[AddOrderLongMsg].encode.ask = 'S';
  conf.fields.side[AddOrderLongMsg].encode.bid = 'B';

  conf.fields.securityId[AddOrderLongMsg].msgId      = 'A';
  conf.fields.securityId[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_0 = 21+2;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_1 = 20+2;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_2 = 19+2;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_3 = 18+2;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.price[AddOrderLongMsg].msgId      = 'A';
  conf.fields.price[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.price[AddOrderLongMsg].byteOffs_0 = 25+2;
  conf.fields.price[AddOrderLongMsg].byteOffs_1 = 24+2;
  conf.fields.price[AddOrderLongMsg].byteOffs_2 = 23+2;
  conf.fields.price[AddOrderLongMsg].byteOffs_3 = 22+2;
  conf.fields.price[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.price[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.price[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.price[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.size[AddOrderLongMsg].msgId      = 'A';
  conf.fields.size[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.size[AddOrderLongMsg].byteOffs_0 = 29+2;
  conf.fields.size[AddOrderLongMsg].byteOffs_1 = 28+2;
  conf.fields.size[AddOrderLongMsg].byteOffs_2 = 27+2;
  conf.fields.size[AddOrderLongMsg].byteOffs_3 = 26+2;
  conf.fields.size[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.size[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.size[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.size[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgId[AddOrderLongMsg].msgId      = 'A';
  conf.fields.msgId[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_0 = 2;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgLen[AddOrderLongMsg].msgId      = 'A';
  conf.fields.msgLen[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_0 = 1;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_1 = 0;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  return 0;
}

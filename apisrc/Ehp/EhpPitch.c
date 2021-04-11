#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpPitch.h"
#include "EkaFhBatsParser.h"

EhpPitch::EhpPitch(EkaDev* dev) : EhpProtocol(dev) {
  EKA_LOG("EhpPitch is created");

  conf.params.protocolID         = EhpHwProtocol::PITCH;
  conf.params.pktHdrLen          = sizeof(batspitch_sequenced_unit_header);
  conf.params.msgDeltaSize       = 0;
  conf.params.bytes4StartMsgProc = 2; // msgLen + msgType 1
 
  conf.fields.sequence[0].msgId      = 0; //Not relevant
  conf.fields.sequence[0].opcode     = EhpOpcode::NOP;
  conf.fields.sequence[0].byteOffs_0 = 7;
  conf.fields.sequence[0].byteOffs_1 = 6;
  conf.fields.sequence[0].byteOffs_2 = 5;
  conf.fields.sequence[0].byteOffs_3 = 4;
  conf.fields.sequence[0].byteOffs_4 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_5 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_6 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_7 = EhpBlankByte;
}

int EhpPitch::init() {
  createAddOrderShort();
  createAddOrderLong();

  return 0;
}


int EhpPitch::createAddOrderShort() {
  uint8_t msgId = 0x22;

  conf.params.bytes4Strategy[AddOrderShortMsg].msgId    = msgId;
  conf.params.bytes4Strategy[AddOrderShortMsg].byteOffs = 25;

  conf.params.bytes4SecLookup[AddOrderShortMsg].msgId    = msgId;
  conf.params.bytes4SecLookup[AddOrderShortMsg].byteOffs = 23;

  conf.fields.side[AddOrderShortMsg].msgId      = msgId;
  conf.fields.side[AddOrderShortMsg].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[AddOrderShortMsg].byteOffs   = 14;
  conf.fields.side[AddOrderShortMsg].encode.ask = 'S';
  conf.fields.side[AddOrderShortMsg].encode.bid = 'B';

  conf.fields.securityId[AddOrderShortMsg].msgId      = msgId;
  conf.fields.securityId[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_0 = 22;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_1 = 21;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_2 = 20;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_3 = 19;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_4 = 18;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_5 = 17;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.price[AddOrderShortMsg].msgId      = msgId;
  conf.fields.price[AddOrderShortMsg].opcode     = EhpOpcode::MUL100;
  conf.fields.price[AddOrderShortMsg].byteOffs_0 = 24;
  conf.fields.price[AddOrderShortMsg].byteOffs_1 = 23;
  conf.fields.price[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.price[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.size[AddOrderShortMsg].msgId      = msgId;
  conf.fields.size[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.size[AddOrderShortMsg].byteOffs_0 = 16;
  conf.fields.size[AddOrderShortMsg].byteOffs_1 = 15;
  conf.fields.size[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.size[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgId[AddOrderShortMsg].msgId      = msgId;
  conf.fields.msgId[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_0 = 1;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgLen[AddOrderShortMsg].msgId      = msgId;
  conf.fields.msgLen[AddOrderShortMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_0 = 0;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_1 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  return 0;

}

int EhpPitch::createAddOrderLong() {
  uint8_t msgId = 0x21;

  conf.params.bytes4Strategy[AddOrderLongMsg].msgId    = msgId;
  conf.params.bytes4Strategy[AddOrderLongMsg].byteOffs = 33;

  conf.params.bytes4SecLookup[AddOrderLongMsg].msgId    = msgId;
  conf.params.bytes4SecLookup[AddOrderLongMsg].byteOffs = 25;

  conf.fields.side[AddOrderLongMsg].msgId      = msgId;
  conf.fields.side[AddOrderLongMsg].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[AddOrderLongMsg].byteOffs   = 14;
  conf.fields.side[AddOrderLongMsg].encode.ask = 'S';
  conf.fields.side[AddOrderLongMsg].encode.bid = 'B';

  conf.fields.securityId[AddOrderLongMsg].msgId      = msgId;
  conf.fields.securityId[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_0 = 24;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_1 = 23;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_2 = 22;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_3 = 21;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_4 = 20;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_5 = 19;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.price[AddOrderLongMsg].msgId      = msgId;
  conf.fields.price[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.price[AddOrderLongMsg].byteOffs_0 = 32;
  conf.fields.price[AddOrderLongMsg].byteOffs_1 = 31;
  conf.fields.price[AddOrderLongMsg].byteOffs_2 = 30;
  conf.fields.price[AddOrderLongMsg].byteOffs_3 = 29;
  conf.fields.price[AddOrderLongMsg].byteOffs_4 = 28;
  conf.fields.price[AddOrderLongMsg].byteOffs_5 = 27;
  conf.fields.price[AddOrderLongMsg].byteOffs_6 = 26;
  conf.fields.price[AddOrderLongMsg].byteOffs_7 = 25;

  conf.fields.size[AddOrderLongMsg].msgId      = msgId;
  conf.fields.size[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.size[AddOrderLongMsg].byteOffs_0 = 18;
  conf.fields.size[AddOrderLongMsg].byteOffs_1 = 17;
  conf.fields.size[AddOrderLongMsg].byteOffs_2 = 16;
  conf.fields.size[AddOrderLongMsg].byteOffs_3 = 15;
  conf.fields.size[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.size[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.size[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.size[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgId[AddOrderLongMsg].msgId      = msgId;
  conf.fields.msgId[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_0 = 1;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  conf.fields.msgLen[AddOrderLongMsg].msgId      = msgId;
  conf.fields.msgLen[AddOrderLongMsg].opcode     = EhpOpcode::NOP;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_0 = 0;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_1 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[AddOrderLongMsg].byteOffs_7 = EhpBlankByte;

  return 0;
}

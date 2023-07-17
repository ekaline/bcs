#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpQED.h"

EhpQED::EhpQED(EkaStrategy *strat) : EhpProtocol(strat) {
  EKA_LOG("EhpQED is created");

  conf.params.protocolID =
      static_cast<decltype(conf.params.protocolID)>(
          EhpHwProtocol::QED);
  conf.params.pktHdrLen = 0;
  conf.params.msgDeltaSize = EhpNoMsgSize;
  conf.params.bytes4StartMsgProc =
      4; // stam - no msgid in protocol

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
}

int EhpQED::init() {
  createQedTrigger();

  return 0;
}

int EhpQED::createQedTrigger() {
  uint16_t msgId =
      EhpNoMsgID; // single type, always matches template0
  int msgType = QedTriggerMsg;

  conf.params.bytes4Strategy[msgType].msgId = msgId;
  conf.params.bytes4Strategy[msgType].byteOffs = 32;

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
  conf.fields.isAON[msgType].byteOffs_0 = 255;
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
  conf.fields.miscEnable[msgType].mask =
      0x0; // no special enable fields, so always enable
  conf.fields.miscEnable[msgType].expected =
      0x0; // no special enable fields, so always enable

  // ds_id
  conf.fields.securityId[msgType].msgId = msgId;
  conf.fields.securityId[msgType].opcode = EhpOpcode::NOP;
  conf.fields.securityId[msgType].byteOffs_0 = 30;
  conf.fields.securityId[msgType].byteOffs_1 = 31;
  conf.fields.securityId[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte;

  // NA
  conf.fields.price[msgType].msgId = msgId;
  conf.fields.price[msgType].opcode = EhpOpcode::NOP;
  conf.fields.price[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_7 = EhpBlankByte;

  // NA
  conf.fields.size[msgType].msgId = msgId;
  conf.fields.size[msgType].opcode = EhpOpcode::NOP;
  conf.fields.size[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_7 = EhpBlankByte;

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

  // no length, defined by conf.params.msgDeltaSize =
  // EhpNoMsgSize
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

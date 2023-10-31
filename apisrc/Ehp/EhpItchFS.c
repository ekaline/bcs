#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpItchFS.h"

EhpItchFS::EhpItchFS(EkaStrategy *strat)
    : EhpProtocol(strat) {
  EKA_LOG("EhpItchFS is created");

  /* conf.params.protocolID = */
  /*     static_cast<decltype(conf.params.protocolID)>( */
  /*         EhpHwProtocol::ITCHFS); // in EhpConf.h */
  /* conf.params.pktHdrLen = sizeof(mold_hdr); */
  /* conf.params.msgDeltaSize = 2;       // msgLen of NOM */
  /* conf.params.bytes4StartMsgProc = 3; // msgLen + msgType */

  /* // Message Count */
  /* conf.fields.sequence[0].msgId = 0; // Not relevant */
  /* conf.fields.sequence[0].opcode = EhpOpcode::NOP; */
  /* conf.fields.sequence[0].byteOffs_0 = 19; */
  /* conf.fields.sequence[0].byteOffs_1 = 18; */
  /* conf.fields.sequence[0].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_7 = EhpBlankByte; */
}

int EhpItchFS::init() {
  createOrderExecuted();
  createTradeNonCross();

  return 0;
}

int EhpItchFS::createOrderExecuted() {
  /* uint16_t msgId = 'E'; */
  /* int msgType = OrderExecutedMsg; */

  /* conf.params.bytes4Strategy[msgType].msgId = msgId; */
  /* //  conf.params.bytes4Strategy[msgType].byteOffs = 3+2; */
  /* //  //stock locate (cant be too small) */
  /* conf.params.bytes4Strategy[msgType].byteOffs = 12; */

  /* conf.params.bytes4SecLookup[msgType].msgId = msgId; */
  /* conf.params.bytes4SecLookup[msgType].byteOffs = */
  /*     255; // no lookup */

  /* // NA */
  /* conf.fields.side[msgType].msgId = msgId; */
  /* conf.fields.side[msgType].presence = */
  /*     EhpSidePresence::EXPLICIT; */
  /* conf.fields.side[msgType].byteOffs = 17 + 2; */
  /* conf.fields.side[msgType].encode.ask = 'S'; */
  /* conf.fields.side[msgType].encode.bid = 'B'; */

  /* // NA */
  /* conf.fields.isAON[msgType].msgId = msgId; */
  /* conf.fields.isAON[msgType].byteOffs_0 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].mask = 0x0; // no AON field */
  /* conf.fields.isAON[msgType].expected = */
  /*     0xFFFF; // no AON field */

  /* // NA */
  /* conf.fields.miscEnable[msgType].msgId = msgId; */
  /* conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].mask = */
  /*     0x0; // no special enable fields, so always enable */
  /* conf.fields.miscEnable[msgType].expected = */
  /*     0x0; // no special enable fields, so always enable */

  /* // LocateID */
  /* conf.fields.securityId[msgType].msgId = msgId; */
  /* conf.fields.securityId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.securityId[msgType].byteOffs_0 = 2 + 2; */
  /* conf.fields.securityId[msgType].byteOffs_1 = 1 + 2; */
  /* conf.fields.securityId[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte; */

  /* // NA */
  /* conf.fields.price[msgType].msgId = msgId; */
  /* conf.fields.price[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.price[msgType].byteOffs_0 = 25 + 2; */
  /* conf.fields.price[msgType].byteOffs_1 = 24 + 2; */
  /* conf.fields.price[msgType].byteOffs_2 = 23 + 2; */
  /* conf.fields.price[msgType].byteOffs_3 = 22 + 2; */
  /* conf.fields.price[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_7 = EhpBlankByte; */

  /* // NA */
  /* conf.fields.size[msgType].msgId = msgId; */
  /* conf.fields.size[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.size[msgType].byteOffs_0 = 29 + 2; */
  /* conf.fields.size[msgType].byteOffs_1 = 28 + 2; */
  /* conf.fields.size[msgType].byteOffs_2 = 27 + 2; */
  /* conf.fields.size[msgType].byteOffs_3 = 26 + 2; */
  /* conf.fields.size[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgId[msgType].msgId = msgId; */
  /* conf.fields.msgId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgId[msgType].byteOffs_0 = 2; */
  /* conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgLen[msgType].msgId = msgId; */
  /* conf.fields.msgLen[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgLen[msgType].byteOffs_0 = 1; */
  /* conf.fields.msgLen[msgType].byteOffs_1 = 0; */
  /* conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte; */

  return 0;
}

int EhpItchFS::createTradeNonCross() {
  /* uint16_t msgId = 'P'; */
  /* int msgType = TradeNonCrossMsg; */

  /* conf.params.bytes4Strategy[msgType].msgId = msgId; */
  /* //  conf.params.bytes4Strategy[msgType].byteOffs = 3+2; */
  /* //  //stock locate (cant be too small) */
  /* conf.params.bytes4Strategy[msgType].byteOffs = 12; */

  /* conf.params.bytes4SecLookup[msgType].msgId = msgId; */
  /* conf.params.bytes4SecLookup[msgType].byteOffs = */
  /*     255; // no lookup */

  /* // NA */
  /* conf.fields.side[msgType].msgId = msgId; */
  /* conf.fields.side[msgType].presence = */
  /*     EhpSidePresence::EXPLICIT; */
  /* conf.fields.side[msgType].byteOffs = 17 + 2; */
  /* conf.fields.side[msgType].encode.ask = 'S'; */
  /* conf.fields.side[msgType].encode.bid = 'B'; */

  /* // NA */
  /* conf.fields.isAON[msgType].msgId = msgId; */
  /* conf.fields.isAON[msgType].byteOffs_0 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].mask = 0x0; // no AON field */
  /* conf.fields.isAON[msgType].expected = */
  /*     0xFFFF; // no AON field */

  /* // NA */
  /* conf.fields.miscEnable[msgType].msgId = msgId; */
  /* conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].mask = */
  /*     0x0; // no special enable fields, so always enable */
  /* conf.fields.miscEnable[msgType].expected = */
  /*     0x0; // no special enable fields, so always enable */

  /* // LocateID */
  /* conf.fields.securityId[msgType].msgId = msgId; */
  /* conf.fields.securityId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.securityId[msgType].byteOffs_0 = 2 + 2; */
  /* conf.fields.securityId[msgType].byteOffs_1 = 1 + 2; */
  /* conf.fields.securityId[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte; */

  /* // NA */
  /* conf.fields.price[msgType].msgId = msgId; */
  /* conf.fields.price[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.price[msgType].byteOffs_0 = 25 + 2; */
  /* conf.fields.price[msgType].byteOffs_1 = 24 + 2; */
  /* conf.fields.price[msgType].byteOffs_2 = 23 + 2; */
  /* conf.fields.price[msgType].byteOffs_3 = 22 + 2; */
  /* conf.fields.price[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_7 = EhpBlankByte; */

  /* // NA */
  /* conf.fields.size[msgType].msgId = msgId; */
  /* conf.fields.size[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.size[msgType].byteOffs_0 = 29 + 2; */
  /* conf.fields.size[msgType].byteOffs_1 = 28 + 2; */
  /* conf.fields.size[msgType].byteOffs_2 = 27 + 2; */
  /* conf.fields.size[msgType].byteOffs_3 = 26 + 2; */
  /* conf.fields.size[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgId[msgType].msgId = msgId; */
  /* conf.fields.msgId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgId[msgType].byteOffs_0 = 2; */
  /* conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgLen[msgType].msgId = msgId; */
  /* conf.fields.msgLen[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgLen[msgType].byteOffs_0 = 1; */
  /* conf.fields.msgLen[msgType].byteOffs_1 = 0; */
  /* conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte; */

  return 0;
}

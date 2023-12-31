#include "EhpPitch.h"
#include "EkaDev.h"
#include "EkaFhBatsParser.h"
#include "EkaP4Strategy.h"

#include "ekaNW.h"

using namespace Bats;

EhpPitch::EhpPitch(EkaStrategy *strat)
    : EhpProtocol(strat) {
  EKA_LOG("EhpPitch is created");

  /* conf.params.protocolID = */
  /*     static_cast<decltype(conf.params.protocolID)>( */
  /*         EhpHwProtocol::PITCH); */
  /* conf.params.pktHdrLen = sizeof(sequenced_unit_header); */
  /* conf.params.msgDeltaSize = 0; */
  /* conf.params.bytes4StartMsgProc = 2; // msgLen + msgType 1 */

  /* conf.fields.sequence[0].msgId = 0; // Not relevant */
  /* conf.fields.sequence[0].opcode = EhpOpcode::NOP; */
  /* conf.fields.sequence[0].byteOffs_0 = 4; */
  /* conf.fields.sequence[0].byteOffs_1 = 5; */
  /* conf.fields.sequence[0].byteOffs_2 = 6; */
  /* conf.fields.sequence[0].byteOffs_3 = 7; */
  /* conf.fields.sequence[0].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.sequence[0].byteOffs_7 = EhpBlankByte; */

  /* fireOnAllAddOrders_ = */
  /*     dynamic_cast<EkaP4Strategy *>(strat_) */
  /*         ->fireOnAllAddOrders_; */

  /* hwFeedVer_ = */
  /*     dynamic_cast<EkaP4Strategy *>(strat_)->feedVer_; */

  /* if (hwFeedVer_ != EfhFeedVer::kCBOE) */
  /*   on_error("hwFeedVer_ %d != EfhFeedVer::kCBOE", */
  /*            (int)hwFeedVer_); */
}

int EhpPitch::init() {
  EKA_LOG(
      "Initializing EhpPitch with fireOnAllAddOrders=%d",
      fireOnAllAddOrders_);
  createAddOrderExpanded();
  if (fireOnAllAddOrders_) {
    createAddOrderShort();
    createAddOrderLong();
  }

  return 0;
}

int EhpPitch::createAddOrderShort() {
  /* uint16_t msgId = 0x22; */
  /* int msgType = AddOrderShortMsg; */

  /* EKA_LOG("creating Ehp for MD Msg type %d: \'%s\'", */
  /*         msgType, msgName[msgType]); */

  /* conf.params.bytes4Strategy[msgType].msgId = msgId; */
  /* conf.params.bytes4Strategy[msgType].byteOffs = 26; */

  /* conf.params.bytes4SecLookup[msgType].msgId = msgId; */
  /* conf.params.bytes4SecLookup[msgType].byteOffs = 23; */

  /* conf.fields.side[msgType].msgId = msgId; */
  /* conf.fields.side[msgType].presence = */
  /*     EhpSidePresence::EXPLICIT; */
  /* conf.fields.side[msgType].byteOffs = 14; */
  /* conf.fields.side[msgType].encode.ask = 'S'; */
  /* conf.fields.side[msgType].encode.bid = 'B'; */

  /* conf.fields.isAON[msgType].msgId = msgId; */
  /* conf.fields.isAON[msgType].byteOffs_0 = 25; */
  /* conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].mask = 0x8; // bit 3 */
  /* conf.fields.isAON[msgType].expected = */
  /*     0x8; // AON is (bit 3 == 1) */

  /* conf.fields.miscEnable[msgType].msgId = msgId; */
  /* conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].mask = */
  /*     0x0; // no special enable fields, so always enable */
  /* conf.fields.miscEnable[msgType].expected = */
  /*     0x0; // no special enable fields, so always enable */

  /* conf.fields.securityId[msgType].msgId = msgId; */
  /* conf.fields.securityId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.securityId[msgType].byteOffs_0 = 22; */
  /* conf.fields.securityId[msgType].byteOffs_1 = 21; */
  /* conf.fields.securityId[msgType].byteOffs_2 = 20; */
  /* conf.fields.securityId[msgType].byteOffs_3 = 19; */
  /* conf.fields.securityId[msgType].byteOffs_4 = 18; */
  /* conf.fields.securityId[msgType].byteOffs_5 = 17; */
  /* conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.price[msgType].msgId = msgId; */
  /* conf.fields.price[msgType].opcode = EhpOpcode::MUL100; */
  /* conf.fields.price[msgType].byteOffs_0 = 23; */
  /* conf.fields.price[msgType].byteOffs_1 = 24; */
  /* conf.fields.price[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.price[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.size[msgType].msgId = msgId; */
  /* conf.fields.size[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.size[msgType].byteOffs_0 = 15; */
  /* conf.fields.size[msgType].byteOffs_1 = 16; */
  /* conf.fields.size[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgId[msgType].msgId = msgId; */
  /* conf.fields.msgId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgId[msgType].byteOffs_0 = 1; */
  /* conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgLen[msgType].msgId = msgId; */
  /* conf.fields.msgLen[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgLen[msgType].byteOffs_0 = 0; */
  /* conf.fields.msgLen[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte; */

  return 0;
}

int EhpPitch::createAddOrderLong() {
  /* uint16_t msgId = 0x21; */
  /* int msgType = AddOrderLongMsg; */

  /* EKA_LOG("creating Ehp for MD Msg type %d: \'%s\'", */
  /*         msgType, msgName[msgType]); */

  /* conf.params.bytes4Strategy[msgType].msgId = msgId; */
  /* conf.params.bytes4Strategy[msgType].byteOffs = 33; */

  /* conf.params.bytes4SecLookup[msgType].msgId = msgId; */
  /* conf.params.bytes4SecLookup[msgType].byteOffs = 25; */

  /* conf.fields.side[msgType].msgId = msgId; */
  /* conf.fields.side[msgType].presence = */
  /*     EhpSidePresence::EXPLICIT; */
  /* conf.fields.side[msgType].byteOffs = 14; */
  /* conf.fields.side[msgType].encode.ask = 'S'; */
  /* conf.fields.side[msgType].encode.bid = 'B'; */

  /* conf.fields.isAON[msgType].msgId = msgId; */
  /* conf.fields.isAON[msgType].byteOffs_0 = 34; */
  /* conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].mask = 0x8; // bit 3 */
  /* conf.fields.isAON[msgType].expected = */
  /*     0x8; // AON is (bit 3 == 1) */

  /* conf.fields.miscEnable[msgType].msgId = msgId; */
  /* conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].mask = */
  /*     0x0; // no special enable fields, so always enable */
  /* conf.fields.miscEnable[msgType].expected = */
  /*     0x0; // no special enable fields, so always enable */

  /* conf.fields.securityId[msgType].msgId = msgId; */
  /* conf.fields.securityId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.securityId[msgType].byteOffs_0 = 24; */
  /* conf.fields.securityId[msgType].byteOffs_1 = 23; */
  /* conf.fields.securityId[msgType].byteOffs_2 = 22; */
  /* conf.fields.securityId[msgType].byteOffs_3 = 21; */
  /* conf.fields.securityId[msgType].byteOffs_4 = 20; */
  /* conf.fields.securityId[msgType].byteOffs_5 = 19; */
  /* conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.price[msgType].msgId = msgId; */
  /* conf.fields.price[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.price[msgType].byteOffs_0 = 25; */
  /* conf.fields.price[msgType].byteOffs_1 = 26; */
  /* conf.fields.price[msgType].byteOffs_2 = 27; */
  /* conf.fields.price[msgType].byteOffs_3 = 28; */
  /* conf.fields.price[msgType].byteOffs_4 = 29; */
  /* conf.fields.price[msgType].byteOffs_5 = 30; */
  /* conf.fields.price[msgType].byteOffs_6 = 31; */
  /* conf.fields.price[msgType].byteOffs_7 = 32; */

  /* conf.fields.size[msgType].msgId = msgId; */
  /* conf.fields.size[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.size[msgType].byteOffs_0 = 15; */
  /* conf.fields.size[msgType].byteOffs_1 = 16; */
  /* conf.fields.size[msgType].byteOffs_2 = 17; */
  /* conf.fields.size[msgType].byteOffs_3 = 18; */
  /* conf.fields.size[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgId[msgType].msgId = msgId; */
  /* conf.fields.msgId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgId[msgType].byteOffs_0 = 1; */
  /* conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgLen[msgType].msgId = msgId; */
  /* conf.fields.msgLen[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgLen[msgType].byteOffs_0 = 0; */
  /* conf.fields.msgLen[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte; */

  return 0;
}

int EhpPitch::createAddOrderExpanded() {
  /* uint16_t msgId = 0x2f; */
  /* int msgType = AddOrderExpandedMsg; */

  /* EKA_LOG("creating Ehp for MD Msg type %d: \'%s\'", */
  /*         msgType, msgName[msgType]); */

  /* conf.params.bytes4Strategy[msgType].msgId = msgId; */
  /* conf.params.bytes4Strategy[msgType].byteOffs = 41; */

  /* conf.params.bytes4SecLookup[msgType].msgId = msgId; */
  /* conf.params.bytes4SecLookup[msgType].byteOffs = 27; */

  /* conf.fields.side[msgType].msgId = msgId; */
  /* conf.fields.side[msgType].presence = */
  /*     EhpSidePresence::EXPLICIT; */
  /* conf.fields.side[msgType].byteOffs = 14; */
  /* conf.fields.side[msgType].encode.ask = 'S'; */
  /* conf.fields.side[msgType].encode.bid = 'B'; */

  /* conf.fields.isAON[msgType].msgId = msgId; */
  /* conf.fields.isAON[msgType].byteOffs_0 = 35; */
  /* conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.isAON[msgType].mask = 0x8; // bit 3 */
  /* conf.fields.isAON[msgType].expected = */
  /*     0x8; // AON is (bit 3 == 1) */

  /* conf.fields.miscEnable[msgType].msgId = msgId; */
  /* conf.fields.miscEnable[msgType].byteOffs_0 = 40; */
  /* conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.miscEnable[msgType].mask = 0xFF; // last Byte */
  /* conf.fields.miscEnable[msgType].expected = */
  /*     (uint32_t)'C'; // Customer Order */

  /* conf.fields.securityId[msgType].msgId = msgId; */
  /* conf.fields.securityId[msgType].opcode = EhpOpcode::NOP; */

  /* conf.fields.securityId[msgType].byteOffs_0 = 24; */
  /* conf.fields.securityId[msgType].byteOffs_1 = 23; */
  /* conf.fields.securityId[msgType].byteOffs_2 = 22; */
  /* conf.fields.securityId[msgType].byteOffs_3 = 21; */
  /* conf.fields.securityId[msgType].byteOffs_4 = 20; */
  /* conf.fields.securityId[msgType].byteOffs_5 = 19; */
  /* conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.price[msgType].msgId = msgId; */
  /* conf.fields.price[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.price[msgType].byteOffs_0 = 27; */
  /* conf.fields.price[msgType].byteOffs_1 = 28; */
  /* conf.fields.price[msgType].byteOffs_2 = 29; */
  /* conf.fields.price[msgType].byteOffs_3 = 30; */
  /* conf.fields.price[msgType].byteOffs_4 = 31; */
  /* conf.fields.price[msgType].byteOffs_5 = 32; */
  /* conf.fields.price[msgType].byteOffs_6 = 33; */
  /* conf.fields.price[msgType].byteOffs_7 = 34; */

  /* conf.fields.size[msgType].msgId = msgId; */
  /* conf.fields.size[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.size[msgType].byteOffs_0 = 15; */
  /* conf.fields.size[msgType].byteOffs_1 = 16; */
  /* conf.fields.size[msgType].byteOffs_2 = 17; */
  /* conf.fields.size[msgType].byteOffs_3 = 18; */
  /* conf.fields.size[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.size[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgId[msgType].msgId = msgId; */
  /* conf.fields.msgId[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgId[msgType].byteOffs_0 = 1; */
  /* conf.fields.msgId[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte; */

  /* conf.fields.msgLen[msgType].msgId = msgId; */
  /* conf.fields.msgLen[msgType].opcode = EhpOpcode::NOP; */
  /* conf.fields.msgLen[msgType].byteOffs_0 = 0; */
  /* conf.fields.msgLen[msgType].byteOffs_1 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte; */
  /* conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte; */

  return 0;
}

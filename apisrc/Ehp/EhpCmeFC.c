#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpCmeFC.h"
//#include "EkaFhBatsParser.h"

//using namespace Bats;

EhpCmeFC::EhpCmeFC(EkaDev* dev) : EhpProtocol(dev) {
  EKA_LOG("EhpCmeFC is created");

  conf.params.protocolID         = static_cast<decltype(conf.params.protocolID)>(EhpHwProtocol::CMEFC);
  conf.params.pktHdrLen          = sizeof(sequenced_unit_header);
  conf.params.msgDeltaSize       = EhpNoMsgSize;
  conf.params.bytes4StartMsgProc = 18; // MHeaderTemplateId
 
  conf.fields.sequence[0].msgId      = 0; //Not relevant
  conf.fields.sequence[0].opcode     = EhpOpcode::NOP;
  conf.fields.sequence[0].byteOffs_0 = 0;
  conf.fields.sequence[0].byteOffs_1 = 1;
  conf.fields.sequence[0].byteOffs_2 = 2;
  conf.fields.sequence[0].byteOffs_3 = 3;
  conf.fields.sequence[0].byteOffs_4 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_5 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_6 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_7 = EhpBlankByte;
}

int EhpCmeFC::init() {
  createFastCancel();


  return 0;
}


int EhpCmeFC::createFastCancel() {
  uint8_t msgId   = 48;
  int     msgType = FastCancelMsg;

  conf.params.bytes4Strategy[msgType].msgId    = msgId;
  conf.params.bytes4Strategy[msgType].byteOffs = 36;

  conf.params.bytes4SecLookup[msgType].msgId    = msgId;
  conf.params.bytes4SecLookup[msgType].byteOffs = 255; //no lookup

  //NA
  conf.fields.side[msgType].msgId      = msgId;
  conf.fields.side[msgType].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[msgType].byteOffs   = 255;
  conf.fields.side[msgType].encode.ask = 'S';
  conf.fields.side[msgType].encode.bid = 'B';

  //NA
  conf.fields.isAON[msgType].msgId      = msgId;
  conf.fields.isAON[msgType].byteOffs_0 = 255;
  conf.fields.isAON[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.isAON[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.isAON[msgType].mask       = 0x0; 
  conf.fields.isAON[msgType].expected   = 0xff; 

  //NA
  conf.fields.miscEnable[msgType].msgId      = msgId;
  conf.fields.miscEnable[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.miscEnable[msgType].mask       = 0x0; // no special enable fields, so always enable
  conf.fields.miscEnable[msgType].expected   = 0x0; // no special enable fields, so always enable

  //NA
  conf.fields.securityId[msgType].msgId      = msgId;
  conf.fields.securityId[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.securityId[msgType].byteOffs_0 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[msgType].byteOffs_7 = EhpBlankByte;

  //GNumInGroup
  conf.fields.price[msgType].msgId      = msgId;
  conf.fields.price[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.price[msgType].byteOffs_0 = 35;
  conf.fields.price[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.price[msgType].byteOffs_7 = EhpBlankByte;

  //MHeaderSize
  conf.fields.size[msgType].msgId      = msgId;
  conf.fields.size[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.size[msgType].byteOffs_0 = 12;
  conf.fields.size[msgType].byteOffs_1 = 13;
  conf.fields.size[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.size[msgType].byteOffs_7 = EhpBlankByte;

  //MHeaderTemplateId
  conf.fields.msgId[msgType].msgId      = msgId;
  conf.fields.msgId[msgType].opcode     = EhpOpcode::NOP;
  conf.fields.msgId[msgType].byteOffs_0 = 16;
  conf.fields.msgId[msgType].byteOffs_1 = 17;
  conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte;


  //no length, defined by conf.params.msgDeltaSize = EhpNoMsgSize
  conf.fields.msgLen[msgType].msgId      = msgId;
  conf.fields.msgLen[msgType].opcode     = EhpOpcode::NOP;
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


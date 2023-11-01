#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpEur.h"
#include "EOBILayouts.h"

EhpEur::EhpEur(EkaStrategy *strat) : EhpProtocol(strat) {
  EKA_LOG("EhpEur is created");

  //MessageHeaderCompT
	/*0-1 bit	[15:0]	BodyLen; */
	/*2-3 bit	[15:0]	TemplateID; */
	/*4-7 bit	[31:0]	MsgSeqNum; */

  //PacketHeaderT (includes MessageHeaderCompT)
	/*8-11 bit	[31:0]	ApplSeqNum; */
	/*12-15 bit	[31:0]	MarketSegmentID; */
	/*16 bit	[7:0]	PartitionID; */
	/*17 bit	[7:0]	CompletionIndicator; */
	/*18 bit	[7:0]	ApplSeqResetIndicator; */
	/*19-23 bit	[39:0]	Pad5; */
	/*24-31 bit	[63:0]	TransactTime; */
	
  conf.params.protocolID =
      static_cast<decltype(conf.params.protocolID)>(
          EhpHwProtocol::EURP4);
  conf.params.pktHdrLen = 32; //sizeof(PacketHeaderT); not packet, not sure
  conf.params.msgDeltaSize = 0;
  conf.params.bytes4StartMsgProc = 4; //len+tid

  //  ApplSeqNum
  conf.fields.sequence[0].msgId = 0; // Not relevant
  conf.fields.sequence[0].opcode = EhpOpcode::NOP;
  conf.fields.sequence[0].byteOffs_0 = 8;
  conf.fields.sequence[0].byteOffs_1 = 9;
  conf.fields.sequence[0].byteOffs_2 = 10;
  conf.fields.sequence[0].byteOffs_3 = 11;
  conf.fields.sequence[0].byteOffs_4 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_5 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_6 = EhpBlankByte;
  conf.fields.sequence[0].byteOffs_7 = EhpBlankByte;

  // TransactTime
  conf.fields.hgeneric0[msgType].msgId = 0; // Not relevant
  conf.fields.hgeneric0[msgType].opcode = EhpOpcode::NOP;
  conf.fields.hgeneric0[msgType].byteOffs_0 = 8 + 16;
  conf.fields.hgeneric0[msgType].byteOffs_1 = 8 + 17;
  conf.fields.hgeneric0[msgType].byteOffs_2 = 8 + 18;
  conf.fields.hgeneric0[msgType].byteOffs_3 = 8 + 19;
  conf.fields.hgeneric0[msgType].byteOffs_4 = 8 + 20;
  conf.fields.hgeneric0[msgType].byteOffs_5 = 8 + 21;
  conf.fields.hgeneric0[msgType].byteOffs_6 = 8 + 22;
  conf.fields.hgeneric0[msgType].byteOffs_7 = 8 + 23;
  
}

int EhpEur::init() {
  createExecutionSummary();

  return 0;
}

int EhpEur::createExecutionSummary() {
  uint16_t msgId = 13202; //EUREX_EOBI_EXECUTION_SUMMARY
  int msgType = ExecutionSummaryMsg;

  conf.params.bytes4Strategy[msgType].msgId = msgId;
  conf.params.bytes4Strategy[msgType].byteOffs = 8 + 56; //end of price

  conf.params.bytes4SecLookup[msgType].msgId = msgId;
  conf.params.bytes4SecLookup[msgType].byteOffs = 8 + 8; //end of secid

  // RequestTime
  conf.fields.generic0[msgType].msgId = msgId;
  conf.fields.generic0[msgType].opcode = EhpOpcode::NOP;
  conf.fields.generic0[msgType].byteOffs_0 = 8 + 16;
  conf.fields.generic0[msgType].byteOffs_1 = 8 + 17;
  conf.fields.generic0[msgType].byteOffs_2 = 8 + 18;
  conf.fields.generic0[msgType].byteOffs_3 = 8 + 19;
  conf.fields.generic0[msgType].byteOffs_4 = 8 + 20;
  conf.fields.generic0[msgType].byteOffs_5 = 8 + 21;
  conf.fields.generic0[msgType].byteOffs_6 = 8 + 22;
  conf.fields.generic0[msgType].byteOffs_7 = 8 + 23;
  
  // AggressorSide
  conf.fields.generic1[msgType].msgId = msgId;
  conf.fields.generic1[msgType].opcode = EhpOpcode::NOP;
  conf.fields.generic1[msgType].byteOffs_0 = 8 + 40;
  conf.fields.generic1[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.generic1[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.generic1[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.generic1[msgType].byteOffs_4 = EhpBlankByte; 
  conf.fields.generic1[msgType].byteOffs_5 = EhpBlankByte; 
  conf.fields.generic1[msgType].byteOffs_6 = EhpBlankByte; 
  conf.fields.generic1[msgType].byteOffs_7 = EhpBlankByte; 
  
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
  conf.fields.miscEnable[msgType].byteOffs_0 = 255
  conf.fields.miscEnable[msgType].byteOffs_1 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.miscEnable[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.miscEnable[msgType].mask = 0x0;
  conf.fields.miscEnable[msgType].expected = 0xff; 

  // securityId
  conf.fields.securityId[msgType].msgId = msgId;
  conf.fields.securityId[msgType].opcode = EhpOpcode::NOP;
  conf.fields.securityId[msgType].byteOffs_0 = 8 + 0;
  conf.fields.securityId[msgType].byteOffs_1 = 8 + 1;
  conf.fields.securityId[msgType].byteOffs_2 = 8 + 2;
  conf.fields.securityId[msgType].byteOffs_3 = 8 + 3;
  conf.fields.securityId[msgType].byteOffs_4 = 8 + 4;
  conf.fields.securityId[msgType].byteOffs_5 = 8 + 5;
  conf.fields.securityId[msgType].byteOffs_6 = 8 + 6;
  conf.fields.securityId[msgType].byteOffs_7 = 8 + 7;

  // price
  conf.fields.price[msgType].msgId = msgId;
  conf.fields.price[msgType].opcode = EhpOpcode::NOP;
  conf.fields.price[msgType].byteOffs_0 = 8 + 48;
  conf.fields.price[msgType].byteOffs_1 = 8 + 49;
  conf.fields.price[msgType].byteOffs_2 = 8 + 50;
  conf.fields.price[msgType].byteOffs_3 = 8 + 51;
  conf.fields.price[msgType].byteOffs_4 = 8 + 52;
  conf.fields.price[msgType].byteOffs_5 = 8 + 53;
  conf.fields.price[msgType].byteOffs_6 = 8 + 54;
  conf.fields.price[msgType].byteOffs_7 = 8 + 55;

  // size
  conf.fields.size[msgType].msgId = msgId;
  conf.fields.size[msgType].opcode = EhpOpcode::NOP;
  conf.fields.size[msgType].byteOffs_0 = 8 + 32;
  conf.fields.size[msgType].byteOffs_1 = 8 + 33;
  conf.fields.size[msgType].byteOffs_2 = 8 + 34;
  conf.fields.size[msgType].byteOffs_3 = 8 + 35;
  conf.fields.size[msgType].byteOffs_4 = 8 + 36;
  conf.fields.size[msgType].byteOffs_5 = 8 + 37;
  conf.fields.size[msgType].byteOffs_6 = 8 + 38;
  conf.fields.size[msgType].byteOffs_7 = 8 + 39;

  // msgId
  conf.fields.msgId[msgType].msgId = msgId;
  conf.fields.msgId[msgType].opcode = EhpOpcode::NOP;
  conf.fields.msgId[msgType].byteOffs_0 = 2;
  conf.fields.msgId[msgType].byteOffs_1 = 3;
  conf.fields.msgId[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[msgType].byteOffs_7 = EhpBlankByte;

  // msgLen
  conf.fields.msgLen[msgType].msgId = msgId;
  conf.fields.msgLen[msgType].opcode = EhpOpcode::NOP;
  conf.fields.msgLen[msgType].byteOffs_0 = 0;
  conf.fields.msgLen[msgType].byteOffs_1 = 1;
  conf.fields.msgLen[msgType].byteOffs_2 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_3 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_4 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_5 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_6 = EhpBlankByte;
  conf.fields.msgLen[msgType].byteOffs_7 = EhpBlankByte;

  return 0;
}

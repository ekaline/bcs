#include "EkaDev.h"
#include "ekaNW.h"

#include "EhpNom.h"


EhpNom::EhpNom(EkaDev* dev) : EhpProtocol(dev) {
  EKA_LOG("EhpNom is created");

  conf.params.pktHdrLen    = sizeof(mold_hdr);
  conf.params.msgDeltaSize = 2; // msgLen of NOM
  conf.params.msgDeltaSize = 3; // msgLen + msgType
}

int EhpNom::init() {
  createAddOrderShort();
  createAddOrderLong();

  return 0;
}


int EhpNom::createAddOrderShort() {
  conf.params.bytes4Strategy[0].msgId    = 'a';
  //  conf.params.bytes4Strategy[AddOrderShortMsg].msgId    = 'a';
  conf.params.bytes4Strategy[AddOrderShortMsg].byteOffs = 26+2;

  conf.fields.side[AddOrderShortMsg].msgId = 'a';
  conf.fields.side[AddOrderShortMsg].presence   = EhpSidePresence::EXPLICIT;
  conf.fields.side[AddOrderShortMsg].byteOffs   = 0; //TBD
  conf.fields.side[AddOrderShortMsg].encode.ask = 'S';
  conf.fields.side[AddOrderShortMsg].encode.bid = 'B';

  
  conf.fields.msgId[AddOrderShortMsg].msgId      = 'a';
  conf.fields.msgId[AddOrderShortMsg].mult = EhpMultFactor::NOP;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_0 = 2;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_1 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_2 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_3 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.msgId[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  
  conf.fields.securityId[AddOrderShortMsg].msgId      = 'a';
  conf.fields.securityId[AddOrderShortMsg].mult = EhpMultFactor::NOP;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_0 = 21+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_1 = 20+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_2 = 19+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_3 = 18+2;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_4 = EhpBlankByte;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_5 = EhpBlankByte;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_6 = EhpBlankByte;
  conf.fields.securityId[AddOrderShortMsg].byteOffs_7 = EhpBlankByte;

  return 0;

}

int EhpNom::createAddOrderLong() {
  return 0;
}

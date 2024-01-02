#ifndef _EKA_UDP_TX_SESS_H_
#define _EKA_UDP_TX_SESS_H_

#include <inttypes.h>
#include <string.h>

class EkaDev;

class EkaUdpTxSess {
 public:
  EkaUdpTxSess(EkaDev*   pEkaDev, 
	       EkaCoreId coreId,int8_t sessId,
	       eka_ether_addr macSa, eka_ether_addr macDa,
	       eka_in_addr_t srcIp, eka_in_addr_t dstIp, 
	       uint16_t srcPort, uint16_t dstPort
	       ) {
    dev  = pEkaDev;
    if (dev == NULL) on_error("dev == NULL");

    sessId_ = sessId;
    coreId_ = coreId;
    memcpy(macSa_,&macSa,6);
    memcpy(macDa_,&macDa,6);
    srcIp_  = srcIp;
    dstIp_  = dstIp;
    srcPort_ = srcPort;
    dstPort_ = dstPort;

    EKA_LOG("New UDP TX Sess %d on core %d: %s %s:%u --> %s %s:%u",
	    sessId_,coreId_,
	    EKA_MAC2STR(macSa_),EKA_IP2STR(srcIp_),srcPort_,
	    EKA_MAC2STR(macDa_),EKA_IP2STR(dstIp_),dstPort_
	    );
  }

  // bool myParams(EkaCoreId _coreId, uint32_t _ip,uint16_t _port) {
  //   return (coreId == _coreId && ip == _ip && port == _port);
  // }


  int8_t         sessId_ = -1;
  EkaCoreId      coreId_ = -1;
  uint8_t        macSa_[6] = {};
  uint8_t        macDa_[6] = {};
  eka_in_addr_t  srcIp_ = 0;
  eka_in_addr_t  dstIp_ = 0;
  uint16_t       srcPort_ = 0;
  uint16_t       dstPort_ = 0;

  ExcUdpTxConnHandle getConnHandle() {
    return coreId_ * 128 + sessId_;
  }
  
 private:
  EkaDev* dev = NULL;
};

#endif

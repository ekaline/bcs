#ifndef _EKA_UDP_TX_SESS_H_
#define _EKA_UDP_TX_SESS_H_

#include <inttypes.h>
#include <string.h>

class EkaDev;

class EkaUdpTxSess {
 public:
  EkaUdpTxSess(EkaDev*   pEkaDev, 
	       EkaCoreId coreId,int8_t sessId,
	       eka_ether_addr srcMac, eka_ether_addr dstMac,
	       eka_in_addr_t srcIp, eka_in_addr_t dstIp, 
	       uint16_t srcUpdPort, uint16_t dstUpdPort
	       ) {
    dev  = pEkaDev;
    if (dev == NULL) on_error("dev == NULL");

    sessId_ = sessId;
    coreId_ = coreId;
    memcpy(srcMac_,&srcMac,6);
    memcpy(dstMac_,&dstMac,6);
    srcIp_  = srcIp;
    dstIp_  = dstIp;
    srcUpdPort_ = srcUpdPort;
    dstUpdPort_ = dstUpdPort;

    EKA_LOG("New UDP TX Sess %d on core %d: %s %s:%u --> %s %s:%u",
	    sessId_,coreId_,
	    EKA_MAC2STR(srcMac_),EKA_IP2STR(srcIp_),srcUpdPort_,
	    EKA_MAC2STR(dstMac_),EKA_IP2STR(dstIp_),dstUpdPort_
	    );
  }

  // bool myParams(EkaCoreId _coreId, uint32_t _ip,uint16_t _port) {
  //   return (coreId == _coreId && ip == _ip && port == _port);
  // }


  int8_t         sessId_ = -1;
  EkaCoreId      coreId_ = -1;
  uint8_t        srcMac_[6] = {};
  uint8_t        dstMac_[6] = {};
  eka_in_addr_t  srcIp_ = 0;
  eka_in_addr_t  dstIp_ = 0;
  uint16_t       srcUpdPort_ = 0;
  uint16_t       dstUpdPort_ = 0;

  ExcUdpTxConnHandle getConnHandle() {
    return coreId_ * 128 + sessId_;
  }
  
 private:
  EkaDev* dev = NULL;
};

#endif

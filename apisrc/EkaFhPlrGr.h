#ifndef _EKA_FH_PLR_GR_H_
#define _EKA_FH_PLR_GR_H_

#include <chrono>

#include "EkaFhGroup.h"
#include "EkaFhTobBook.h"

class EkaFhPlrGr : public EkaFhGroup{
public:
  virtual               ~EkaFhPlrGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 const unsigned char*   m,
				 uint64_t         sequence,
				 EkaFhMode        op,
				 std::chrono::high_resolution_clock::time_point startTime={});

  int                   bookInit();

  int                  subscribeStaticSecurity(uint64_t        securityId, 
					       EfhSecurityType efhSecurityType,
					       EfhSecUserData  efhSecUserData,
					       uint64_t        opaqueAttrA,
					       uint64_t        opaqueAttrB) {
    if (book == NULL) on_error("%s:%u book == NULL",EKA_EXCH_DECODE(exch),id);
    book->subscribeSecurity(securityId, 
			    efhSecurityType,
			    efhSecUserData,
			    opaqueAttrA,
			    opaqueAttrB);
    return 0;
  }

  bool                  processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      uint             msgInPkt, 
				      uint64_t         seq,
				      std::chrono::high_resolution_clock::time_point start={});

  void                 pushUdpPkt2Q(const uint8_t* pkt, 
				    uint           msgInPkt, 
				    uint64_t       sequence);

  int    closeSnapshotGap(EfhCtx*              pEfhCtx, 
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t          startSeq,
			  uint64_t          endSeq);

  int    closeIncrementalGap(EfhCtx*           pEfhCtx, 
			     const EfhRunCtx* pEfhRunCtx, 
			     uint64_t          startSeq,
			     uint64_t          endSeq);
  int printConfig() {
    EKA_LOG("%s:%u : productMask: 0x%x"
	    "MCAST: %s:%u, "
	    "Refresh Tcp: %s:%u, "
	    "Refresh Udp: %s:%u, "
	    "ReTrans Tcp: %s:%u, "
	    "ReTrans Udp: %s:%u, "

	    "Source ID: %s "
	    "Channel: %d, "
	    "connectRetryDelayTime: %d",
	    EKA_EXCH_DECODE(exch),id,
	    productMask,
	    EKA_IP2STR(mcast_ip),   mcast_port,
	    EKA_IP2STR(refreshTcpIp),be16toh(refreshTcpPort),
	    EKA_IP2STR(refreshUdpIp),be16toh(refreshUdpPort),
	    EKA_IP2STR(retransTcpIp),be16toh(retransTcpPort),
	    EKA_IP2STR(retransUdpIp),be16toh(retransUdpPort),
	    std::string(sourceId,sizeof(sourceId)).c_str(),
	    channelId,
	    connectRetryDelayTime
	    );
    return 0;
  }


private:
  int    sendMdCb(const EfhRunCtx* pEfhRunCtx,
		  const uint8_t* m,
		  int            gr,
		  uint64_t       sequence,
		  uint64_t       ts)
      {return 0;}



  
  /* ##################################################################### */

public:
  char     sourceId[10]     = {};
  int      channelId   = 0;
  uint32_t refreshTcpIp     = 0;
  uint16_t refreshTcpPort   = 0;
  uint32_t retransTcpIp     = 0;
  uint16_t retransTcpPort   = 0;
  uint32_t refreshUdpIp     = 0;
  uint16_t refreshUdpPort   = 0;
  uint32_t retransUdpIp     = 0;
  uint16_t retransUdpPort   = 0;

  static const uint   SCALE          = 22;
  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurity  = EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>;
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,
				   EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>,
				   SecurityIdT,
				   PriceT,
				   SizeT>;

  FhBook*   book = NULL;

private:

};
#endif

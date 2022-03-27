#ifndef _EKA_FH_BATS_GR_H_
#define _EKA_FH_BATS_GR_H_

#include <chrono>
#include <unordered_map>

#include "EkaFhGroup.h"
#include "EkaFhFullBook.h"

class EkaFhBatsGr : public EkaFhGroup{
 public:
  virtual ~EkaFhBatsGr() {};

  bool parseMsg(const EfhRunCtx* pEfhRunCtx,
		const unsigned char*   m,
		uint64_t         sequence,
		EkaFhMode        op,
		std::chrono::high_resolution_clock::time_point startTime={});

  int bookInit();

  int subscribeStaticSecurity(uint64_t        securityId, 
			      EfhSecurityType efhSecurityType,
			      EfhSecUserData  efhSecUserData,
			      uint64_t        opaqueAttrA,
			      uint64_t        opaqueAttrB) {
    if (!book)
      on_error("%s:%u !book",EKA_EXCH_DECODE(exch),id);
    
    book->subscribeSecurity(securityId, 
			    efhSecurityType,
			    efhSecUserData,
			    opaqueAttrA,
			    opaqueAttrB);
    return 0;
  }

  bool processUdpPkt(const EfhRunCtx* pEfhRunCtx,
		     const uint8_t*   pkt, 
		     uint             msgInPkt, 
		     uint64_t         seq,
		     std::chrono::high_resolution_clock::time_point start={});
  
  void pushUdpPkt2Q(const uint8_t* pkt, 
		    uint           msgInPkt, 
		    uint64_t       sequence);
  
  
  int closeSnapshotGap(EfhCtx*              pEfhCtx, 
		       const EfhRunCtx* pEfhRunCtx, 
		       uint64_t          startSeq,
		       uint64_t          endSeq);
  
  int closeIncrementalGap(EfhCtx*           pEfhCtx, 
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t          startSeq,
			  uint64_t          endSeq);

  int printConfig() {
    EKA_LOG("%s:%u : "
	    "productMask: \'%s\' (0x%x) "
	    
	    "MCAST: %s:%u, "
	    
	    "Spin Tcp: %s:%u, "
	    "Spin User:Pswd: \'%s\':\'%s\', "
	    "Spin SessionSubID: \'%s\', "

	    "GRP Tcp: %s:%u, "
	    "GRP Udp: %s:%u, "
	    "GRP SessionSubID: \'%s\', "
	    
	    "BatsUnit: %u, "

	    "GRP User:Pswd: \'%s\':\'%s\', "

	    "connectRetryDelayTime: %d",
	    EKA_EXCH_DECODE(exch),id,
	    lookupProductName(productMask), productMask,
	    
	    EKA_IP2STR(mcast_ip),   mcast_port,
	    EKA_IP2STR(snapshot_ip),be16toh(snapshot_port),
	    
	    std::string(auth_user,sizeof(auth_user)).c_str(),
	    std::string(auth_passwd,sizeof(auth_passwd)).c_str(),
	    std::string(sessionSubID,sizeof(sessionSubID)).c_str(),
	    
	    EKA_IP2STR(grpIp),      be16toh(grpPort),
	    EKA_IP2STR(recovery_ip),be16toh(recovery_port),

	    std::string(grpSessionSubID,sizeof(grpSessionSubID)).c_str(),
	    batsUnit,
	    
	    std::string(grpUser,  sizeof(grpUser)  ).c_str(),
	    std::string(grpPasswd,sizeof(grpPasswd)).c_str(),

	    connectRetryDelayTime
	    );
    return 0;
  }
private:

  
  /* ##################################################################### */

public:
  char                  sessionSubID[4] = {};  // for BATS Spin
  uint8_t               batsUnit = 0;

  char                  grpSessionSubID[4] = {'0','5','8','7'};  // C1 default
  uint32_t              grpIp         = 0x667289aa;              // C1 default "170.137.114.102"
  uint16_t              grpPort       = 0x6e42;                  // C1 default be16toh(17006)
  char                  grpUser[4]    = {'G','T','S','S'};       // C1 default
  char                  grpPasswd[10] = {'e','b','3','g','t','s','s',' ',' ',' '}; // C1 default
  bool                  grpSet        = false;

  static const uint   SCALE          = (const uint) 22;
  static const uint   SEC_HASH_SCALE = 17;

  using AuctionIdT = uint64_t;
  using SecurityIdT = uint64_t;
  using OrderIdT    = uint64_t;
  using PriceT      = int64_t;
  using SizeT       = uint32_t;

  using FhPlevel     = EkaFhPlevel      <                                                   PriceT, SizeT>;
  using FhSecurity   = EkaFhFbSecurity  <EkaFhPlevel<PriceT, SizeT>, SecurityIdT, OrderIdT, PriceT, SizeT>;
  using FhOrder      = EkaFhOrder       <EkaFhPlevel<PriceT, SizeT>,              OrderIdT,         SizeT>;

  using FhBook      = EkaFhFullBook<
    SCALE,SEC_HASH_SCALE,
    EkaFhFbSecurity  <EkaFhPlevel<PriceT, SizeT>,SecurityIdT, OrderIdT, PriceT, SizeT>,
    EkaFhPlevel      <PriceT, SizeT>,
    EkaFhOrder       <EkaFhPlevel<PriceT, SizeT>,OrderIdT,SizeT>,
    SecurityIdT, OrderIdT, PriceT, SizeT>;

  FhBook*   book = NULL;
  std::unordered_map<AuctionIdT,SecurityIdT> auctionMap;

private:
  int sendMdCb(const EfhRunCtx* pEfhRunCtx,
	       const uint8_t* m,
	       int            gr,
	       uint64_t       sequence,
	       uint64_t       ts);
  
  bool process_Definition(const EfhRunCtx* pEfhRunCtx,
			  const unsigned char* m,
			  uint64_t sequence,
			  EkaFhMode op);
  
  template <class SecurityT, class OrderMsgT>
  SecurityT* process_AddOrderShort(const uint8_t* m);
  
  template <class SecurityT, class OrderMsgT>
  SecurityT* process_AddOrderLong(const uint8_t* m);
   
  template <class SecurityT, class OrderMsgT>
  SecurityT* process_AddOrderExpanded(const uint8_t* m);

   template <class SecurityT, class OrderMsgT>
  SecurityT* process_OrderModifyShort(const uint8_t* m);
  
  template <class SecurityT, class OrderMsgT>
  SecurityT* process_OrderModifyLong(const uint8_t* m);

  template <class SecurityT, class OrderMsgT>
  SecurityT* process_TradeShort(const EfhRunCtx* pEfhRunCtx,
				uint64_t sequence,
				uint64_t msg_timestamp,
				const uint8_t* m);
  
  template <class SecurityT, class OrderMsgT>
  SecurityT* process_TradeLong(const EfhRunCtx* pEfhRunCtx,
			       uint64_t sequence,
			       uint64_t msg_timestamp,
			       const uint8_t* m);
  
  template <class SecurityT, class OrderMsgT>
  SecurityT* process_TradeExpanded(const EfhRunCtx* pEfhRunCtx,
				   uint64_t sequence,
				   uint64_t msg_timestamp,
				   const uint8_t* m);

};
#endif

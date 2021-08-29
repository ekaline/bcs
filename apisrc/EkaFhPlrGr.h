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

  int                   bookInit()
  {return 0;}

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
				      std::chrono::high_resolution_clock::time_point start={})
      {return false;}


  void                 pushUdpPkt2Q(const uint8_t* pkt, 
				    uint           msgInPkt, 
				    uint64_t       sequence)
  {}


  int    closeSnapshotGap(EfhCtx*              pEfhCtx, 
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t          startSeq,
			  uint64_t          endSeq)
      {return 0;}


  int    closeIncrementalGap(EfhCtx*           pEfhCtx, 
			     const EfhRunCtx* pEfhRunCtx, 
			     uint64_t          startSeq,
			     uint64_t          endSeq)
      {return 0;}


private:
  int    sendMdCb(const EfhRunCtx* pEfhRunCtx,
		  const uint8_t* m,
		  int            gr,
		  uint64_t       sequence,
		  uint64_t       ts)
      {return 0;}



  
  /* ##################################################################### */

public:
  char                  sessionSubID[4] = {};  // for PLR Spin
  uint8_t               plrUnit = 0;

  char                  grpSessionSubID[4] = {'0','5','8','7'};  // C1 default
  uint32_t              grpIp         = 0x667289aa;              // C1 default "170.137.114.102"
  uint16_t              grpPort       = 0x6e42;                  // C1 default be16toh(17006)
  char                  grpUser[4]    = {'G','T','S','S'};       // C1 default
  char                  grpPasswd[10] = {'e','b','3','g','t','s','s',' ',' ',' '}; // C1 default
  bool                  grpSet        = false;

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

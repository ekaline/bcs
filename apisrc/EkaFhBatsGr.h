#ifndef _EKA_FH_BATS_GR_H_
#define _EKA_FH_BATS_GR_H_

#include <chrono>

#include "EkaFhGroup.h"
#include "EkaFhFullBook.h"

class EkaFhBatsGr : public EkaFhGroup{
 public:
  virtual               ~EkaFhBatsGr() {};

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

private:
  int    sendMdCb(const EfhRunCtx* pEfhRunCtx,
		  const uint8_t* m,
		  int            gr,
		  uint64_t       sequence,
		  uint64_t       ts);


  
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

  using SecurityIdT = uint64_t;
  using OrderIdT    = uint64_t;
  using PriceT      = uint32_t;
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

private:
  /* ------------------------------------------------ */
  // converting 6 char CBOE symbol to uint64_t
  inline SecurityIdT symbol2secId(const char* s) {
    return be64toh(*(uint64_t*)(s - 2)) & 0x0000ffffffffffff;
  }
  inline SecurityIdT expSymbol2secId(const char* s) {
    if (s[6] != ' ' || s[7] != ' ')
      on_error("ADD_ORDER_EXPANDED message with \'%c%c%c%c%c%c%c%c\' symbol (longer than 6 chars) not supported",
	       s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7]);
    return be64toh(*(uint64_t*)(s - 2)) & 0x0000ffffffffffff;
  }
};
#endif

#ifndef _EKA_FH_MIAX_GR_H_
#define _EKA_FH_MIAX_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhTobBook.h"

 /* ##################################################################### */

class Underlying {
 public:
  Underlying(const char* _name) {
    strncpy(name,_name,sizeof name);
    tradeStatus = EfhTradeStatus::kNormal;
  }
  
  EfhSymbol name;
  EfhTradeStatus tradeStatus = EfhTradeStatus::kNormal;
};
 /* ##################################################################### */

class EkaFhMiaxGr : public EkaFhGroup{
 public:
  virtual               ~EkaFhMiaxGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 const unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op,std::chrono::high_resolution_clock::time_point startTime={});

  bool                  processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      int16_t          pktLen);

  void                  pushUdpPkt2Q(const uint8_t* pkt, int16_t pktLen);

  int    closeSnapshotGap(EfhCtx*              pEfhCtx, 
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t          startSeq,
			  uint64_t          endSeq);

  int    closeIncrementalGap(EfhCtx*           pEfhCtx, 
			     const EfhRunCtx* pEfhRunCtx, 
			     uint64_t          startSeq,
			     uint64_t          endSeq);


  int                  bookInit();

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

/* ####################################################### */

 private:
  inline auto findUnderlying(const char* name) {
    for (int i = 0; i < (int)underlyingNum; i ++) {
      /* TEST_LOG("Comparing \'%s\' vs \'%s\'",name,underlying[i]->name); */
      /* if (memcmp(&underlying[i]->name,name,size) == 0) return i; */
      /* TEST_LOG("memcmp - FALSE"); */
      if (strcmp(underlying[i]->name,name) == 0) return i;
      /* TEST_LOG("strncmp - FALSE"); */
    }
    return -1;
  }

  inline auto addUnderlying(const char* name) {
    int u = findUnderlying(name);
    if (u >= 0) return (uint)u;

    if (underlyingNum == UndelyingsPerGroup) 
      on_error("cannot add %s to gr %u: underlyingNum=%u",name,id,underlyingNum);

    uint newUnderlying = underlyingNum++;

    underlying[newUnderlying] = new Underlying(name);
    if (underlying[newUnderlying] == NULL) on_error("cannot create new Underlying %s",name);
#if 0
    char name2print[16] = {};
    memcpy(name2print,name,size);
    TEST_LOG("%s:%u underlying[newUnderlying] = \'%s\', size = %u",
	     EKA_EXCH_DECODE(exch),id,name,(uint)size);
#endif
    return newUnderlying;
  }

/* ####################################################### */

 public:


  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurity  = EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>;
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>,SecurityIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

  static const uint UndelyingsPerGroup = 2048;

  uint  underlyingNum = 0;
  Underlying*           underlying[UndelyingsPerGroup] = {};

};
#endif

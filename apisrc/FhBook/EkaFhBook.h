#ifndef _EKA_FH_BOOK_H_
#define _EKA_FH_BOOK_H_

#include "Efh.h"

template <class FhSecurity, class SecurityId, class OrderId, class PriceT, class SizeT>
  class EkaFhBook {
 protected:
  EkaFhBook (EkaDev* _dev, 
	     EkaFhGroup* _gr, 
	     EkaSource _exch,
	     const uint _SCALE,
	     const uint _SEC_HASH_SCALE) : 
  dev(_dev),
    gr(_gr),
    exch(_exch),
    SCALE(_SCALE),
    SEC_HASH_SCALE(_SEC_HASH_SCALE) {}

 public:
  virtual int      init() = 0;

  FhSecurity*      findSecurity(SecurityId secId);

  FhSecurity*      subscribeSecurity(SecurityId      secId,
				     EfhSecurityType type,
				     EfhSecUserData  userData,
				     uint64_t        opaqueAttrA,
				     uint64_t        opaqueAttrB);

  virtual int       generateOnQuote(const EfhRunCtx* pEfhRunCtx, 
				    FhSecurity*      pFhSecurity, 
				    uint64_t         sequence,
				    uint64_t         timestamp,
				    uint             gapNum) = 0;

  //----------------------------------------------------------

  EkaDev*          dev      = NULL;
  EkaFhGroup*      gr       = NULL; 
  EkaSource        exch     = EkaSource::kInvalid; 
  //----------------------------------------------------------
  const uint       SCALE;
  const uint       SEC_HASH_SCALE; // num of significant LSBs: 17 means 2^17 = 128 * 1024 hash lines
  
  const uint64_t SEC_HASH_LINES = 0x1 << SEC_HASH_SCALE;
  const uint64_t SEC_HASH_MASK  = (0x1 << SEC_HASH_SCALE) - 1;

  //----------------------------------------------------------

  FhSecurity*     sec[SEC_HASH_LINES] = {}; // array of pointers to Securities
  uint             numSecurities = 0;

  FILE*            parser_log = NULL; // used with PRINT_PARSED_MESSAGES define
};

#endif

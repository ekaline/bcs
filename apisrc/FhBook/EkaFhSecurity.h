#ifndef _EKA_FH_SECURITY_H_
#define _EKA_FH_SECURITY_H_

#include "EkaFhBook.h"

template <class SecurityIdT, slass FhPlevel, class OrderIdT, class PriceT, class SizeT>
  class EkaFhSecurity {
 public:
  

  EkaFhSecurity(SecurityIdT     secId,
		EfhSecurityType type,
		EfhSecUserData  userData,
		uint64_t        opaqueAttrA,
		uint64_t        opaqueAttrB);

  EfhSecurityType  type;

  FhPlevel*        bid = NULL;
  FhPlevel*        ask = NULL;

  SecurityIdT      secId = 0;

  EkaFhSecurity*   next = NULL;


};
#endif

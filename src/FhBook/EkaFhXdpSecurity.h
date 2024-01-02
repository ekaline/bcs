#ifndef _EKA_FH_XDP_SECURITY_H_
#define _EKA_FH_XDP_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"
#include "EkaFhTobSecurity.h"

/* ####################################################### */

template <class SecurityIdT,
  class PriceT,
  class SizeT>
class EkaFhXdpSecurity : public EkaFhTobSecurity<SecurityIdT, PriceT, SizeT> {
 public:
    EkaFhXdpSecurity(SecurityIdT     _secId,
                     EfhSecurityType _type,
                     EfhSecUserData  _userData,
                     uint64_t        _opaqueAttrA,
                     uint64_t        _opaqueAttrB)
    : EkaFhTobSecurity<SecurityIdT, PriceT, SizeT>(_secId,_type,_userData,_opaqueAttrA,_opaqueAttrB) {}

    uint64_t auctionId = 0;
};
/* ####################################################### */

#endif

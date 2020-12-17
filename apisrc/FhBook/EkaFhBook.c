#include "EkaFhBook.h"

//----------------------------------------------------------
FhSecurity*      EkaFhBook::subscribeSecurity(SecurityId     secId,
					      EfhSecurityType type,
					      EfhSecUserData  userData,
					      uint64_t        opaqueAttrA,
					      uint64_t        opaqueAttrB) {
  FhSecurity* s = new FhSecurity(secId,type,userData,opaqueAttrA,opaqueAttrB);
  if (s == NULL) on_error("s == NULL, new FhSecurity failed");
  
  uint32_t index = secId & SEC_HASH_MASK;
  if (index >= SEC_HASH_LINES) 
    on_error("index = %u >= SEC_HASH_LINES %u",index,SEC_HASH_LINES);

  if (sec[index] == NULL) {
    sec[index] = s; // empty bucket
  } else {
    FhSecurity *sp = sec[index];
    while (sp->next != NULL) sp = sp->next;
    sp->next = s;
  }
  numSecurities++;
  return s;
}
//----------------------------------------------------------

FhSecurity* fh_book::find_security(SecurityId secId) {
  uint32_t index =  secId & EKA_FH_SEC_HASH_MASK;
  if (index >= SEC_HASH_LINES) on_error("index = %u >= SEC_HASH_LINES %u",index,SEC_HASH_LINES);
  if (sec[index] == NULL) return NULL;

  FhSecurity* sp = sec[index];

  while (sp != NULL) {
    if (sp->secId == secId) return sp;
    sp = sp->next;
  }

  return NULL;
}
//----------------------------------------------------------

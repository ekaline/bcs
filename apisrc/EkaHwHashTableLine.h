#ifndef _EKA_HW_HASH_TABLE_LINE_H_
#define _EKA_HW_HASH_TABLE_LINE_H_

#include "Efh.h"
#include "Efc.h"
#include "eka_macros.h"
#include "efh_macros.h"
#include "eka_hw_conf.h"

struct EkaHashCol {
  uint64_t secId = 0;
  uint16_t hash  = 0;
};
/* ############################################### */

class EkaHwHashTableLine {
 public:
  EkaHwHashTableLine(EkaDev* dev, EfhFeedVer hwFeedVer, int id);
  bool addSecurity(uint64_t secId);
  int  getSubscriptionId(uint64_t secId);
  int  pack6b(int _sum);
  int  downloadPacked();

 private:
  uint16_t getHash(uint64_t normSecId);
  int      getHashSize();
  int      print(const char* msg);
  int      printPacked(const char* msg);

 private:

  uint8_t  packed[64] = {};
  uint8_t  validCnt   = 0;
  uint32_t sum        = 0;

  EkaHashCol col[EKA_SUBSCR_TABLE_COLUMNS] = {};
  EkaDev*    dev        = NULL;
  EfhFeedVer hwFeedVer  = EfhFeedVer::kInvalid;
  int        id         = -1;
};

#endif

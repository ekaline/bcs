#ifndef _EKA_HW_HASH_TABLE_LINE_H_
#define _EKA_HW_HASH_TABLE_LINE_H_

#include "Efc.h"
#include "Efh.h"
#include "efh_macros.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"

struct EkaHashCol {
  bool valid = false;
  uint64_t secId = 0;
  uint16_t hash = 0;
};
/* ############################################### */

class EkaHwHashTableLine {
public:
  EkaHwHashTableLine(EkaDev *dev, EfhFeedVer hwFeedVer,
                     int id);
  bool addSecurity(uint64_t secId);
  int getSubscriptionId(uint64_t secId);
  // int  pack6b(int _sum);
  // int  pack8b(int _sum);
  int pack(int _sum);
  int downloadPacked();

private:
  uint16_t getHash(uint64_t normSecId);
  int getHashSize();
  int printLine(const char *msg);
  int printPacked(const char *msg);

private:
  //  uint8_t  packed[64] = {};
  struct PackedHashLine {
    uint32_t attr;
    uint16_t col[EFC_SUBSCR_TABLE_COLUMNS];
  } __attribute__((packed));
  PackedHashLine packed = {};

  uint32_t validCnt = 0;
  uint32_t sum = 0;

  EkaHashCol col[EFC_SUBSCR_TABLE_COLUMNS] = {};
  EkaDev *dev = NULL;
  EfhFeedVer hwFeedVer = EfhFeedVer::kInvalid;
  int id = -1;
};

#endif

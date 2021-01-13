#include "EkaHwHashTableLine.h"
#include "EkaEfc.h"
#include "EkaDev.h"
#include "EkaCore.h"
#include "EkaIgmp.h"


/* ############################################### */
EkaHwHashTableLine::EkaHwHashTableLine(EkaDev* _dev, EfhFeedVer _hwFeedVer, int _id) {
  dev       = _dev;
  hwFeedVer = _hwFeedVer;
  id        = _id;
}

/* ############################################### */
uint16_t EkaHwHashTableLine::getHash(uint64_t normSecId) {
  switch(hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
    return (normSecId & 0x1F8000) >> 15; // 6 bits

  case EfhFeedVer::kMIAX:
    return (((normSecId >> 24) & 0x3E) | (( normSecId >> 15) & 0x1)); // 6 bits

  case EfhFeedVer::kCBOE:
    return ((normSecId >> 15) & 0x1FF); // 9 bits

  default:
    on_error ("Unexpected hwFeedVer: %d",(int)hwFeedVer);
  }
}

/* ############################################### */
int EkaHwHashTableLine::getHashSize() {
  switch(hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
  case EfhFeedVer::kMIAX:
    return 6;
  case EfhFeedVer::kCBOE:
    return 9;
  default:
    on_error ("Unexpected hwFeedVer: %d",(int)hwFeedVer);
  }
}
/* ############################################### */

bool EkaHwHashTableLine::addSecurity(uint64_t secId) {
  if (validCnt == EKA_SUBSCR_TABLE_COLUMNS) {
    EKA_WARN("No room for %ju in Hash Row %d",secId,id);
    return false;
  }

  uint16_t hash = getHash(secId);

  for (auto i = 0; i < validCnt; i++) {
    if (col[i].hash == hash) {
      if (col[i].secId != secId)
	on_error("Hash error! Hash line %d: Securities %jx (%ju) and %jx (%ju) have same hash %04x",
		 id, secId, secId, col[i].secId, col[i].secId, hash);
      EKA_WARN("secId %jx (%ju) is already subscribed at Hash line %d with hash %04x",
	       secId,secId,id,hash);
      return false;
    }
  }

  col[validCnt].hash  = hash;
  col[validCnt].secId = secId;
  validCnt++;
  return true;
}

/* ############################################### */
int EkaHwHashTableLine::getSubscriptionId(uint64_t secId) {
  for (auto i = 0; i < validCnt; i++) {
    if (col[i].secId == secId) return sum + i;
  }
  return -1;
}

/* ############################################### */
static void turnOnBit(uint8_t* dst, int bitLocation) {
  int byteOffset = bitLocation / 8;
  int bitOffset  = bitLocation % 8;

  dst[byteOffset] |=  (1ULL << bitOffset);
}

/* ############################################### */
int EkaHwHashTableLine::print() {
  EKA_LOG("line %d: validCnt = %u, sum=%u, col[0].secId = 0x%jx col[0].hash = %u",
	  id, validCnt, sum, col[0].secId, col[0].hash);
  return 0;
}

/* ############################################### */
int EkaHwHashTableLine::pack(int _sum) {
  sum = _sum;
  
  *(uint32_t*)packed = sum;
  *(packed + 3)      = validCnt;
  
  uint bitOffs = 4 * 8; // sum + validCnt
  for (auto i = 0; i < EKA_SUBSCR_TABLE_COLUMNS; i++) {
    for (auto b = 0; b < getHashSize(); b++) { //iterate through bits
      if (col[i].hash & (1ULL << b)) turnOnBit(packed,bitOffs++); 
    }
  }
  /* if (id == 0x440e) { */
  /*   print(); */
  /*   uint64_t* pWord = (uint64_t*)packed; */

  /*   int packedBytes = roundUp<int>(EKA_SUBSCR_TABLE_COLUMNS * getHashSize(),8) / 8 + 4; */
  /*   int packedWords = roundUp<int>(packedBytes,8) / 8; */

  /*   for (auto i = 0; i < packedWords; i++) { */
  /*     EKA_LOG("pWord = %jx",*pWord); */
  /*     pWord++; */
  /*   } */
  /* } */

  return validCnt;
}

/* ############################################### */
int EkaHwHashTableLine::downloadPacked() {
  int packedBytes = roundUp<int>(EKA_SUBSCR_TABLE_COLUMNS * getHashSize(),8) / 8 + 4;
  int packedWords = roundUp<int>(packedBytes,8) / 8;

#ifdef _VERILOG_SIM 
  if (id == 0x440e) {
    print();
    uint64_t* pWord = (uint64_t*)packed;
    for (auto i = 0; i < packedWords; i++) {
      eka_write(dev, FH_SUBS_HASH_BASE + 8 * i, *pWord);
      EKA_LOG("pWord = %jx",*pWord);
      pWord++;
    }
    union large_table_desc desc = {};
    desc.ltd.src_bank = 0;
    desc.ltd.src_thread = 0;
    desc.ltd.target_idx = id;
    eka_write(dev, FH_SUBS_HASH_DESC, desc.lt_desc);
  }

#else
  uint64_t* pWord = (uint64_t*)packed;
  for (auto i = 0; i < packedWords; i++) {
/* #ifdef _VERILOG_SIM  */
/*     if (*pWord != 0) eka_write(dev, FH_SUBS_HASH_BASE + 8 * i, *pWord); */
/* #else */
    eka_write(dev, FH_SUBS_HASH_BASE + 8 * i, *pWord);
/* #endif */
    pWord++;
  }
  
  union large_table_desc desc = {};
  desc.ltd.src_bank = 0;
  desc.ltd.src_thread = 0;
  desc.ltd.target_idx = id;
  eka_write(dev, FH_SUBS_HASH_DESC, desc.lt_desc);

  uint64_t subdone = 1ULL<<63;
  uint64_t val = eka_read(dev, SW_STATISTICS);
  val |= subdone;
  eka_write(dev, SW_STATISTICS, val);
#endif

  return 0;
}
/* ############################################### */


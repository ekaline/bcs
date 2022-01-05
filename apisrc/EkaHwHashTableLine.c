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
  case EfhFeedVer::kCBOE:
    return (normSecId >> EFC_SUBSCR_TABLE_ROWS_BITS) & 0xFFFF;

  case EfhFeedVer::kMIAX:
    return (normSecId >> 24) & 0xFF;
    //    return (((normSecId >> 24) & 0x3E) | (( normSecId >> 15) & 0x1)); // 6 bits

  default:
    on_error ("Unexpected hwFeedVer: %d",(int)hwFeedVer);
  }
}

/* ############################################### */
int EkaHwHashTableLine::getHashSize() {
  return 8;
  /* switch(hwFeedVer) { */
  /* case EfhFeedVer::kGEMX: */
  /* case EfhFeedVer::kNASDAQ: */
  /* case EfhFeedVer::kPHLX: */
  /* case EfhFeedVer::kMIAX: */
  /*   return 6; */
  /* case EfhFeedVer::kCBOE: */
  /*   return 9; */
  /* default: */
  /*   on_error ("Unexpected hwFeedVer: %d",(int)hwFeedVer); */
  /* } */
}
/* ############################################### */

bool EkaHwHashTableLine::addSecurity(uint64_t secId) {
  if (validCnt == EFC_SUBSCR_TABLE_COLUMNS) {
    EKA_WARN("No room for %ju in Hash Row %d",secId,id);
    return false;
  }

  auto hash = getHash(secId);

  for (uint32_t i = 0; i < validCnt; i++) {
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
  col[validCnt].valid = true;
  validCnt++;
  return true;
}

/* ############################################### */
int EkaHwHashTableLine::getSubscriptionId(uint64_t secId) {
  for (uint32_t i = 0; i < validCnt; i++) {
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
int EkaHwHashTableLine::print(const char* msg) {
  EKA_LOG("%s: line %d: validCnt = %u, sum=%u, %jx:%x, %jx:%x, %jx:%x, %jx:%x",
	  msg, id, validCnt, sum, 
	  col[0].secId, col[0].hash,
	  col[1].secId, col[1].hash,
	  col[2].secId, col[2].hash,
	  col[3].secId, col[3].hash
	  );
  return 0;
}

/* ############################################### */
int EkaHwHashTableLine::printPacked(const char* msg) {
  int packedBytes = roundUp<int>(EFC_SUBSCR_TABLE_COLUMNS * getHashSize(),8) / 8 + 4;
  int packedWords = roundUp<int>(packedBytes,8) / 8;

  uint64_t* pWord = (uint64_t*)&packed;
  for (auto i = 0; i < packedWords; i++) {
    //    EKA_LOG("%s %d: pWord = %016jx",msg,i,*pWord);
    pWord++;
  }
  return 0;
}
/* ############################################### */
void packA(uint8_t* dst, const uint8_t* src, int size) {
  uint8_t* d = dst;
  bool bitArr[8 * EFC_SUBSCR_TABLE_COLUMNS] = {};
  for (auto i = 0; i < size; i++) {
    for (auto b = 0; b < 8; b++) {
      bitArr[i * 8 + b] =  (src[i] & (0x80 >> b)) != 0;
    }
  }

  bool bitArrPacked[8 * EFC_SUBSCR_TABLE_COLUMNS] = {};
  int j = 0;
  for (auto i = 0; i < size; i++) {
    for (auto b = 2; b < 8; b++) {
      bitArrPacked[j++] =  bitArr[i * 8 + b];
    }
  }

  for (auto i = 0; i < size; i++) {
    uint8_t n = 0;
    for (auto b = 0; b < 8; b++) {
      uint idx = i * 8 + b;
      n |= bitArrPacked[idx] << (7 - b);
    }
    *d++ = n;
  }
}
/* ############################################### */

/* int EkaHwHashTableLine::pack6b(int _sum) { */
/*   sum = _sum; */
  
/*   uint8_t* dst    = packed; */
/*   *(uint32_t*)dst = sum; */
/*   *(dst + 3)      = validCnt; */
/*   dst += 4; */
  
/*   uint8_t s_col = 0; // source column */
/*   uint8_t d_col = 0; // desination column */
/*   uint8_t period_groups = (EFC_SUBSCR_TABLE_COLUMNS / 4) + (EFC_SUBSCR_TABLE_COLUMNS % 4 == 0 ? 0 : 1); // */
/*   for (uint8_t gr_per = 0; gr_per < period_groups; gr_per++) { // running 42/4 = 11 group */
/*     uint8_t curr0 = s_col   < EFC_SUBSCR_TABLE_COLUMNS ? (col[s_col  ].hash     ) & 0x3F : 0; */
/*     uint8_t next0 = s_col+1 < EFC_SUBSCR_TABLE_COLUMNS ? (col[s_col+1].hash << 6) & 0xC0 : 0; */
/*     dst[d_col] = curr0 | next0; */

/*     uint8_t curr1 = s_col+1 < EFC_SUBSCR_TABLE_COLUMNS ? (col[s_col+1].hash >> 2) & 0x0F : 0; */
/*     uint8_t next1 = s_col+2 < EFC_SUBSCR_TABLE_COLUMNS ? (col[s_col+2].hash << 4) & 0xF0 : 0; */
/*     dst[d_col+1] = curr1 | next1; */

/*     uint8_t curr2 = s_col+2 < EFC_SUBSCR_TABLE_COLUMNS ? (col[s_col+2].hash >> 4) & 0x03 : 0; */
/*     uint8_t next2 = s_col+3 < EFC_SUBSCR_TABLE_COLUMNS ? (col[s_col+3].hash << 2) & 0xFC : 0; */
/*     dst[d_col+2] = curr2 | next2; */

/*     s_col += 4; */
/*     d_col += 3; */
/*   } */

/*   return validCnt; */
/* } */
/* ############################################### */

/* int EkaHwHashTableLine::pack8b(int _sum) { */
/*   sum = _sum; */
  
/*   uint8_t* dst    = packed; */
/*   *(uint32_t*)dst = sum; */
/*   packed[3] = validCnt; */

/*   return validCnt; */
/* } */
/* ############################################### */

int EkaHwHashTableLine::pack(int _sum) {
  sum = _sum;
  
  /* uint8_t* dst    = packed; */
  /* *(uint32_t*)dst = sum; */
  /* packed[3] = validCnt; */

  packed.attr  = sum;
  packed.attr |= (validCnt << 24);
  for (auto i = 0; i < EFC_SUBSCR_TABLE_COLUMNS; i++) {
    packed.col[i] = col[i].hash;
  }

  return validCnt;
}

/* ############################################### */
int EkaHwHashTableLine::downloadPacked() {
  int packedBytes = roundUp<int>(EFC_SUBSCR_TABLE_COLUMNS * getHashSize(),8) / 8 + 4;
  int packedWords = roundUp<int>(packedBytes,8) / 8;

#ifdef _VERILOG_SIM
  if (validCnt == 0) return 0;
#endif

  /* if (validCnt != 0) { */
  /*   print("downloadPacked"); */
  /*   printPacked("downloadPacked"); */
  /* } */

  uint64_t* pWord = (uint64_t*)&packed;
  for (auto i = 0; i < packedWords; i++) {
    eka_write(dev, FH_SUBS_HASH_BASE + 8 * i, *pWord);
    pWord++;
  }  

  union large_table_desc desc = {};
  desc.ltd.src_bank = 0;
  desc.ltd.src_thread = 0;
  desc.ltd.target_idx = id;
  eka_write(dev, FH_SUBS_HASH_DESC, desc.lt_desc);

  return 0;
}
/* ############################################### */


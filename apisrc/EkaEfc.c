#include <thread>
#include <arpa/inet.h>

#include "EkaEfc.h"
#include "EkaDev.h"
#include "EkaCore.h"
#include "EkaIgmp.h"
#include "EkaEpm.h"
#include "EkaHwHashTableLine.h"



/* ################################################ */
static bool isAscii (char letter) {
  //  EKA_LOG("testing %d", letter);
  if ( (letter>='0' && letter<='9') || (letter>='a' && letter<='z') || (letter>='A' && letter<='Z') ) return true;
  return false;
}

/* ################################################ */
EkaEfc::EkaEfc(EkaDev* _dev, EfhFeedVer _hwFeedVer, const EfcInitCtx* pEfcInitCtx) {
  dev       = _dev;
  hwFeedVer = _hwFeedVer;

  if (dev == NULL) on_error("dev == NULL");
  if (pEfcInitCtx == NULL) on_error("pEfcInitCtx == NULL");
  mdCoreId = pEfcInitCtx->mdCoreId;
  if (dev->core[mdCoreId] == NULL) on_error("dev->core[%u] == NULL",mdCoreId);

  for (auto i = 0; i < EKA_SUBSCR_TABLE_ROWS; i++) {
    hashLine[i] = new EkaHwHashTableLine(dev, hwFeedVer, i);
    if (hashLine[i] == NULL) on_error("hashLine[%d] == NULL",i);
  }

  cleanSubscrHwTable();
  initHwRoundTable();
  ekaIgmp = new EkaIgmp(dev,NULL,mdCoreId,EkaEpm::EfcRegion,"Efc");
  if (ekaIgmp == NULL) on_error("ekaIgmp == NULL");

  setMcParams(pEfcInitCtx);
}

/* ################################################ */
int EkaEfc::confParse(const char *key, const char *value) {
  char val_buf[200] = {};
  strcpy (val_buf,value);
  int i=0;
  char* v[10];
  v[i] = strtok(val_buf,":");
  while(v[i]!=NULL) v[++i] = strtok(NULL,":");

  // parsing KEY
  char key_buf[200] = {};
  strcpy (key_buf,key);
  i=0;
  char* k[10];
  k[i] = strtok(key_buf,".");
  while(k[i]!=NULL) k[++i] = strtok(NULL,".");


  // efc.NOM_ITTO.group.X.mcast.addr, x.x.x.x:xxxx
  // k[0] k[1]   k[2]  k[3] k[4] k[5]
  if (((strcmp(k[0],"efh")==0) || (strcmp(k[0],"efc")==0)) && (strcmp(k[2],"group")==0) && (strcmp(k[4],"mcast")==0) && (strcmp(k[5],"addr")==0)) {
    EkaSource exch = EFH_GET_SRC(k[1]);
    if (EFH_EXCH2FEED(exch) == hwFeedVer) {
      uint32_t mcAddr = inet_addr(v[0]);
      uint16_t mcPort = (uint16_t)atoi(v[1]);

      ekaIgmp->mcJoin(mcAddr,mcPort,0);
    } 
  }
  return 0;
}
/* ################################################ */

int EkaEfc::setMcParams(const EfcInitCtx* pEfcInitCtx) {
    for (uint i = 0; i < pEfcInitCtx->ekaProps->numProps; i++) {
    EkaProp& ekaProp{ pEfcInitCtx->ekaProps->props[i] };
    //    EKA_DEBUG("ekaProp.szKey = %s, ekaProp.szVal = %s",ekaProp.szKey,ekaProp.szVal);   fflush(stderr);
    confParse(ekaProp.szKey,ekaProp.szVal);
  }

  return 0;
}

/* ################################################ */
int EkaEfc::cleanSubscrHwTable() {
  /* struct timespec req = {0, 1000}; */
  /* struct timespec rem = {}; */

  EKA_LOG("Cleaning HW Subscription Table: %d rows, %d words per row",
	  EKA_SUBSCR_TABLE_ROWS,EKA_SUBSCR_TABLE_COLUMNS);

  uint64_t val = eka_read(dev, SW_STATISTICS);
  val &= 0xFFFFFFFF00000000;
  eka_write(dev, SW_STATISTICS, val);


  /* for (auto r = 0; r < EKA_SUBSCR_TABLE_ROWS; r++) { */
  /*   for (int w = 0; w < wordsPerSubscrRow(); w++) { */
  /*     eka_write(dev, FH_SUBS_HASH_BASE + 8 * w, 0); */
  /*   } */
  /*   union large_table_desc desc = {}; */
  /*   desc.ltd.src_bank = 0; */
  /*   desc.ltd.src_thread = 0; */
  /*   desc.ltd.target_idx = i; */
  /*   eka_write(dev, FH_SUBS_HASH_DESC, desc.lt_desc); */
      
  /*   nanosleep(&req, &rem);  // added due to "too fast" write to card */
  /* } */
  return 0;
}


/* ################################################ */
int EkaEfc::initHwRoundTable() {
  for (uint64_t addr = 0; addr < ROUND_2B_TABLE_DEPTH; addr++) {
    uint64_t data = 0;
    switch (hwFeedVer) {
    case EfhFeedVer::kPHLX:
    case EfhFeedVer::kGEMX: 
      data = (addr / 10) * 10;
      break;
    case EfhFeedVer::kNASDAQ: 
    case EfhFeedVer::kMIAX:
    case EfhFeedVer::kCBOE:
      data = addr;
      break;
    default:
      on_error("Unexpected hwFeedVer = 0x%x",(int)hwFeedVer);
    }
    eka_write (dev,ROUND_2B_ADDR,addr);
    eka_write (dev,ROUND_2B_DATA,data);
    //    EKA_LOG("%016x (%ju) @ %016x (%ju)",data,data,addr,addr);
  }
  return 0;
}
/* ############################################### */

bool EkaEfc::isValidSecId(uint64_t secId) {
  switch(hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
    if ((secId & ~0x1FFFFFULL) != 0) return false;
    return true;

  case EfhFeedVer::kMIAX:
    if ((secId & ~0x3E00FFFFULL) != 0) return false;
    return true;

  case EfhFeedVer::kCBOE:
    if (((char)((secId >> (8 * 5)) & 0xFF) != '0') ||
	((char)((secId >> (8 * 4)) & 0xFF) != '1') ||
	! isAscii((char)((secId >> (8 * 3)) & 0xFF)) ||
	! isAscii((char)((secId >> (8 * 2)) & 0xFF)) ||
	! isAscii((char)((secId >> (8 * 1)) & 0xFF)) ||
	! isAscii((char)((secId >> (8 * 0)) & 0xFF))) return false;
    return true;

  default:
    on_error ("Unexpected hwFeedVer: %d", (int)hwFeedVer);
  }
}
/* ################################################ */
static uint64_t char2num(char c) {
  if (c >= '0' && c <= '9') return c - '0';            // 10
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;       // 36
  if (c >= 'a' && c <= 'z') return c - 'a' + 10 + 26;  // 62
  on_error("Unexpected char \'%c\' (0x%x)",c,c);
}

/* ################################################ */
int EkaEfc::normalizeId(uint64_t secId) {
  switch(hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
  case EfhFeedVer::kMIAX:
    return secId;
  case EfhFeedVer::kCBOE: {
    uint64_t res = 0;
    char c = '\0'; 
    uint64_t n = 0;

    c = (char) ((secId >> (8 * 0)) & 0xFF);
    n = char2num(c)    << (6 * 0);
    res |= n;

    c = (char) ((secId >> (8 * 1)) & 0xFF);
    n = char2num(c)    << (6 * 1);
    res |= n;

    c = (char) ((secId >> (8 * 2)) & 0xFF);
    n = char2num(c)    << (6 * 2);
    res |= n;

    c = (char) ((secId >> (8 * 3)) & 0xFF);
    n = char2num(c)    << (6 * 3);
    res |= n;

    return res;
  }
  default:
    on_error ("Unexpected hwFeedVer: %d", (int)hwFeedVer);
  }
}
/* ################################################ */
int EkaEfc::getLineIdx(uint64_t normSecId) {
    return (int) normSecId & 0x7FFF; // Low 15 bits
}
/* ################################################ */
int EkaEfc::subscribeSec(uint64_t secId) {
  if (! isValidSecId(secId)) 
    on_error("Security %ju (0x%jx) violates Hash function assumption",secId,secId);

  if (numSecurities == EKA_MAX_P4_SUBSCR) {
    EKA_WARN("numSecurities %d  == EKA_MAX_P4_SUBSCR: secId %ju (0x%jx) is ignored",
	     numSecurities,secId);
    return -1;
  }

  uint64_t normSecId = normalizeId(secId);
  int      lineIdx   = getLineIdx(normSecId);

  if (hashLine[lineIdx]->addSecurity(normSecId)) {
    numSecurities++;
    uint64_t val = eka_read(dev, SW_STATISTICS);
    val &= 0xFFFFFFFF00000000;
    val |= (uint64_t)(numSecurities);
    eka_write(dev, SW_STATISTICS, val);
  }
  return 0;
}
/* ################################################ */
int EkaEfc::getSubscriptionId(uint64_t secId) {
  if (! isValidSecId(secId)) 
    on_error("Security %ju (0x%jx) violates Hash function assumption",secId,secId);
  uint64_t normSecId = normalizeId(secId);
  int      lineIdx   = getLineIdx(normSecId);
  
  return hashLine[lineIdx]->getSubscriptionId(normSecId);
}

/* ################################################ */
int EkaEfc::downloadTable() {
  int sum = 0;
  for (auto i = 0; i < EKA_SUBSCR_TABLE_ROWS; i++) {
    struct timespec req = {0, 1000};
    struct timespec rem = {};

    sum += hashLine[i]->pack(sum);
    hashLine[i]->downloadPacked();

    nanosleep(&req, &rem);  // added due to "too fast" write to card
  }

  if (sum != numSecurities) 
    on_error("sum %d != numSecurities %d",sum,numSecurities);

  return 0;
}

/* ################################################ */

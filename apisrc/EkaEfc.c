#include <thread>
#include <arpa/inet.h>

#include "EkaEfc.h"
#include "EkaDev.h"
#include "EkaCore.h"
#include "EkaIgmp.h"
#include "EkaEpm.h"
#include "EkaHwHashTableLine.h"
#include "EkaUdpSess.h"
#include "EkaTcpSess.h"
#include "EkaEpmAction.h"
#include "EpmFireSqfTemplate.h"
#include "EkaEfcDataStructs.h"

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
  ekaIgmp = new EkaIgmp(dev,mdCoreId,EkaEpm::EfcRegion,"Efc");
  if (ekaIgmp == NULL) on_error("ekaIgmp == NULL");

  getMcParams(pEfcInitCtx);
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

      if (findUdpSess(mcAddr,mcPort) != NULL) 
	on_error("\'%s\',\'%s\' : Udp Session %s:%u is already set",
		 key,value,EKA_IP2STR(mcAddr),mcPort);

      udpSess[numUdpSess] = new EkaUdpSess(dev,numUdpSess,mcAddr,mcPort);
      if (udpSess[numUdpSess] == NULL) on_error("udpSess[%d] == NULL",numUdpSess);
      numUdpSess++;
    } 
  }
  return 0;
}
/* ################################################ */

int EkaEfc::igmpJoinAll() {
  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);
    ekaIgmp->mcJoin(udpSess[i]->ip,udpSess[i]->port,0);
  }
  
  return 0;
}

/* ################################################ */
int EkaEfc::armController() {
  eka_write(dev, P4_ARM_DISARM, 1); 
  return 0;
}
/* ################################################ */
int EkaEfc::disArmController() {
  eka_write(dev, P4_ARM_DISARM, 0); 
  return 0;
}

/* ################################################ */
int EkaEfc::initStrategy(const EfcStratGlobCtx* efcStratGlobCtx) {
  memcpy(&stratGlobCtx,efcStratGlobCtx,sizeof(EfcStratGlobCtx));
  eka_write(dev,P4_ARM_DISARM,0); 
  return 0;
}
/* ################################################ */
EkaUdpSess* EkaEfc::findUdpSess(uint32_t mcAddr, uint16_t mcPort) {
  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);
    if (udpSess[i]->myParams(mcAddr,mcPort)) return udpSess[i];
  }
  return NULL;
}
/* ################################################ */

int EkaEfc::getMcParams(const EfcInitCtx* pEfcInitCtx) {
    for (uint i = 0; i < pEfcInitCtx->ekaProps->numProps; i++) {
    EkaProp& ekaProp{ pEfcInitCtx->ekaProps->props[i] };
    //    EKA_DEBUG("ekaProp.szKey = %s, ekaProp.szVal = %s",ekaProp.szKey,ekaProp.szVal);   fflush(stderr);
    confParse(ekaProp.szKey,ekaProp.szVal);
  }

  return 0;
}

/* ################################################ */
int EkaEfc::cleanSubscrHwTable() {
  EKA_LOG("Cleaning HW Subscription Table: %d rows, %d words per row",
	  EKA_SUBSCR_TABLE_ROWS,EKA_SUBSCR_TABLE_COLUMNS);

  uint64_t val = eka_read(dev, SW_STATISTICS);
  val &= 0xFFFFFFFF00000000;
  eka_write(dev, SW_STATISTICS, val);
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

    uint64_t indAddr = 0x0100000000000000 + addr;
    indirectWrite(dev,indAddr,data);

    /* eka_write (dev,ROUND_2B_ADDR,addr); */
    /* eka_write (dev,ROUND_2B_DATA,data); */
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
int EkaEfc::enableRxFire() {
  uint64_t fire_rx_tx_en = eka_read(dev,ENABLE_PORT);
  fire_rx_tx_en |= 1ULL << (16 + fireCoreId); //fire core enable */
  fire_rx_tx_en |= 1ULL << mdCoreId;          // RX (Parser) core enable */
  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);

  EKA_LOG("fire_rx_tx_en = 0x%016jx",fire_rx_tx_en);
  return 0;
}
/* ################################################ */
int EkaEfc::run() {
  setHwGlobalParams();
  setHwUdpParams();
  setHwStratRegion();
  igmpJoinAll();

  enableRxFire();

  return 0;
}

/* ################################################ */
int EkaEfc::setHwGlobalParams() {
  EKA_LOG("enable_strategy=%d, report_only=%d, debug_always_fire=%d, debug_always_fire_on_unsubscribed=%d, max_size=%d, watchdog_timeout_sec=%ju",
	  stratGlobCtx.enable_strategy, 
	  stratGlobCtx.report_only, 
	  stratGlobCtx.debug_always_fire, 
	  stratGlobCtx.debug_always_fire_on_unsubscribed, 
	  stratGlobCtx.max_size, 
	  stratGlobCtx.watchdog_timeout_sec);

  eka_write(dev,P4_GLOBAL_MAX_SIZE,stratGlobCtx.max_size);

  uint64_t p4_strat_conf = eka_read(dev,P4_STRAT_CONF);
  uint64_t p4_watchdog_period = EKA_WATCHDOG_SEC_VAL;

  if (stratGlobCtx.report_only == 1) 
    p4_strat_conf |= EKA_P4_REPORT_ONLY_BIT;
  if (stratGlobCtx.debug_always_fire == 1) 
    p4_strat_conf |= EKA_P4_ALWAYS_FIRE_BIT;
  if (stratGlobCtx.debug_always_fire_on_unsubscribed == 1) 
    p4_strat_conf |= EKA_P4_ALWAYS_FIRE_UNSUBS_BIT;
  /* if (stratGlobCtx.auto_rearm == 1)  */
  /*   p4_strat_conf |= EKA_P4_AUTO_REARM_BIT; */
  if (stratGlobCtx.watchdog_timeout_sec != 0) 
    p4_watchdog_period = EKA_WATCHDOG_SEC_VAL * stratGlobCtx.watchdog_timeout_sec;
    

  eka_write(dev,P4_STRAT_CONF,   p4_strat_conf);
  eka_write(dev,P4_WATCHDOG_CONF,p4_watchdog_period);

  return 0;
}
/* ################################################ */
int EkaEfc::setHwUdpParams() {
  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);

    EKA_LOG("configuring IP:UDP_PORT for MD for group:%d",i);
    uint32_t ip   = be32toh(udpSess[i]->ip);
    uint16_t port = be16toh(udpSess[i]->port);
    uint64_t tmp_ip   = ((uint64_t) i) << 32 | ip;
    uint64_t tmp_port = ((uint64_t) i) << 32 | ((uint64_t)udpSess[i]->firstSessId) << 16 | port;
    //        EKA_LOG("writing 0x%016jx (ip=%s) to addr  0x%016jx",tmp_ip,inet_ntoa(dev->core[0].udp_sess[s].mcast.sin_addr),(uint64_t)FH_GROUP_IP);
    eka_write (dev,FH_GROUP_IP,tmp_ip);
    //        EKA_LOG("writing 0x%016jx to addr  0x%016jx",tmp_port,(uint64_t)FH_GROUP_PORT);
    eka_write (dev,FH_GROUP_PORT,tmp_port);
  }
  return 0;
}
/* ################################################ */
int EkaEfc::setHwStratRegion() {
  struct StratRegion {
    uint8_t region;
    uint8_t strategyIdx;
  } __attribute__((packed));

  StratRegion stratRegion[MAX_UDP_SESS] = {};

  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);
    stratRegion[i].region      = EkaEpm::EfcRegion;
    stratRegion[i].strategyIdx = 0;
  }
  copyBuf2Hw(dev,0x83000,(uint64_t*) &stratRegion,sizeof(stratRegion));

  return 0;
}
/* ################################################ */

int EkaEfc::createFireAction(uint8_t group, ExcConnHandle hConn) {
  if (numFireActions == MAX_FIRE_ACTIONS) 
    on_error("numFireActions == MAX_FIRE_ACTIONS %d",numFireActions);

  EkaCoreId   myCoreId  = excGetCoreId(hConn);
  uint        mySessId  = excGetSessionId(hConn);
  if (dev->core[myCoreId] == NULL) on_error("dev->core[%u] == NULL",myCoreId);
  EkaTcpSess* myTcpSess = dev->core[myCoreId]->tcpSess[mySessId];
  if (myTcpSess == NULL) on_error("myTcpSess == NULL");
  
  if (fireCoreId == -1) {
    fireCoreId = myCoreId;
  } else {
    if (fireCoreId != myCoreId) 
      on_error("fireCoreId %u != myCoreId %u",fireCoreId, myCoreId);
  }

  udpSess[group]->firstSessId = mySessId;

  fireAction[numFireActions] = dev->epm->addAction(EkaEpm::ActionType::HwFireAction,
						   EkaEpm::EfcRegion,
						   0, //localIdx
						   myCoreId,
						   mySessId,
						   0 //auxIdx
						   );

  fireAction[numFireActions]->setNwHdrs(myTcpSess->macDa,
					myTcpSess->macSa,
					myTcpSess->srcIp,
					myTcpSess->dstIp,
					myTcpSess->srcPort,
					myTcpSess->dstPort);

  numFireActions++;
  return 0;
}

/* ################################################ */
EkaEpmAction* EkaEfc::findFireAction(ExcConnHandle hConn) {
  EkaCoreId   myCoreId  = excGetCoreId(hConn);
  uint        mySessId  = excGetSessionId(hConn);
  for (auto i = 0; i < numFireActions; i++) {
    if (fireAction[i] == NULL) on_error("fireAction[%d] == NULL",i);
    if (fireAction[i]->coreId == myCoreId && fireAction[i]->sessId == mySessId)
      return fireAction[i];
  }
  return NULL;
}

/* ################################################ */
int EkaEfc::setActionPayload(ExcConnHandle hConn,const void* fireMsg, size_t fireMsgSize) {
  EkaEpmAction* myAction = findFireAction(hConn);
  if (myAction == NULL) on_error("myAction == NULL");

  myAction->setPktPayload(fireMsg,fireMsgSize);

  return 0;
}

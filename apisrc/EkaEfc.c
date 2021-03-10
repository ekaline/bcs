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
#include "EkaHwCaps.h"

void ekaFireReportThread(EkaDev* dev);

/* ################################################ */
static bool isAscii (char letter) {
  //  EKA_LOG("testing %d", letter);
  if ( (letter>='0' && letter<='9') || (letter>='a' && letter<='z') || (letter>='A' && letter<='Z') ) return true;
  return false;
}

/* ################################################ */
EkaEfc::EkaEfc(EkaEpm*                  epm, 
	       epm_strategyid_t         id, 
	       epm_actionid_t           baseActionIdx, 
	       const EpmStrategyParams* params, 
	       EfhFeedVer               hwFeedVer) : 
EpmStrategy(epm,id,baseActionIdx,params,hwFeedVer) {
  
  EKA_LOG("Creating EkaEfc: hwFeedVer=%d",(int)hwFeedVer);
  
  for (auto i = 0; i < EKA_SUBSCR_TABLE_ROWS; i++) {
    hashLine[i] = new EkaHwHashTableLine(dev, hwFeedVer, i);
    if (hashLine[i] == NULL) on_error("hashLine[%d] == NULL",i);
  }
  
#ifndef _VERILOG_SIM
  cleanSubscrHwTable();
#endif

  initHwRoundTable();
  /* ekaIgmp = new EkaIgmp(dev,EkaEpm::EfcRegion,"Efc"); */
  /* if (ekaIgmp == NULL) on_error("ekaIgmp == NULL"); */

  /* getMcParams(pEfcInitCtx); */
}

/* ################################################ */
/* int EkaEfc::confParse(const char *key, const char *value) { */
/*   char val_buf[200] = {}; */
/*   strcpy (val_buf,value); */
/*   int i=0; */
/*   char* v[10]; */
/*   v[i] = strtok(val_buf,":"); */
/*   while(v[i]!=NULL) v[++i] = strtok(NULL,":"); */

/*   // parsing KEY */
/*   char key_buf[200] = {}; */
/*   strcpy (key_buf,key); */
/*   i=0; */
/*   char* k[10]; */
/*   k[i] = strtok(key_buf,"."); */
/*   while(k[i]!=NULL) k[++i] = strtok(NULL,"."); */

/*   EKA_LOG("Processing %s -- %s",key,value); */
/*   // efc.group.X.mcast.addr, x.x.x.x:xxxx */
/*   //k[0] k[1]k[2] k[3] k[4] */
/*   if (((strcmp(k[0],"efh")==0) || (strcmp(k[0],"efc")==0)) && (strcmp(k[1],"group")==0) && (strcmp(k[3],"mcast")==0) && (strcmp(k[4],"addr")==0)) { */

/*     uint32_t mcAddr = inet_addr(v[0]); */
/*     uint16_t mcPort = (uint16_t)atoi(v[1]); */

/*     if (findUdpSess(mcAddr,mcPort) != NULL)  */
/*       on_error("\'%s\',\'%s\' : Udp Session %s:%u is already set", */
/* 	       key,value,EKA_IP2STR(mcAddr),mcPort); */

/*     udpSess[numUdpSess] = new EkaUdpSess(dev,numUdpSess,mcAddr,mcPort); */
/*     if (udpSess[numUdpSess] == NULL) on_error("udpSess[%d] == NULL",numUdpSess); */
/*     numUdpSess++; */
/*     EKA_LOG("%s:%u is set, numUdpSess = %d",EKA_IP2STR(mcAddr),mcPort,numUdpSess); */
/*   } */
/*   return 0; */
/* } */
/* ################################################ */

/* int EkaEfc::igmpJoinAll() { */
/*   for (auto i = 0; i < numUdpSess; i++) { */
/*     if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i); */
/*     ekaIgmp->mcJoin(udpSess[i]->coreId,udpSess[i]->ip,udpSess[i]->port,0,&pktCnt); */
/*   } */
  
/*   return 0; */
/* } */

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
EkaUdpSess* EkaEfc::findUdpSess(EkaCoreId coreId, uint32_t mcAddr, uint16_t mcPort) {
  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);
    if (udpSess[i]->myParams(coreId,mcAddr,mcPort)) return udpSess[i];
  }
  return NULL;
}
/* ################################################ */

/* int EkaEfc::getMcParams(const EfcInitCtx* pEfcInitCtx) { */
/*     for (uint i = 0; i < pEfcInitCtx->ekaProps->numProps; i++) { */
/*     EkaProp& ekaProp{ pEfcInitCtx->ekaProps->props[i] }; */
/*     //    EKA_DEBUG("ekaProp.szKey = %s, ekaProp.szVal = %s",ekaProp.szKey,ekaProp.szVal);   fflush(stderr); */
/*     confParse(ekaProp.szKey,ekaProp.szVal); */
/*   } */

/*   return 0; */
/* } */

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
#ifdef _VERILOG_SIM
  return 0;
#else
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
#endif
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
/* #ifdef _VERILOG_SIM */
/*     return (int) normSecId & 0x3F; // Low 6 bits */
/* #else */
    return (int) normSecId & 0x7FFF; // Low 15 bits
/* #endif */

}
/* ################################################ */
int EkaEfc::subscribeSec(uint64_t secId) {
  if (! isValidSecId(secId)) 
    on_error("Security %ju (0x%jx) violates Hash function assumption",secId,secId);

  if (numSecurities == EKA_MAX_P4_SUBSCR) {
    EKA_WARN("numSecurities %d  == EKA_MAX_P4_SUBSCR: secId %ju (0x%jx) is ignored",
	     numSecurities,secId,secId);
    return -1;
  }

  uint64_t normSecId = normalizeId(secId);
  int      lineIdx   = getLineIdx(normSecId);

  EKA_DEBUG("Subscribing on 0x%jx, lineIdx = 0x%x (%d)",secId,lineIdx,lineIdx);
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

    sum += hashLine[i]->pack6b(sum);
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
  uint8_t tcpCores = dev->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores  = dev->ekaHwCaps->hwCaps.core.bitmap_md_cores;

  for (uint8_t coreId = 0; coreId < EkaDev::MAX_CORES; coreId++) {
    if ((0x1 << coreId) & tcpCores) {
      fire_rx_tx_en |= 1ULL << (16 + coreId); //fire core enable */
    }
    if ((0x1 << coreId) & mdCores) {
      fire_rx_tx_en |= 1ULL << coreId;          // RX (Parser) core enable */
    }
  }

  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);

  EKA_LOG("fire_rx_tx_en = 0x%016jx",fire_rx_tx_en);
  return 0;
}
/* ################################################ */
int EkaEfc::run(EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx) {
  memcpy(&localCopyEfcCtx,   pEfcCtx,   sizeof(EfcCtx));
  memcpy(&localCopyEfcRunCtx,pEfcRunCtx,sizeof(EfcRunCtx));

  setHwGlobalParams();
  setHwUdpParams();
  setHwStratRegion();
  //  igmpJoinAll();

  enableRxFire();

  if (! dev->fireReportThreadActive) {
    dev->fireReportThread = std::thread(ekaFireReportThread,dev);
    dev->fireReportThread.detach();
    while (! dev->fireReportThreadActive) sleep(0);
    EKA_LOG("fireReportThread activated");
  }

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
  EKA_LOG("downloading %d MC sessions to FPGA",numUdpSess);
  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);

    EKA_LOG("configuring IP:UDP_PORT %s:%u for MD for group:%d",EKA_IP2STR(udpSess[i]->ip),udpSess[i]->port,i);
    uint32_t ip   = udpSess[i]->ip;
    uint16_t port = udpSess[i]->port;

    uint64_t tmp_ipport = ((uint64_t)i) << 56 | ((uint64_t)port) << 32 | be32toh(ip);
    //  EKA_LOG("HW Port-IP register = 0x%016jx (%x : %x)",tmp_ipport,ip,port);
    eka_write (dev,FH_GROUP_IPPORT,tmp_ipport);

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

/* EkaEpmAction* EkaEfc::createFireAction(epm_actionid_t actionIdx, ExcConnHandle hConn) { */
/*   if (numFireActions == MAX_FIRE_ACTIONS)  */
/*     on_error("numFireActions == MAX_FIRE_ACTIONS %d",numFireActions); */

/*   EkaCoreId   myCoreId  = excGetCoreId(hConn); */
/*   uint        mySessId  = excGetSessionId(hConn); */
/*   if (dev->core[myCoreId] == NULL) on_error("dev->core[%u] == NULL",myCoreId); */
/*   EkaTcpSess* myTcpSess = dev->core[myCoreId]->tcpSess[mySessId]; */
/*   if (myTcpSess == NULL) on_error("myTcpSess == NULL"); */
  
/*   if (fireCoreId == -1) { */
/*     fireCoreId = myCoreId; */
/*   } else { */
/*     if (fireCoreId != myCoreId)  */
/*       on_error("fireCoreId %d != myCoreId %d",fireCoreId, myCoreId); */
/*   } */

/*   //  udpSess[group]->firstSessId = mySessId; */

/*   int newActionId = numFireActions; */
  
/*   fireAction[newActionId] = dev->epm->addAction(EkaEpm::ActionType::HwFireAction, */
/* 						   EkaEpm::EfcRegion, */
/* 						   actionIdx, //localIdx */
/* 						   myCoreId, */
/* 						   mySessId, */
/* 						   0 //auxIdx */
/* 						   ); */

/*   fireAction[newActionId]->setNwHdrs(myTcpSess->macDa, */
/* 					myTcpSess->macSa, */
/* 					myTcpSess->srcIp, */
/* 					myTcpSess->dstIp, */
/* 					myTcpSess->srcPort, */
/* 					myTcpSess->dstPort); */


/*   EKA_LOG("Created FireAction: on fireCoreId %d %s:%u --> %s:%u ", */
/* 	  fireCoreId, */
/* 	  EKA_IP2STR(myTcpSess->srcIp),myTcpSess->srcPort, */
/* 	  EKA_IP2STR(myTcpSess->dstIp),myTcpSess->dstPort); */
/*   numFireActions++; */
/*   return fireAction[newActionId]; */
/* } */

/* ################################################ */
/* EkaEpmAction* EkaEfc::findFireAction(ExcConnHandle hConn) { */
/*   EkaCoreId   myCoreId  = excGetCoreId(hConn); */
/*   uint        mySessId  = excGetSessionId(hConn); */
/*   for (auto i = 0; i < numFireActions; i++) { */
/*     if (fireAction[i] == NULL) on_error("fireAction[%d] == NULL",i); */
/*     if (fireAction[i]->coreId == myCoreId && fireAction[i]->sessId == mySessId) */
/*       return fireAction[i]; */
/*   } */
/*   return NULL; */
/* } */

/* /\* ################################################ *\/ */
/* int EkaEfc::setActionPayload(ExcConnHandle hConn,const void* fireMsg, size_t fireMsgSize) { */
/*   EkaEpmAction* myAction = findFireAction(hConn); */
/*   if (myAction == NULL) on_error("myAction == NULL"); */

/*   myAction->setPktPayload(fireMsg,fireMsgSize); */

/*   return 0; */
/* } */

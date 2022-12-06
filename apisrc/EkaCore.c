#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"
#include "EkaUdpTxSess.h"
#include "EkaUdpChannel.h"
#include "EkaHwCaps.h"

struct netif* initLwipNetIf(EkaDev* dev, EkaCoreId coreId, uint8_t* macSa, uint32_t srcIp);

void ekaInitLwip (EkaDev* dev);
void setNetifIpMacSa (EkaDev* dev, EkaCoreId coreId, const uint8_t*  macSa);
void setNetifIpSrc   (EkaDev* dev, EkaCoreId coreId, const uint32_t* srcIp);
void setNetifGw      (EkaDev* dev, EkaCoreId coreId, const uint32_t* pGwIp);
void setNetifNetmask (EkaDev* dev, EkaCoreId coreId, const uint32_t* pNetmaskIp);
int ekaAddArpEntry   (EkaDev* dev, EkaCoreId coreId, const uint32_t* protocolAddr,
                      const uint8_t* hwAddr);

/* ------------------------------------------------------------- */

EkaCore::EkaCore(EkaDev* pEkaDev, uint8_t lane, uint32_t ip, uint8_t* mac, bool epmEnabled) {
    dev = pEkaDev;
    coreId = lane;
    memcpy(macSa,mac,6);
    srcIp = ip;
    connected = true;
    vlanTag = 0;

    isTcpCore = dev->ekaHwCaps->hwCaps.core.bitmap_tcp_cores & (1 << coreId);

    EKA_LOG("%s Core %u: %s %s",
	    isTcpCore ? "TCP Enabled" : "NON TCP",
	    coreId,EKA_MAC2STR(macSa),EKA_IP2STR(srcIp));
    for (uint i = 0; i < MAX_SESS_PER_CORE; i++) tcpSess[i] = NULL;
    tcpSessions = 0;


    return;
}

/* ------------------------------------------------------------- */
bool EkaCore::initTcp() {
  if (! isTcpCore) {
    on_error("TCP functionality is disabled for lane %u",coreId);
    return false;
  }

  EKA_LOG("Initializing TCP for lane %u: "
	  "macSa=%s, srcIp=%s, gwIp=%s",
	  coreId,EKA_MAC2STR(macSa),EKA_IP2STR(srcIp),EKA_IP2STR(gwIp));


  pLwipNetIf = initLwipNetIf(dev,coreId,macSa,srcIp);

  setNetifIpMacSa(dev,coreId,macSa);
  setNetifIpSrc(dev,coreId,&srcIp);
  
  if (gwIp != 0)    setNetifGw(dev,coreId,&gwIp);
  if (netmask != 0) setNetifNetmask(dev,coreId,&netmask);
  if (! eka_is_all_zeros(&macDa,6)) {
      if (ekaAddArpEntry(dev, coreId, &gwIp, macDa) == -1)
	on_error("Failed to add static ARP entry for lane %u: %s --> %s",
		 coreId,EKA_IP2STR(gwIp),EKA_MAC2STR(macDa));
      EKA_LOG("Lane %u: Adding GW static ARP: %s",
	      coreId,EKA_MAC2STR(macDa));
  }

    
  tcpSess[CONTROL_SESS_ID] = new EkaTcpSess(dev,
					    this,
					    coreId,
					    CONTROL_SESS_ID,
					    0 /* srcIp */,
					    0 /* dstIp */,
					    0 /* dstPort */, 
					    macSa);

  uint64_t macSaBuf = 0;
  auto pmacSaBuf = reinterpret_cast<uint8_t*>(&macSaBuf);
  memcpy(pmacSaBuf + 2,macSa,6);
  
  EKA_LOG("Enabling TCP RX on core %u for MAC %s (0x%016jx)",
	  coreId,EKA_MAC2STR(macSa),be64toh(macSaBuf));
  
  eka_write(dev, 0xe0000 + coreId * 0x1000 + 0x200, be64toh(macSaBuf));
  
  return true;  
}
/* ------------------------------------------------------------- */

EkaCore::~EkaCore() {
  EKA_LOG("deleting core %d",coreId);

  for (uint i = 0; i < MAX_SESS_PER_CORE; i++) {
    if (tcpSess[i]) delete tcpSess[i];
  }
  
  /* if (stratUdpChannel != NULL) delete stratUdpChannel; */
  /* if (feedServerUdpChannel != NULL) delete feedServerUdpChannel; */
}

/* ------------------------------------------------------------- */
EkaTcpSess* EkaCore::findTcpSess(uint32_t ipSrc, uint16_t srcPort, uint32_t ipDst, uint16_t dstPort) {
  for (uint i = 0; i < MAX_SESS_PER_CORE; i++) {
    if (tcpSess[i] == NULL) continue;// on_error("Core %u, tcpSess[%u] == NULL",coreId,i);
    if (tcpSess[i]->myParams(ipSrc,srcPort,ipDst,dstPort)) return tcpSess[i];
  }
  return NULL;
}
/* ------------------------------------------------------------- */
EkaTcpSess* EkaCore::findTcpSess(int sock) {
  for (uint i = 0; i < MAX_SESS_PER_CORE; i++) {
    if (tcpSess[i] == NULL) continue;// on_error("Core %u, tcpSess[%u] == NULL",coreId,i);
    if (tcpSess[i]->myParams(sock)) return tcpSess[i];
  }
  return NULL;
}
/* ------------------------------------------------------------- */
/* EkaUdpSess* EkaCore::findUdpSess(uint32_t ip, uint16_t port) { */
/*   for (uint i = 0; i < udpSessions; i++) { */
/*     if (udpSess[i] == NULL) on_error("Core %u, udpSess[%u] == NULL",coreId,i); */
/*     if (udpSess[i]->myParams(ip,port)) return udpSess[i]; */
/*   } */
/*   return NULL; */
/* } */

/* ------------------------------------------------------------- */
uint8_t EkaCore::getFreeTcpSess() {
  for (uint i = 0; i < MAX_SESS_PER_CORE; i++) {
    if (tcpSess[i] == NULL) return i;
  }
  on_error("No free Tcp Sessions to allocate (all %u are in use)",MAX_SESS_PER_CORE);
  return 0xFF;
}

/* ------------------------------------------------------------- */
uint EkaCore::addTcpSess() {
  if (tcpSessions == MAX_SESS_PER_CORE) on_error("tcpSessions = %u",tcpSessions);

  uint8_t sessId = getFreeTcpSess();
  tcpSessions++;
  tcpSess[sessId] = new EkaTcpSess(dev, this, coreId, sessId, srcIp, 0, 0, macSa);
  return sessId;
}
/* ------------------------------------------------------------- */
uint EkaCore::addUdpTxSess(eka_ether_addr srcMac, eka_ether_addr dstMac,
			   eka_in_addr_t srcIp, eka_in_addr_t dstIp, 
			   uint16_t srcUpdPort, uint16_t dstUpdPort) {
  if (udpTxSessions ==MAX_UDP_TX_SESS_PER_CORE)
    on_error("udpTxSessions = %u",udpTxSessions);

  uint8_t sessId = udpTxSessions++;

  udpTxSess[sessId] = new EkaUdpTxSess(dev, coreId, sessId,
				       srcMac,dstMac,srcIp,dstIp,
				       srcUpdPort,dstUpdPort);
  if (!udpTxSess[sessId])
    on_error("Failed creating new EkaUdpTxSess");
  return sessId;
}
/* ------------------------------------------------------------- */
void EkaCore::suppressOldTcpSess(uint8_t newSessId,
			 uint32_t srcIp,uint16_t srcPort,
			 uint32_t dstIp,uint16_t dstPort) {
  for (uint i = 0; i < MAX_SESS_PER_CORE; i++) {
    if (i == newSessId) continue;
    if (tcpSess[i] == NULL) continue;
    if (tcpSess[i]->srcIp   == srcIp   &&
	tcpSess[i]->srcPort == srcPort &&
	tcpSess[i]->dstIp   == dstIp   &&
	tcpSess[i]->dstPort == dstPort) {
      if (tcpSessions == 0) on_error("tcpSessions == 0");
      tcpSessions--;
      delete tcpSess[i];
      tcpSess[i] = NULL;
      EKA_LOG("Session %u:%u is suppressed %s:%u --> %s:%u",coreId,i,
	      EKA_IP2STR(srcIp),srcPort,EKA_IP2STR(dstIp),dstPort);
    }
  }
  return;
}
/* ------------------------------------------------------------- */
/* int EkaCore::tcpConnect(uint32_t dstIp, uint16_t dstPort) { */
/*   dev->addTcpSessMtx.lock(); */
/*   if (tcpSessions == MAX_SESS_PER_CORE)  */
/*     on_error("tcpSessions = %u",tcpSessions); */

/*   uint8_t sessId = getFreeTcpSess(); */
/*   tcpSessions++; */
/*   EKA_LOG("Opening TCP session %u from core %u, srcIp = %s",sessId,coreId,EKA_IP2STR(srcIp)); */

/*   tcpSess[sessId] = new EkaTcpSess(dev, this, coreId, sessId, srcIp, dstIp, dstPort, macSa); */
/*   tcpSess[sessId]->bind(); */

/*   suppressOldTcpSess(sessId, */
/* 		     tcpSess[sessId]->srcIp,tcpSess[sessId]->srcPort, */
/* 		     tcpSess[sessId]->dstIp,tcpSess[sessId]->dstPort); */


/*   dev->snDev->set_fast_session(coreId,sessId, */
/* 			       tcpSess[sessId]->srcIp,tcpSess[sessId]->srcPort, */
/* 			       tcpSess[sessId]->dstIp,tcpSess[sessId]->dstPort); */

/*   tcpSess[sessId]->connect(); */

/*   tcpSess[sessId]->preloadNwHeaders(); */

/*   uint64_t val = eka_read(dev, SW_STATISTICS); */

/*   val = val & 0xffffffffffffff0f; */
/*   val = val | ((++dev->totalNumTcpSess) & 0xf)<<4; */
/*   eka_write(dev, SW_STATISTICS, val); */
/*   dev->addTcpSessMtx.unlock(); */

/*   return tcpSess[sessId]->sock; */
/* } */
/* ------------------------------------------------------------- */
/* uint EkaCore::addUdpSess(uint32_t ip, uint16_t port) { */
/*   for (uint i = 0; i < udpSessions; i++) { */
/*     if (udpSess[i]->myParams(ip,port)) return i; */
/*   } */
/*   if (udpSessions == EKA_MAX_UDP_SESSIONS_PER_CORE) on_error("udpSessions = %u",tcpSessions); */
/*   uint8_t sessId = udpSessions++; */
/*   udpSess[sessId] = new EkaUdpSess(dev, sessId, ip, port); */
/*   return sessId; */
/* } */

/* ------------------------------------------------------------- */

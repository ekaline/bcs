#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"
#include "EkaUdpChannel.h"

void initLwipNetIf(void** pLwipNetIf, EkaDev* dev, uint8_t coreId, uint8_t* macSa, uint8_t* macDa, uint32_t srcIp);

/* ------------------------------------------------------------- */
#define ARP_STRING_LEN  1023
#define ARP_BUFFER_LEN  (ARP_STRING_LEN + 1)

/**
 * Macros to turn a numeric macro into a string literal.  See
 * https://gcc.gnu.org/onlinedocs/cpp/Stringification.html
 */
#define xstr(s) str(s)
#define str(s) #s

/* Format for fscanf() to read the 1st, 4th, and 6th space-delimited fields */
#define ARP_LINE_FORMAT "%" xstr(ARP_STRING_LEN) "s %*s %*s " \
                        "%" xstr(ARP_STRING_LEN) "s %*s " \
                        "%" xstr(ARP_STRING_LEN) "s"

static bool getMacDaFromArp(const char* arpTableFile,uint8_t coreId, uint8_t* macDa) {
  FILE *arpCache = fopen(arpTableFile, "r");
  if (arpCache == NULL) on_error("cannot open ARP Cache %s",arpTableFile);

  char header[ARP_BUFFER_LEN] = {};
  if (!fgets(header, sizeof(header), arpCache)) return false;

  char ipAddr[ARP_BUFFER_LEN], hwAddr[ARP_BUFFER_LEN], device[ARP_BUFFER_LEN];

  while (3 == fscanf(arpCache, ARP_LINE_FORMAT, ipAddr, hwAddr, device)) {
    char myName[10] = {};
    sprintf (myName,"feth%u",coreId);
    if (strcmp(myName,device) == 0) {
      sscanf(hwAddr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &macDa[0], &macDa[1], &macDa[2], &macDa[3], &macDa[4], &macDa[5]);
      //      EKA_LOG("%s destination MAC is %s = %s",myName,hwAddr,EKA_MAC2STR(macDa));
      fclose(arpCache);
      return true;
    }
    //    printf("%03d: Mac Address of [%s] on [%s] is \"%s\"\n", ++count, ipAddr, device, hwAddr);
  }
  fclose(arpCache);
  return false;
}
/* ------------------------------------------------------------- */

EkaCore::EkaCore(EkaDev* pEkaDev, uint8_t lane, uint32_t ip, uint8_t* mac) {
    dev = pEkaDev;
    coreId = lane;
    memcpy(macSa,mac,6);
    srcIp = ip;
    memset(macDa,0,6);
    connected = true;
    vlanTag = 0;

    const char* arpTableFile = "/proc/net/arp";
    if (getMacDaFromArp(arpTableFile,coreId,macDa))
      EKA_LOG("MAC DA is taken from %s : %s",arpTableFile,EKA_MAC2STR(macDa));

    EKA_LOG("Core %u: %s %s",coreId,EKA_MAC2STR(macSa),EKA_IP2STR(srcIp));
    for (uint i = 0; i < MAX_SESS_PER_CORE; i++) tcpSess[i] = NULL;
    tcpSessions = 0;
    udpSessions = 0;
    //    numBooks = 0;

    udpChannel = NULL;

    // PATCH!!!!
    /* if (coreId == 0)  */initLwipNetIf(&pLwipNetIf,dev,coreId,macSa,macDa,srcIp);

    // Control session for ARPs
    tcpSess[MAX_SESS_PER_CORE] = new EkaTcpSess(dev, this, coreId, MAX_SESS_PER_CORE,
						0 /* srcIp */, 0 /* dstIp */, 0 /* dstPort */, 
						macSa, macDa,
						MAX_SESS_PER_CORE);
  }

/* ------------------------------------------------------------- */

EkaCore::~EkaCore() {
  if (udpChannel != NULL) delete udpChannel;
}

/* ------------------------------------------------------------- */
EkaTcpSess* EkaCore::findTcpSess(uint32_t ipSrc, uint16_t srcPort, uint32_t ipDst, uint16_t dstPort) {
  for (uint i = 0; i < tcpSessions; i++) {
    if (tcpSess[i] == NULL) on_error("Core %u, tcpSess[%u] == NULL",coreId,i);
    if (tcpSess[i]->myParams(ipSrc,srcPort,ipDst,dstPort)) return tcpSess[i];
  }
  return NULL;
}
/* ------------------------------------------------------------- */
EkaTcpSess* EkaCore::findTcpSess(int sock) {
  for (uint i = 0; i < tcpSessions; i++) {
    if (tcpSess[i] == NULL) on_error("Core %u, tcpSess[%u] == NULL",coreId,i);
    if (tcpSess[i]->myParams(sock)) return tcpSess[i];
  }
  return NULL;
}
/* ------------------------------------------------------------- */
EkaUdpSess* EkaCore::findUdpSess(uint32_t ip, uint16_t port) {
  for (uint i = 0; i < udpSessions; i++) {
    if (udpSess[i] == NULL) on_error("Core %u, udpSess[%u] == NULL",coreId,i);
    if (udpSess[i]->myParams(ip,port)) return udpSess[i];
  }
  return NULL;
}
/* ------------------------------------------------------------- */
uint EkaCore::addTcpSess() {
  if (tcpSessions == MAX_SESS_PER_CORE) on_error("tcpSessions = %u",tcpSessions);

  uint8_t sessId = tcpSessions++;
  tcpSess[sessId] = new EkaTcpSess(dev, this, coreId, sessId, srcIp, 0, 0, 
				   macSa, macDa,MAX_SESS_PER_CORE);
  return sessId;
}
/* ------------------------------------------------------------- */
int EkaCore::tcpConnect(uint32_t dstIp, uint16_t dstPort) {
  if (tcpSessions == MAX_SESS_PER_CORE) 
    on_error("tcpSessions = %u",tcpSessions);

  uint8_t sessId = tcpSessions++;
  EKA_LOG("Opening TCP session %u from core %u, srcIp = %s",sessId,coreId,EKA_IP2STR(srcIp));

  tcpSess[sessId] = new EkaTcpSess(dev, this, coreId, sessId, srcIp, dstIp, dstPort, 
				   macSa, macDa,MAX_SESS_PER_CORE);
  tcpSess[sessId]->bind();

  dev->snDev->set_fast_session(coreId,sessId,
			       tcpSess[sessId]->srcIp,tcpSess[sessId]->srcPort,
			       tcpSess[sessId]->dstIp,tcpSess[sessId]->dstPort);

  tcpSess[sessId]->connect();

  tcpSess[sessId]->preloadNwHeaders();

  uint64_t val = eka_read(dev, SW_STATISTICS);

  val = val & 0xffffffffffffff0f;
  val = val | ((++dev->totalNumTcpSess) & 0xf)<<4;
  eka_write(dev, SW_STATISTICS, val);
  return tcpSess[sessId]->sock;
}
/* ------------------------------------------------------------- */
uint EkaCore::addUdpSess(uint32_t ip, uint16_t port) {
    for (uint i = 0; i < udpSessions; i++) {
      if (udpSess[i]->myParams(ip,port)) return i;
    }
    if (udpSessions == EKA_MAX_UDP_SESSIONS_PER_CORE) on_error("udpSessions = %u",tcpSessions);
    uint8_t sessId = udpSessions++;
    udpSess[sessId] = new EkaUdpSess(dev, sessId, ip, port);
    return sessId;
  }

/* ------------------------------------------------------------- */
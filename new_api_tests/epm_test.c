#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <iterator>
#include <thread>

#include <linux/sockios.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <sys/time.h>
#include <chrono>

#include "EkaDev.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Efh.h"
#include "Epm.h"
#include "EkaEpm.h"


#include "eka_macros.h"

#include "EkaCore.h"

/* --------------------------------------------- */

volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static EkaOpResult ekaRC = EKA_OPRESULT__OK;
/* --------------------------------------------- */

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected: keep_work = false\n",__func__);
  printf ("%s: exitting...\n",__func__);
  fflush(stdout);
  return;
}

/* --------------------------------------------- */

int createThread(const char* name, EkaThreadType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}
/* --------------------------------------------- */

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}
/* --------------------------------------------- */

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}
/* --------------------------------------------- */

void fireReportCb (const EpmFireReport *report, int nReports, void *ctx) {

}

/* --------------------------------------------- */
void tcpChild(EkaDev* dev, int sock, uint port) {
  pthread_setname_np(pthread_self(),"tcpServerChild");

  int bytes_read = -1;
  //  printf ("Running TCP Server for sock=%d, port = %u\n",sock,port);
  do {
    char line[1536] = {};
    bytes_read = recv(sock, line, sizeof(line), 0);
    if (bytes_read > 0) {
      /* EKA_LOG ("recived pkt: %s",line); */
      /* fflush(stderr); */
      send(sock, line, bytes_read, 0);
    }
  } while (keep_work);
  EKA_LOG(" -- closing");
  fflush(stdout);
  close(sock);
  //  keep_work = false;
  return;
}
/* --------------------------------------------- */
void tcpServer(EkaDev* dev, std::string ip, uint16_t port) {
  pthread_setname_np(pthread_self(),"tcpServerParent");

  printf("Starting TCP server: %s:%u\n",ip.c_str(),port);
  int sd = 0;
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEADDR) failed");

  struct linger so_linger = {
    true, // Linger ON
    0     // Abort pending traffic on close()
  };

  if (setsockopt(sd,SOL_SOCKET,SO_LINGER,&so_linger,sizeof(struct linger)) < 0) 
    on_error("Cant set SO_LINGER");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = be16toh(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sd,(struct sockaddr*)&addr, sizeof(addr)) != 0 ) 
    on_error("failed to bind server sock to %s:%u",EKA_IP2STR(addr.sin_addr.s_addr),be16toh(addr.sin_port));
  if ( listen(sd, 20) != 0 ) on_error("Listen");
  serverSet = true;
  while (keep_work) {
    int childSock, addr_size = sizeof(addr);

    childSock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
    EKA_LOG("Connected from: %s:%d -- sock=%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),childSock);
    std::thread child(tcpChild,dev,childSock,be16toh(addr.sin_port));
    child.detach();
  }
  EKA_LOG(" -- closing");
}

/* --------------------------------------------- */
void tcpRxClientLoop(EkaDev* dev, ExcConnHandle conn) {
  rxClientReady = true;
  while (keep_work) {
    char rxBuf[1000] = {};
    int rxsize = excRecv(dev,conn, rxBuf, sizeof(rxBuf));
    //   if (rxsize < 1) on_error("rxsize < 1");
    if (rxsize > 0) EKA_LOG("\n%s\n",rxBuf);
  }
}
/* --------------------------------------------- */


ExcConnHandle runTcpClient(EkaDev* dev, EkaCoreId coreId,sockaddr_in* dst,uint16_t serverTcpPort) {
  int sock = excSocket(dev,coreId,0,0,0);
  if (sock < 0) on_error("failed to open sock");

  EKA_LOG ("Trying to connect to %s:%u on sock = %d",
	   EKA_IP2STR(dst->sin_addr.s_addr),
	   be16toh(dst->sin_port),
	   sock);
  ExcConnHandle conn = excConnect(dev,sock,(struct sockaddr*) dst, sizeof(struct sockaddr_in));
  if (conn < 0) on_error("excConnect %s:%u",EKA_IP2STR(dst->sin_addr.s_addr),be16toh(dst->sin_port));

  std::thread rxLoop = std::thread(tcpRxClientLoop,dev,conn);
  rxLoop.detach();

  while (!rxClientReady && keep_work) sleep(0);

  const char* pkt = "\n\nHello world! Im staring the test TCP session\n\n";
  excSend (dev, conn, pkt, strlen(pkt));

  return conn;
}

/* --------------------------------------------- */


void triggerGenerator(EkaDev* dev, const sockaddr_in* triggerAddr,const sockaddr_in* udpSenderAddr, EpmTrigger* epmTrigger,uint numTriggers) {
  EKA_LOG("\n=======================\nStarting triggerGenerator: %s:%u --> %s:%u, numTriggers=%u\n",
	  EKA_IP2STR(udpSenderAddr->sin_addr.s_addr),be16toh(udpSenderAddr->sin_port),
	  EKA_IP2STR(triggerAddr->sin_addr.s_addr),  be16toh(triggerAddr->sin_port),numTriggers);

  int sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (sock < 0) on_error("failed to open UDP sock");

  /* struct ifreq interface; */
  /* strncpy(interface.ifr_ifrn.ifrn_name, "eth1", IFNAMSIZ); */
  /* if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, */
  /*                      (char *)&interface, sizeof(interface)) < 0) { */
  /*   on_error("failed SO_BINDTODEVICE to eth1"); */
  /* } */

  if (bind(sock,(sockaddr*)udpSenderAddr, sizeof(sockaddr)) != 0 ) {
    on_error("failed to bind server sock to %s:%u",EKA_IP2STR(udpSenderAddr->sin_addr.s_addr),be16toh(udpSenderAddr->sin_port));
  } else {
    EKA_LOG("udpSenderAddr sock is binded to %s:%u",EKA_IP2STR(udpSenderAddr->sin_addr.s_addr),be16toh(udpSenderAddr->sin_port));
  }
  for(uint i = 0; i < numTriggers; i++) {
    printf("Sending trigger %d to %s:%u\n",i,EKA_IP2STR(triggerAddr->sin_addr.s_addr),be16toh(triggerAddr->sin_port));
    if (sendto(sock,&epmTrigger[i],sizeof(EpmTrigger),0,(const sockaddr*) triggerAddr,sizeof(sockaddr)) < 0) 
      on_error("Failed to send trigegr to %s:%u",EKA_IP2STR(triggerAddr->sin_addr.s_addr),be16toh(triggerAddr->sin_port));
    sleep (1);
  }

  triggerGeneratorDone = true;
}

/* --------------------------------------------- */

void printUsage(char* cmd) {
  printf("USAGE: %s -s <Connection Server IP> -p <Connection Server TcpPort> -c <Connection Client IP> -t <Trigger IP> -u <Trigger UdpPort> \n",cmd); 
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::string* serverIp, uint16_t* serverTcpPort, 
		   std::string* clientIp, 
		   std::string* triggerIp, uint16_t* triggerUdpPort) {
  int opt; 
  while((opt = getopt(argc, argv, ":c:s:p:u:t:h")) != -1) {  
    switch(opt) {  
      case 's':  
	*serverIp = std::string(optarg);
	printf("serverIp = %s\n", (*serverIp).c_str());  
	break;  
      case 'c':  
	*clientIp = std::string(optarg);
	printf("clientIp = %s\n", (*clientIp).c_str());  
	break;  
      case 'p':  
	*serverTcpPort = atoi(optarg);
	printf("serverTcpPort = %u\n", *serverTcpPort);  
	break;  
      case 't':  
	*triggerIp = std::string(optarg);
	printf("triggerIp = %s\n", (*triggerIp).c_str());  
	break;  
      case 'u':  
	*triggerUdpPort = atoi(optarg);
	printf("triggerUdpPort = %u\n", *triggerUdpPort);  
	break;  
      case 'h':  
	printUsage(argv[0]);
	exit (1);
	break;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  

  /* if ((*clientIp).empty() || (*serverIp).empty() || (*triggerIp).empty() || *serverTcpPort == 0 || *triggerUdpPort == 0) { */
  /*   printUsage(argv[0]); */
  /*   on_error("missing params: clientIp=%s, serverIp=%s, serverTcpPort=%u, triggerUdpPort=%u", */
  /* 	     (*clientIp).c_str(),(*serverIp).c_str(),*serverTcpPort,*triggerUdpPort); */
  /* } */
  return 0;
}
/* --------------------------------------------- */

/* [6/16 5:33 PM] Igor Galitskiy */
    
/* multicast IPs: */
/* 239.255.119.16 & 239.255.119.17 */


/* nqxlxavt059d */
/*  publish: sfc0 10.120.115.53 */
/*  receive: fpga0_0 10.120.115.56 */


/* nqxlxavt061d */
/*  publish: sfc0 10.120.115.52 */
/*  receive: fpga0_0 10.120.115.54 */





/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  EkaDev* dev = NULL;
  EkaCoreId coreId = 0;

  EkaDevInitCtx ekaDevInitCtx = {
    .logCallback = NULL,
    .logContext  = NULL,
    .credAcquire = credAcquire,
    .credRelease  = credRelease,
    .credContext = NULL,
    .createThread = createThread,
    .createThreadContext = NULL
  };
  ekaDevInit(&dev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  std::string triggerIp = "239.255.119.16";
  std::string serverIp  = "10.0.0.10";
  std::string clientIp  = "100.0.0.110"; //std::string(EKA_IP2STR(dev->core[0]->srcIp));

  uint16_t serverTcpPort  = 22222;
  uint16_t triggerUdpPort = 18333;

  getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort);

  EKA_LOG("\n==============================\nUDP Trigger: %s:%u, Actions Server %s:%u, Client IP %s\n==============================",
	  triggerIp.c_str(),triggerUdpPort,serverIp.c_str(),serverTcpPort,clientIp.c_str());

  std::thread server = std::thread(tcpServer,dev,serverIp,serverTcpPort);
  server.detach();

  while (keep_work && ! serverSet) { sleep (0); }

  EkaCoreInitCtx ekaCoreInitCtx = {
    .coreId = coreId,
    .attrs = {
      .host_ip = inet_addr(clientIp.c_str()),
      .netmask = inet_addr("255.255.255.0"),
      .gateway = inet_addr(clientIp.c_str()),
      .nexthop_mac  = {}, //{0x00,0x0F,0x53,0x08,0xED,0xD5},
      .src_mac_addr = {}, //{0x00,0x21,0xB2,0x19,0x0F,0x30},
      .dont_garp    = 0
    }
  };
  ekaDevConfigurePort (dev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);

  struct sockaddr_in serverAddr = {};
  serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
  serverAddr.sin_family      = AF_INET;
  serverAddr.sin_port        = be16toh(serverTcpPort);


  ExcConnHandle conn = runTcpClient(dev, coreId,&serverAddr,serverTcpPort);

  /* ============================================== */
  static const int ChainRows = 4;
  static const int ChainCols = 8;

  int actionChain[ChainRows][ChainCols] = {
    {1,   51,  8, 13, 0},
    {4,  0 },
    {47, 0 },
    {100, 15, 21, 49, 17, 31, 0}
  };

  /* ============================================== */

  struct sockaddr_in triggerDst = {};
  triggerDst.sin_family      = AF_INET;  
  triggerDst.sin_addr.s_addr = inet_addr(triggerIp.c_str());
  triggerDst.sin_port        = be16toh(triggerUdpPort);

  const epm_strategyid_t numStrategies = 7;
  EpmStrategyParams strategyParams[numStrategies] = {};
  for (auto i = 0; i < numStrategies; i++) {
    strategyParams[i].numActions = 200;
    strategyParams[i].triggerAddr = reinterpret_cast<const sockaddr*>(&triggerDst);
    strategyParams[i].reportCb = fireReportCb;
  }

  EKA_LOG("Before: epmInitStrategies");
  ekaRC = epmInitStrategies(dev, coreId, strategyParams, numStrategies);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmInitStrategies failed: ekaRC = %u",ekaRC);

  uint nwHdrOffset = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset  = 0;

  for (auto stategyIdx = 0; stategyIdx < numStrategies; stategyIdx++) {
    for (auto chainIdx = 0; chainIdx < ChainRows; chainIdx++) {
      bool imLast = false;
      for (auto actionIdx = 0; actionIdx < ChainCols && ! imLast; actionIdx++) {
	imLast = (actionIdx == ChainCols - 1) || (actionChain[chainIdx][actionIdx+1] == 0);
	epm_actionid_t nextAction = imLast ? EPM_LAST_ACTION : actionChain[chainIdx][actionIdx+1];
	char pkt2send[1000] = {};
	sprintf(pkt2send,"Action Pkt: strategy=%d, action-in-chain=%d, actionId=%u, next=%u EOP",
		stategyIdx,actionIdx,static_cast<uint>(actionChain[chainIdx][actionIdx]),
		nextAction);
	pkt2send[strlen(pkt2send)] = '\n'; // replacing first '\0' for future printf
	uint32_t payloadSize = strlen(pkt2send);
	/* EKA_LOG("\t%s",pkt2send); */
	heapOffset = roundUp<uint>(heapOffset,8);
	heapOffset += nwHdrOffset;
	EpmAction epmAction = {
	  .token         = static_cast<epm_token_t>(0), ///< Security token
	  .hConn         = conn,                        ///< TCP connection where segments will be sent
	  .offset        = heapOffset,                  ///< Offset to payload in payload heap
	  .length        = payloadSize,                 ///< Payload length
	  .actionFlags   = AF_Valid,                    ///< Behavior flags (see EpmActionFlag)
	  .nextAction    = nextAction,                  ///< Next action in sequence, or EPM_LAST_ACTION
	  .enable        = 0x01,                        ///< Enable bits
	  .postLocalMask = 0x01,                        ///< Post fire: enable & mask -> enable
	  .postStratMask = 0x01,                        ///< Post fire: strat-enable & mask -> strat-enable
	  .user          = static_cast<uintptr_t>
	  (actionChain[chainIdx][actionIdx])                        ///< Opaque value copied into `EpmFireReport`.
	};
	ekaRC = epmPayloadHeapCopy(dev, coreId,
				  static_cast<epm_strategyid_t>(stategyIdx),
				  heapOffset,
				  payloadSize,
				  (const void *)pkt2send);

	if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %u",ekaRC);
	heapOffset += payloadSize;
	heapOffset += fcsOffset;
	
	ekaRC = epmSetAction(dev, coreId, stategyIdx, 
			     actionChain[chainIdx][actionIdx], 
			     &epmAction);
	if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %u",ekaRC);


      }
    }
  }
  /* ============================================== */
  ekaRC = epmEnableController(dev,coreId,true);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %u",ekaRC);

  /* ============================================== */

  EpmTrigger epmTrigger[] = {
    /* token, strategy, action */
    /* {   11,       1,       1}, */
    /* {   7,        2,       4}, */
    /* {   9,        3,      47}, */
    {   17,       4,     100},
  };
  uint numTriggers = sizeof(epmTrigger)/sizeof(epmTrigger[0]);

  /* ============================================== */


  struct sockaddr_in udpSenderAddr = {};
  udpSenderAddr.sin_family      = AF_INET;
  udpSenderAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
  udpSenderAddr.sin_port        = be16toh(22221);

  std::thread trigger = std::thread(triggerGenerator,dev,&triggerDst,&udpSenderAddr,epmTrigger,numTriggers);
  trigger.detach();

  /* ============================================== */


  while (keep_work && ! triggerGeneratorDone) sleep(0);
  keep_work = false;
  sleep(3);

  /* ============================================== */


  excClose(dev,conn);
  printf("Closing device\n");

  sleep (1);
  ekaDevClose(dev);

  return 0;
}

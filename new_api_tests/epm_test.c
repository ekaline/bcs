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

#include "eka_dev.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Efh.h"
#include "Epm.h"
#include "EkaEpm.h"

#include "eka_macros.h"


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

void hexDump (const char* desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    if (desc != NULL) printf ("%s:\n", desc);
    if (len == 0) { printf("  ZERO LENGTH\n"); return; }
    if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) printf ("  %s\n", buff);
            printf ("  %04x ", i);
        }
        printf (" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
        else buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) { printf ("   "); i++; }
    printf ("  %s\n", buff);
}


/* --------------------------------------------- */

int createThread(const char* name, EkaThreadType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}
/* --------------------------------------------- */

int credAcquire(EkaCredentialType credType, EkaSource source, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s\n",user,EKA_EXCH_DECODE(source));
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
  int bytes_read = -1;
  //  printf ("Running TCP Server for sock=%d, port = %u\n",sock,port);
  do {
    char line[1536] = {};
    bytes_read = recv(sock, line, sizeof(line), 0);
    //    printf ("%u: %s\n",sock,line);
    fflush(stdout);
    send(sock, line, bytes_read, 0);
  } while (keep_work);
  EKA_LOG("%u: bytes_read = %d -- closing\n",sock,bytes_read);
  fflush(stdout);
  close(sock);
  keep_work = false;
  return;
}
/* --------------------------------------------- */
void tcpServer(EkaDev* dev, std::string ip, uint16_t port) {
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
}

/* --------------------------------------------- */
void tcpRxClientLoop(EkaDev* dev, ExcConnHandle conn) {
  while (keep_work) {
    char rxBuf[1000] = {};
    int rxsize = excRecv(dev,conn, rxBuf, sizeof(rxBuf));
    //   if (rxsize < 1) on_error("rxsize < 1");
    if (rxsize > 0) EKA_LOG("\n\t%s\n",rxBuf);
    rxClientReady = true;
  }
}
/* --------------------------------------------- */


ExcConnHandle runTcpClient(EkaDev* dev, EkaCoreId coreId,std::string serverIp,uint16_t serverTcpPort) {
  struct sockaddr_in dst = {};
  dst.sin_addr.s_addr = inet_addr(serverIp.c_str());
  dst.sin_family      = AF_INET;
  dst.sin_port        = be16toh(serverTcpPort);

  int sock = excSocket(dev,coreId,0,0,0);
  if (sock < 0) on_error("failed to open sock");

  EKA_LOG ("Trying to connect to %s:%u",EKA_IP2STR(dst.sin_addr.s_addr),be16toh(dst.sin_port));
  ExcConnHandle conn = excConnect(dev,sock,(struct sockaddr*) &dst, sizeof(struct sockaddr_in));
  if (conn < 0) on_error("excConnect %s:%u",EKA_IP2STR(dst.sin_addr.s_addr),be16toh(dst.sin_port));

  std::thread rxLoop = std::thread(tcpRxClientLoop,dev,conn);
  rxLoop.detach();

  while (!rxClientReady) sleep(0);

  const char* pkt = "\n\nHello world! Im staring the test TCP session\n\n";
  excSend (dev, conn, pkt, strlen(pkt));

  return conn;
}

/* --------------------------------------------- */


void triggerGenerator(EkaDev* dev, const sockaddr_in* triggerAddr,EpmTrigger* epmTrigger,uint numTriggers) {
  EKA_LOG("\n=======================\nStrating triggerGenerator: %s:%u, numTriggers=%u\n",
	  EKA_IP2STR(triggerAddr->sin_addr.s_addr),be16toh(triggerAddr->sin_port),numTriggers);

  int sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (sock < 0) on_error("failed to open UDP sock");

  struct ifreq interface;

  strncpy(interface.ifr_ifrn.ifrn_name, "eth1", IFNAMSIZ);
  if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
                       (char *)&interface, sizeof(interface)) < 0) {
    on_error("failed SO_BINDTODEVICE to eth1");
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
	break;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  

  if ((*clientIp).empty() || (*serverIp).empty() || (*triggerIp).empty() || *serverTcpPort == 0 || *triggerUdpPort == 0) {
    printUsage(argv[0]);
    on_error("missing params: clientIp=%s, serverIp=%s, serverTcpPort=%u, triggerUdpPort=%u",
	     (*clientIp).c_str(),(*serverIp).c_str(),*serverTcpPort,*triggerUdpPort);
  }
  return 0;
}
/* --------------------------------------------- */


/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  std::string clientIp, serverIp, triggerIp;
  uint16_t serverTcpPort  = 0;
  uint16_t triggerUdpPort = 0;;

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

  getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort);

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

  ExcConnHandle conn = runTcpClient(dev, coreId,serverIp,serverTcpPort);

  /* ============================================== */
  static const int ChainRows = 4;
  static const int ChainCols = 8;

  int actionChain[ChainRows][ChainCols] = {
    {1, 51, 8, 13, -1},
    {4, -1      },
    {47, -1     },
    {100, 15, 21, 49, 17, 31, -1}
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
  ekaRC = epmInitStrategies(dev, coreId, strategyParams, numStrategies);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmInitStrategies failed: ekaRC = %u",ekaRC);

  uint32_t heapOffset = 0;

  for (auto stategyIdx = 0; stategyIdx < numStrategies; stategyIdx++) {
    for (auto chainIdx = 0; chainIdx < ChainRows; chainIdx++) {
      bool imLast = false;
      for (auto actionIdx = 0; actionIdx < ChainCols && ! imLast; actionIdx++) {
	imLast = (actionIdx == ChainCols - 1) || (actionChain[chainIdx][actionIdx+1] == -1);
	epm_actionid_t nextAction = imLast ? EPM_LAST_ACTION : actionChain[chainIdx][actionIdx+1];
	char pkt2send[1000] = {};
	sprintf(pkt2send,"Action Pkt: strategy=%d, action-in-chain=%d, actionId=%u, next=%u",
		stategyIdx,actionIdx,static_cast<uint>(actionChain[chainIdx][actionIdx]),
		nextAction);

	EKA_LOG("\t%s",pkt2send);
	EpmAction epmAction = {
	  .token         = static_cast<epm_token_t>(0), ///< Security token
	  .hConn         = conn,                        ///< TCP connection where segments will be sent
	  .offset        = heapOffset,                  ///< Offset to payload in payload heap
	  .length        = (uint32_t)strlen(pkt2send),  ///< Payload length
	  .actionFlags   = AF_Valid,                    ///< Behavior flags (see EpmActionFlag)
	  .nextAction    = nextAction,                  ///< Next action in sequence, or EPM_LAST_ACTION
	  .enable        = 0x01,                        ///< Enable bits
	  .postLocalMask = 0x01,                        ///< Post fire: enable & mask -> enable
	  .postStratMask = 0x01,                        ///< Post fire: strat-enable & mask -> strat-enable
	  .user          = static_cast<uintptr_t>
	  (actionChain[chainIdx][actionIdx])                        ///< Opaque value copied into `EpmFireReport`.
	};
	ekaRC = epmSetAction(dev, coreId, stategyIdx, 
			     actionChain[chainIdx][actionIdx], 
			     &epmAction);
	if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %u",ekaRC);

	ekaRC = epmPayloadHeapCopy(dev, coreId,
				  static_cast<epm_strategyid_t>(stategyIdx),
				  heapOffset,
				  (uint32_t)strlen(pkt2send),
				  (const void *)pkt2send);

	if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %u",ekaRC);
	heapOffset += strlen(pkt2send);

      }
    }
  }
  /* ============================================== */
  ekaRC = epmEnableController(dev,coreId,true);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %u",ekaRC);

  /* ============================================== */

  EpmTrigger epmTrigger[] = {
    /* token, strategy, action */
    {   11,       1,       1},
    {   7,        2,       4},
    {   9,        3,      47},
    {   17,       4,     100},
  };
  uint numTriggers = sizeof(epmTrigger)/sizeof(epmTrigger[0]);

  /* ============================================== */

  std::thread trigger = std::thread(triggerGenerator,dev,&triggerDst,epmTrigger,numTriggers);
  trigger.detach();

  /* ============================================== */


  while (keep_work && ! triggerGeneratorDone) sleep(0);

  sleep(3);

  /* ============================================== */


  excClose(dev,conn);
  printf("Closing device\n");

  sleep (1);
  ekaDevClose(dev);

  return 0;
}

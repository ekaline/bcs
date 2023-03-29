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

#include "eka_macros.h"

#include "EkaCtxs.h"
#include "EkaEfcDataStructs.h"
#include "EkaFhCmeParser.h"

#include <fcntl.h>
#include "ekaNW.h"
#include "EfcCme.h"

/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;

/* --------------------------------------------- */

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
  return;
}


/* --------------------------------------------- */
void tcpServer(std::string ip,
	       uint16_t port) {
  pthread_setname_np(pthread_self(),"tcpServerParent");

  printf("Starting TCP server: %s:%u\n",ip.c_str(),port);
  int sd = 0;
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &one_const, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEPORT) failed");

  struct linger so_linger = {
    true, // Linger ON
    0     // Abort pending traffic on close()
  };

  if (setsockopt(sd,SOL_SOCKET,SO_LINGER,
		 &so_linger,sizeof(so_linger)) < 0) 
    on_error("Cant set SO_LINGER");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = be16toh(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sd,(struct sockaddr*)&addr, sizeof(addr)) != 0 ) 
    on_error("failed to bind server sock to %s:%u",EKA_IP2STR(addr.sin_addr.s_addr),be16toh(addr.sin_port));
  if ( listen(sd, 20) != 0 ) on_error("Listen");
  serverSet = true;

  int addr_size = sizeof(addr);
  int sock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
  EKA_LOG("Connected from: %s:%d -- sock=%d\n",
	  inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),sock);

  int status = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
  if (status == -1)  on_error("fcntl error");
  /* int i = 0; */
  while (keep_work) {
    char rxBuf[1536] = {};
    int rc = recv(sock, rxBuf, sizeof(rxBuf), 0);
    if (rc > 0) {
    /*   TEST_LOG("%4d: RX: %s\n",++i,rxBuf); */
    }
  }
  return;
}


/* ############################################# */
int main(int argc, char *argv[]) {
  
    signal(SIGINT, INThandler);
    // ==============================================

    std::string serverIp        = "10.0.0.10";      // Ekaline lab default
    uint16_t serverTcpPort      = 22345;            // Ekaline lab default
    std::string clientIp        = "100.0.0.110";    // Ekaline lab default

    // ==============================================
    // EkaDev general setup
    EkaDev*     dev = NULL;
    EkaCoreId   coreId = 0;
    EkaOpResult rc;
    const EkaDevInitCtx ekaDevInitCtx = {};

    ekaDevInit(&dev, &ekaDevInitCtx);

    // ==============================================
    // 10G Port (core) setup
    const EkaCoreInitCtx ekaCoreInitCtx = {
	.coreId = coreId,
	.attrs = {
	    .host_ip      = inet_addr(clientIp.c_str()),
	    .netmask      = inet_addr("255.255.255.0"),
	    .gateway      = inet_addr("10.0.0.10"),
	}
    };
    ekaDevConfigurePort (dev, &ekaCoreInitCtx);

    // ==============================================
    // Launching TCP test Servers

    std::thread server = std::thread(tcpServer,
				     serverIp,
				     serverTcpPort);
    server.detach();
    while (keep_work && ! serverSet) sleep (0);

    // ==============================================
    // Establishing EXC connections for EPM/EFC fires 

    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
    serverAddr.sin_port        = be16toh(serverTcpPort);

    int excSock = excSocket(dev,coreId,0,0,0);
    if (excSock < 0)
      on_error("failed to open excSocket");
    ExcConnHandle conn = excConnect(dev,excSock,(sockaddr*) &serverAddr, sizeof(sockaddr_in));
    if (conn < 0)
      on_error("failed excConnect to %s:%u",
	       EKA_IP2STR(serverAddr.sin_addr.s_addr),
	       be16toh(serverAddr.sin_port));


    // ==============================================
    // Setup EFC MC groups

    EpmTriggerParams triggerParam[] = {
	{0,"224.0.74.0",30301},
	/* {0,"224.0.74.1",30302}, */
	/* {0,"224.0.74.2",30303}, */
	/* {0,"224.0.74.3",30304}, */
    };

    EfcCtx efcCtx = {};
    EfcCtx* pEfcCtx = &efcCtx;

    EfcInitCtx initCtx = {
	.feedVer = EfhFeedVer::kCME
    };  
    rc = efcInit(&pEfcCtx,dev,&initCtx);
    if (rc != EKA_OPRESULT__OK)
	on_error("efcInit returned %d",(int)rc);

    // ==============================================
    // Configuring EFC as EPM Strategy

    const EpmStrategyParams efcEpmStrategyParams = {
	.numActions    = 256,          // just a number
	.triggerParams = triggerParam,         
	.numTriggers   = std::size(triggerParam),
	.reportCb      = NULL,         // set via EfcRunCtx
	.cbCtx         = NULL
    };
    rc = epmInitStrategies(dev, &efcEpmStrategyParams, 1);
    if (rc != EKA_OPRESULT__OK)
	on_error("epmInitStrategies failed: rc = %d",rc);

    // ==============================================
    // Global EFC config
    EfcStratGlobCtx efcStratGlobCtx = {
	.enable_strategy      = 1,
	.report_only          = 0,
	.watchdog_timeout_sec = 100000,
    };
    efcInitStrategy(pEfcCtx, &efcStratGlobCtx);

    EfcRunCtx runCtx = {};
    runCtx.onEfcFireReportCb = efcPrintFireReport;
    // ==============================================
    // CME FastCancel EFC config
    static const uint64_t CmeTestFastCancelAlwaysFire = 0xadcd;
    static const uint64_t CmeTestFastCancelToken = 0x1122334455667788;
    static const uint64_t CmeTestFastCancelUser  = 0xaabbccddeeff0011;
    static const uint16_t CmeTestFastCancelMaxMsgSize = 1300;
    static const uint8_t  CmeTestFastCancelMinNoMDEntries = 1;

    const EfcCmeFastCancelParams params = {
	.maxMsgSize     = CmeTestFastCancelMaxMsgSize,
	.minNoMDEntries = CmeTestFastCancelMinNoMDEntries,
	.token          = CmeTestFastCancelToken
    };

    uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
    uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
    uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
    uint heapOffset    = 0;
    uint32_t maxPayloadLen = 1536 - nwHdrOffset - fcsOffset;
    
    EpmAction cmeAction[(size_t)EfcCmeActionId::Count] = {};

    for (epm_actionid_t actionId = 0; actionId < (epm_actionid_t)EfcCmeActionId::Count; actionId++) {
	cmeAction[actionId].type = efcCmeActionId2Type((EfcCmeActionId)actionId);
	cmeAction[actionId].token         = params.token;
	cmeAction[actionId].hConn         = conn;
	cmeAction[actionId].offset        = heapOffset + nwHdrOffset;
	cmeAction[actionId].length        = maxPayloadLen;
	cmeAction[actionId].actionFlags   = AF_Valid;
	cmeAction[actionId].nextAction    = EPM_LAST_ACTION;
	cmeAction[actionId].enable        = CmeTestFastCancelAlwaysFire;
	cmeAction[actionId].postLocalMask = CmeTestFastCancelAlwaysFire;
	cmeAction[actionId].postStratMask = CmeTestFastCancelAlwaysFire;
	cmeAction[actionId].user          = CmeTestFastCancelUser;
	
	epmSetAction(dev,EFC_STRATEGY,actionId,&cmeAction[actionId]);
	
	heapOffset += cmeAction[actionId].length + nwHdrOffset + fcsOffset;
	heapOffset += dataAlignment - (heapOffset % dataAlignment);
    }


    // ==============================================
    // Manually prepared CmeTestFastCancel message fired by FPGA
    const char CmeTestFastCancelMsg[] = "CME Fast Cancel: Sequence = |____| With Dummy payload";
    rc = epmPayloadHeapCopy(dev, 
			    EFC_STRATEGY,
			    cmeAction[(size_t)EfcCmeActionId::HwCancel].offset,
			    strlen(CmeTestFastCancelMsg),
			    CmeTestFastCancelMsg);
    cmeAction[(size_t)EfcCmeActionId::HwCancel].length = strlen(CmeTestFastCancelMsg);
    epmSetAction(dev,EFC_STRATEGY,(epm_actionid_t)EfcCmeActionId::HwCancel,
		             &cmeAction[(size_t)EfcCmeActionId::HwCancel]);
    
    if (rc != EKA_OPRESULT__OK) 
	    on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
		           cmeAction[(size_t)EfcCmeActionId::HwCancel].offset,
		           (uint)strlen(CmeTestFastCancelMsg),(int)rc);

    // ==============================================
    efcCmeFastCancelInit(dev,&params);
    // ==============================================
    //    efcEnableController(pEfcCtx, 0);
    // ==============================================
    efcRun(pEfcCtx, &runCtx );
    // ==============================================

    const int SwMsgLen = 762;
    const int NumMsgs = 64;

    char swCmeQuote[NumMsgs][SwMsgLen] = {};
    efcCmeSetILinkAppseq(dev,conn,0x1);

    /* ------------------------------------------------- */
    printf("Sending constant msg\n");
    for (auto i = 0; keep_work; i++) {
      efcCmeSend(dev,conn,swCmeQuote[0],SwMsgLen,0,true);
      if (i % 10000 == 0) printf(".");
    }
    
    printf("\n===========================\nEND OT TESTS\n===========================\n");

    fflush(stdout);fflush(stderr);


    /* ============================================== */

    printf("Closing device\n");

    ekaDevClose(dev);
    sleep(1);
  
    return 0;
}

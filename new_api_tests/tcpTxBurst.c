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
#include <string>

#include <sys/time.h>
#include <chrono>

#include "eka_macros.h"
#include "Exc.h"
#include "Eka.h"
#include "Efc.h"


#define CORES 2
#define SESSIONS 32

#define STATISTICS_PERIOD 1000000

static const uint     MaxTestCores    = 2;
static const uint     MaxTestSessions = 14;
static const size_t   MaxThreads = MaxTestCores * MaxTestSessions;

static const size_t   BufSize = 1024 * 1024;

volatile bool g_keepWork = true;;
volatile bool g_serverSet = false;

volatile bool  g_sessRcvThreadReady[MaxThreads] = {};
volatile char* g_bufPtr[MaxThreads] = {};
volatile bool  g_bufVerified[MaxThreads];

void excSendThr_f(EkaDev* pEkaDev,
		       ExcConnHandle hCon,uint thrId,
		       uint p2p_delay) {
  uint8_t coreId = excGetCoreId(hCon);
  uint8_t sessId = excGetSessionId(hCon);
  TEST_LOG("Launching excSendThr for coreId %u, sessId %u, p2p_delay=%u",
	   coreId,sessId,p2p_delay);
 
  size_t bufCnt = 0;
  
  auto txBuf = new char[BufSize];
  if (! txBuf)
    on_error("! txBuf");
  g_bufPtr[thrId] = txBuf;
  g_bufVerified[thrId] = true;
  
  while (g_keepWork) {
    ++bufCnt;
    if (bufCnt % STATISTICS_PERIOD == 0) 
      TEST_LOG ("CoreId %u, SessId %u, bufCnt: %ju",
		coreId,sessId,bufCnt);
    if (g_bufVerified[thrId]) {
      g_bufVerified[thrId] = false;
      
      for (size_t i = 0; i < BufSize; i++)
	txBuf[i] = 'a' + rand() % ('z' -'a' + 1);

      /* -------------------------------------------------- */

      size_t sentBytes = 0;
      const char* p = txBuf;
      while (g_keepWork && sentBytes < BufSize) {
	/* static const size_t PktSize = 1360; */ // large pkt
	/* size_t currPktSize = std::min(BufSize - sentBytes, */
	/* 			      1 + rand() % PktSize); */
	size_t currPktSize = std::min(BufSize - sentBytes,
				      (uint64_t)708);
	int rc = excSend (pEkaDev, hCon, p, currPktSize, 0);
	/* ------------------- */
	switch (rc) {
	case -1 :
	  TEST_LOG("%u_%u_%2u: Session dropped \'%s\'",
		   thrId,coreId,sessId,strerror(errno));
	  g_keepWork = false;
	  break;
	case 0 :
	  if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    /* TEST_LOG("Retrying pkt %ju",cnt); */
	    /* send same again */
	    std::this_thread::yield();

	  } else {
	    g_keepWork = false;
	    EKA_ERROR("Unexpected errno=%d (\'%s\')",
		      errno,strerror(errno));
	  }
	  break;	
	default:
	  sentBytes += rc;
	  p += rc;
	}
	/* ------------------- */

      }
      if (p2p_delay != 0) usleep (p2p_delay);
    }
  }
  TEST_LOG("excSendThr_f[%u] is terminated",thrId);
  delete[] g_bufPtr[thrId];
  return;
}
/* --------------------------------------------- */
void excRecvThr_f(EkaDev* pEkaDev,
		  ExcConnHandle hCon,uint thrId) {
  uint8_t coreId = excGetCoreId(hCon);
  uint8_t sessId = excGetSessionId(hCon);
  TEST_LOG("Launching excRecvThr for coreId %u, sessId %u",
	   coreId,sessId);
  g_sessRcvThreadReady[thrId] = true;

  size_t rxCnt = 0;  
  char rxBuf[BufSize] = {};
  char* rxP = rxBuf;
    
  while (g_keepWork) {
    int rx = excRecv(pEkaDev,hCon, rxP, BufSize - rxCnt, 0);
    if (rx <= 0) {
      if (errno != EAGAIN)
	on_error("core %u, sess %u: excRecv()=%d (\'%s\')",
		 coreId,sessId,rx,strerror(errno));
      std::this_thread::yield();
     continue;
    }

    rxCnt += rx;
    rxP += rx;
    if (rxCnt > BufSize)
      on_error("rxCnt %jd > BufSize %jd",rxCnt,BufSize);
    if (rxCnt == BufSize) {
      if (memcmp((const void*)g_bufPtr[thrId],rxBuf,BufSize) != 0) { 
	on_error("ThrId %u: RX != TX", thrId);       
      }
      rxP = rxBuf;
      rxCnt = 0;
      g_bufVerified[thrId] = true;
    }
  }


  TEST_LOG("excRecvThr_f[%u] is terminated",thrId);

  return;
}
/* --------------------------------------------- */


void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  g_keepWork = false;
  printf("%s:Ctrl-C detected: g_keepWork = false\n",__func__);
  printf ("%s: exitting...\n",__func__);
  fflush(stdout);
  return;
}
/* --------------------------------------------- */

void printUsage(char* cmd) {
  printf("USAGE: %s -s <Connection Server IP> -p <Connection Server TcpPort> -c <Connection Client IP> -t <testSessions> -r <testCores> -d <p2p_delay> \n",cmd); 
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::string* serverIp, uint16_t* serverTcpPort, 
		   std::string* clientIp, 
		   uint* testSessions, uint* testCores, uint* p2p_delay) {
  int opt; 
  while((opt = getopt(argc, argv, ":c:s:p:d:t:r:h")) != -1) {  
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
	*testSessions = atoi(optarg);
	printf("testSessions = %u\n", *testSessions);  
	break;  
      case 'r':  
	*testCores = atoi(optarg);
	printf("testCores = %u\n", *testCores);  
	break;  
      case 'd':  
	*p2p_delay = atoi(optarg);
	printf("p2p_delay = %u\n", *p2p_delay);  
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
  return 0;
}
/* --------------------------------------------- */
/* --------------------------------------------- */
void tcpChild(EkaDev* dev, int sock, uint port) {
  pthread_setname_np(pthread_self(),"tcpServerChild");
  //  printf ("Running TCP Server for sock=%d, port = %u\n",sock,port);
  do {
    char line[1536] = {};
    int rc = recv(sock, line, sizeof(line), 0);
    if (rc > 0)
      send(sock, line, rc, 0);
  } while (g_keepWork);
  TEST_LOG("tcpChild thread is terminated");
  fflush(stdout);
  close(sock);
  //  g_keepWork = false;
  return;
}
/* --------------------------------------------- */
void tcpServer(EkaDev* dev, std::string ip, uint16_t port) {
  printf("Starting TCP server: %s:%u\n",ip.c_str(),port);
  pthread_setname_np(pthread_self(),"tcpServerParent");

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
  g_serverSet = true;
  while (g_keepWork) {
    int childSock, addr_size = sizeof(addr);

    childSock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
    TEST_LOG("Connected from: %s:%d -- sock=%d\n",
	     inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),childSock);
    std::thread child(tcpChild,dev,childSock,be16toh(addr.sin_port));
    child.detach();
  }
  TEST_LOG(" -- closing");
}

/* --------------------------------------------- */


int main(int argc, char *argv[]) {
  g_keepWork = true;
  EkaDev* pEkaDev = NULL;

  signal(SIGINT, INThandler);

  uint     testCores      = 1;
  uint     testSessions   = 2;
  uint     p2p_delay      = 0; // microseconds

  struct testConnection {
    std::string srcIp;
    std::string dstIp;
    uint16_t    dstTcpPort;
  };

  testConnection conn[MaxTestCores] = {
    { std::string("100.0.0.110"), std::string("10.0.0.10"), 55555},
    { std::string("200.0.0.111"), std::string("10.0.0.11"), 22223}
  };

  getAttr(argc,argv,&conn[0].dstIp,&conn[0].dstTcpPort,&conn[0].srcIp,
	  &testSessions,&testCores,&p2p_delay);
  //-----------------------------------------
  printf ("Running with %u cores, %u sessions, %uus pkt-to-pkt delay\n",
	  testCores,testSessions,p2p_delay);

  //-----------------------------------------
  // Launching TCP server threads
  for (uint8_t c = 0; c < testCores ;c ++) {
    std::thread server = std::thread(tcpServer,pEkaDev,
				     conn[c].dstIp.c_str(),
				     conn[c].dstTcpPort);
    server.detach();
  }

  while (g_keepWork && ! g_serverSet)
    std::this_thread::yield();

  std::thread thr[testCores][testSessions];

  //------------------------------------------------
  // Initializing EkaDev
  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, &ekaDevInitCtx);

  //------------------------------------------------
  // Initializing FETH Ports (Cores)
  EkaCoreInitCtx ekaCoreInitCtx = {};
  for (uint8_t c = 0; c < testCores ;c ++) {
    ekaCoreInitCtx.coreId = c;
    inet_aton(conn[c].srcIp.c_str(),(struct in_addr*)&ekaCoreInitCtx.attrs.host_ip);
    ekaDevConfigurePort (pEkaDev, &ekaCoreInitCtx);
  }

  //------------------------------------------------
  // Creating Sockets

  int sock[MaxTestCores][MaxTestSessions] = {};
  static ExcConnHandle hCon[MaxTestCores][MaxTestSessions] = {};
  std::thread excSendThr[MaxTestCores][MaxTestSessions] = {};
  std::thread excRecvThr[MaxTestCores][MaxTestSessions] = {};

  for (uint8_t c = 0; c < testCores ;c ++) {
    for (uint8_t s = 0; s < testSessions ;s ++) {
      sock[c][s] =  excSocket(pEkaDev,c,0,0,0);
      if (sock[c][s] < 0) on_error("failed to open sock[%u][%u]",c,s);

      struct sockaddr_in dst = {};
      dst.sin_addr.s_addr = inet_addr(conn[c].dstIp.c_str());
      dst.sin_family = AF_INET;
      dst.sin_port = htons(conn[c].dstTcpPort);

      TEST_LOG("Trying to connect connect sock[%u][%u] on %s:%u",
	       c,s,inet_ntoa(dst.sin_addr),conn[c].dstTcpPort);
      fflush(stderr);
      if ((hCon[c][s] = excConnect(pEkaDev,sock[c][s],(sockaddr*) &dst,
				      sizeof(sockaddr_in))) < 0) 
	on_error("failed to connect sock[%u][%u] on port %u",
		 c,s,conn[c].dstTcpPort);

      int rc = excSetBlocking(pEkaDev,sock[c][s],false);
      if (rc)
      	on_error("excSetBlocking() returned %d",rc);
    }
  }

  //------------------------------------------------
  // Launching TCP Client threads

  for (uint8_t c = 0; g_keepWork && c < testCores; c ++) {
    for (uint8_t s = 0; g_keepWork && s < testSessions ;s ++) {
      uint thrId = c * testSessions + s;
      g_sessRcvThreadReady[thrId] = false;
      excRecvThr[c][s] = std::thread(excRecvThr_f,
				     pEkaDev,hCon[c][s],
				     thrId);
      while (g_keepWork && ! g_sessRcvThreadReady[thrId])
	std::this_thread::yield();

      excSendThr[c][s] = std::thread(excSendThr_f,
				     pEkaDev,hCon[c][s],
				     thrId,p2p_delay);


    }
  }
  //------------------------------------------------

  while (g_keepWork)
    std::this_thread::yield();
  
  printf ("Closing the sockets\n");

  for (uint c=0; c<testCores; c++) {
    for (uint8_t s=0; s<testSessions ;s++) {
      excSendThr[c][s].join();
      excRecvThr[c][s].join();
      excClose(pEkaDev,hCon[c][s]);
    }
  }

  printf("Closing device\n");
  ekaDevClose(pEkaDev);
  printf("Device is closed\n"); fflush(stdout);

  return 0;
}

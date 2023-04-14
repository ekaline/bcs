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

#define TCP_TEST_ECHO 0
#define TCP_TEST_DATA 1

#define BUF_SIZE 8192
//#define BUF_SIZE (1536 - 14 - 20 - 20)
//#define BUF_SIZE 1460

#define CORES 2
#define SESSIONS 32

#define STATISTICS_PERIOD 10000

volatile bool keep_work = true;;
volatile bool serverSet = false;


#define EXC_TEST_PKT_PREFIX_SIZE 6
#define EXC_TEST_PKT_FREE_TEXT_SIZE 64

struct TcpTestPkt {
  char     prefix[EXC_TEST_PKT_PREFIX_SIZE];
  uint8_t  core;      // 1
  uint8_t  sess;      // 1
  uint64_t cnt;   // 8
  char     free_text[EXC_TEST_PKT_FREE_TEXT_SIZE];
};


int createThread(const char* name, EkaServiceType type,
		 void *(*threadRoutine)(void*), void* arg,
		 void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}

int credAcquire(EkaCredentialType credType, EkaGroup group,
		const char *user,const size_t userLength,
		const struct timespec *leaseTime,
		const struct timespec *timeout,
		void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",
	  user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}

void fastpath_thread_f(EkaDev* pEkaDev, ExcConnHandle sess_id,
		       uint thrId, uint p2p_delay) {
  uint8_t coreId = excGetCoreId(sess_id);
  uint8_t sessId = excGetSessionId(sess_id);
  TEST_LOG("Launching TcpClient for coreId %u, sessId %u",coreId,sessId);

  char tx_buf[BUF_SIZE] = {};
  const char* test_pkt_prefix = "EXCPKT";

  TcpTestPkt* pkt = (TcpTestPkt*) tx_buf;
  memcpy(&pkt->prefix,test_pkt_prefix,EXC_TEST_PKT_PREFIX_SIZE);
  pkt->core = coreId;
  pkt->sess = sessId;
  pkt->cnt = 0;

  while (keep_work) {
#if TCP_TEST_DATA
    sprintf(pkt->free_text,"%u_%u_%2u_%08ju",thrId,coreId,sessId,pkt->cnt);


    //    size_t pkt_size = 1461; // Ken's magic number
    //size_t pkt_size = 1000; // Ken's magic number

    size_t pkt_size = rand() % (BUF_SIZE - sizeof(TcpTestPkt) - 54) + sizeof(TcpTestPkt);
    //    uint pkt_size = rand() % (BUF_SIZE - 2 - sizeof(TcpTestPkt)) + 3;
    //uint pkt_size = rand() % 2; tx_buf[0] = 0xa1; tx_buf[1] = 0xb2;
    //    if (pkt_size == 0) pkt_size = 2;
    //    uint pkt_size = 128;
    //uint pkt_size = 1; tx_buf[0] = 0xab;

    for (auto i = sizeof(TcpTestPkt); i < pkt_size; i++) {
      //  tx_buf[i] = (char)(sessId + 1);
          tx_buf[i] = 'a' + (rand() % ('z' - 'a'));
    }

    /* -------------------------------------------------- */
    //    TEST_LOG("%u %04ju: sending %u bytes",sessId,pkt->cnt,pkt_size); fflush(stderr);
    int sentBytes = 0;
    while (keep_work && (sentBytes < (int)pkt_size || pkt_size == 0)) {
      int sent = excSend (pEkaDev, sess_id, pkt, pkt_size, 0);
      //      TEST_LOG("%u %04ju: sent %u out of %u bytes",sessId,pkt->cnt,sent,pkt_size); fflush(stderr);
      if (pkt_size != 0 && sent == 0) usleep(10);
      sentBytes += sent;
    }
    /* -------------------------------------------------- */
    //   TEST_LOG("RX or %u bytes:",pkt_size);
#if TCP_TEST_ECHO
    char rx_buf[BUF_SIZE] = {};
    size_t rxsize = 0;
    do {
      size_t rc = excRecv(pEkaDev,sess_id, &rx_buf[rxsize], pkt_size, 0);
      if (rc < 0) {
	TEST_LOG("WARNING: rc = %jd",rc);
	continue;
      }
      rxsize += rc;
    } while (keep_work && rxsize != pkt_size);

    if (! keep_work) return;
    //    hexDump("RCV",rx_buf,rxsize);

    if (memcmp(tx_buf,rx_buf,pkt_size) != 0) { 
      hexDump("TX_BUF",tx_buf,pkt_size);
      hexDump("RX_BUF",rx_buf,pkt_size);
      on_error("%u pkt %04ju: RX != TX pkt_size=%jd (=0x%jx) for coreId %u sessId %u",
	       sessId,pkt->cnt,pkt_size,pkt_size,coreId,sessId);
      TEST_LOG("ERROR: %u %04ju: payload is INCORRECT",sessId,pkt->cnt); fflush(stderr);

    } 
#endif // TCP_TEST_ECHO
    if (pkt->cnt % STATISTICS_PERIOD == 0) {
      TEST_LOG ("CoreId %u, SessId %u, pkt->cnt: %ju",coreId,sessId,pkt->cnt);
    }
    if (p2p_delay != 0) usleep (p2p_delay);
    pkt->cnt++;

    //    if (pkt->cnt > 20000) keep_work = false;
#endif // TCP_TEST_DATA
  }
  TEST_LOG("fastpath_thread_f is terminated"); fflush(stderr);fflush(stdout);

  return;
}
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
  int bytes_read = -1;
  //  printf ("Running TCP Server for sock=%d, port = %u\n",sock,port);
  do {
    char line[1536] = {};
    bytes_read = recv(sock, line, sizeof(line), 0);
    if (bytes_read > 0) {
      /* TEST_LOG ("recived pkt: %s",line); */
      /* fflush(stderr); */

#if TCP_TEST_ECHO
      send(sock, line, bytes_read, 0);
#endif
    }
  } while (keep_work);
  TEST_LOG("tcpChild thread is terminated");
  fflush(stdout);
  close(sock);
  //  keep_work = false;
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
  serverSet = true;
  while (keep_work) {
    int childSock, addr_size = sizeof(addr);

    childSock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
    TEST_LOG("Connected from: %s:%d -- sock=%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),childSock);
    std::thread child(tcpChild,dev,childSock,be16toh(addr.sin_port));
    child.detach();
  }
  TEST_LOG(" -- closing");
}

/* --------------------------------------------- */


int main(int argc, char *argv[]) {
  keep_work = true;
  EkaDev* pEkaDev = NULL;

  signal(SIGINT, INThandler);

  const uint     MaxTestCores    = 2;
  const uint     MaxTestSessions = 16;
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

  getAttr(argc,argv,&conn[0].dstIp,&conn[0].dstTcpPort,&conn[0].srcIp,&testSessions,&testCores,&p2p_delay);
  //-----------------------------------------
  printf ("Running with %u cores, %u sessions, %uus pkt-to-pkt delay\n",testCores,testSessions,p2p_delay);

  //-----------------------------------------
  // Launching TCP server threads
  for (uint8_t c = 0; c < testCores ;c ++) {
    std::thread server = std::thread(tcpServer,pEkaDev,conn[c].dstIp.c_str(),conn[c].dstTcpPort);
    server.detach();
  }

  while (keep_work && ! serverSet) { sleep (0); }
  std::thread thr[testCores][testSessions];

  //------------------------------------------------
  // Initializing EkaDev
  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  //  EkaDev* dev = pEkaDev;

  //------------------------------------------------
  // Initializing FETH Ports (Cores)
  EkaCoreInitCtx ekaCoreInitCtx = {};
  for (uint8_t c = 0; c < testCores ;c ++) {
    ekaCoreInitCtx.coreId = c;
    inet_aton(conn[c].srcIp.c_str(),(struct in_addr*)&ekaCoreInitCtx.attrs.host_ip);
    ekaDevConfigurePort (pEkaDev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);
  }

  //------------------------------------------------
  // Creating Sockets

  int sock[MaxTestCores][MaxTestSessions] = {};
  static ExcConnHandle sess_id[MaxTestCores][MaxTestSessions] = {};
  std::thread fast_path_thread[MaxTestCores][MaxTestSessions] = {};

  for (uint8_t c = 0; c < testCores ;c ++) {
    for (uint8_t s = 0; s < testSessions ;s ++) {
      sock[c][s] =  excSocket(pEkaDev,c,0,0,0);
      if (sock[c][s] < 0) on_error("failed to open sock[%u][%u]",c,s);

      struct sockaddr_in dst = {};
      dst.sin_addr.s_addr = inet_addr(conn[c].dstIp.c_str());
      dst.sin_family = AF_INET;
      dst.sin_port = htons(conn[c].dstTcpPort);

      TEST_LOG("Trying to connect connect sock[%u][%u] on %s:%u",c,s,inet_ntoa(dst.sin_addr),conn[c].dstTcpPort);
      fflush(stderr);
      if ((sess_id[c][s] = excConnect(pEkaDev,sock[c][s],(sockaddr*) &dst, sizeof(sockaddr_in))) < 0) 
	on_error("failed to connect sock[%u][%u] on port %u",c,s,conn[c].dstTcpPort);

      /* int rc = excSetBlocking(pEkaDev,sess_id[c][s],false); */
      /* if (rc) */
      /* 	on_error("excSetBlocking() returned %d",rc); */
    }
  }

  //------------------------------------------------
  // Launching TCP Client threads

  for (uint8_t c = 0; c < testCores; c ++) {
    for (uint8_t s = 0; s < testSessions ;s ++) {
      uint thrId = c * testSessions + s;
      fast_path_thread[c][s] = std::thread(fastpath_thread_f,pEkaDev,sess_id[c][s],thrId,p2p_delay);
      fast_path_thread[c][s].detach();
    }
  }
  //------------------------------------------------

  while (keep_work) usleep(0);
  printf ("Closing the sockets\n");

  for (uint c=0; c<testCores; c++) {
    for (uint8_t s=0; s<testSessions ;s++) {
      excClose(pEkaDev,sess_id[c][s]);
    }
  }

  sleep (1);
  printf("Closing device\n");
  ekaDevClose(pEkaDev);
  printf("Device is closed\n"); fflush(stdout);
  //  sleep(5);
  return 0;
}

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

#define STATISTICS_PERIOD 50000

volatile bool keep_work = true;;
volatile bool serverSet = false;

void fastpath_thread_f(EkaDev* pEkaDev,
		       ExcConnHandle sess_id,uint thrId,
		       uint p2p_delay) {
  uint8_t coreId = excGetCoreId(sess_id);
  uint8_t sessId = excGetSessionId(sess_id);
  TEST_LOG("Launching TcpClient for coreId %u, sessId %u, p2p_delay=%u",
	   coreId,sessId,p2p_delay);
 
  static const size_t PktSize = 708; // = CME SW Quote
  size_t cnt = 0;
  while (keep_work) {
    ++cnt;
    char pkt[PktSize] = {};    
    //    sprintf(pkt,"%u_%u_%2u_%08ju",thrId,coreId,sessId,cnt);
    /* -------------------------------------------------- */

    size_t sentBytes = 0;
    size_t byte2send = PktSize;
    const char* p = pkt;
    while (keep_work && sentBytes < PktSize) {
      int rc = excSend (pEkaDev, sess_id, p, byte2send, 0);
      /* ------------------- */
      switch (rc) {
      case -1 :
	TEST_LOG("%u_%u_%2u: Session dropped \'%s\'",
		 thrId,coreId,sessId,strerror(errno));
	keep_work = false;
	break;
      case 0 :
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	  /* TEST_LOG("Retrying pkt %ju",cnt); */
	  /* send same again */
	  std::this_thread::yield();

	} else {
	  keep_work = false;
	  EKA_ERROR("Unexpected errno=%d (\'%s\')",
		    errno,strerror(errno));
	}
	break;	
      default:
	sentBytes += rc;
	p += rc;
	byte2send = PktSize - sentBytes;
      }
      /* ------------------- */

    }

    if (cnt % STATISTICS_PERIOD == 0) {
      TEST_LOG ("CoreId %u, SessId %u, pkt->cnt: %ju",
		coreId,sessId,cnt);
    }
    if (p2p_delay != 0) usleep (p2p_delay);
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
  //  printf ("Running TCP Server for sock=%d, port = %u\n",sock,port);
  do {
    char line[1536] = {};
    recv(sock, line, sizeof(line), 0);
    //    usleep(10);
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
  ekaDevInit(&pEkaDev, &ekaDevInitCtx);

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

      int rc = excSetBlocking(pEkaDev,sock[c][s],false);
      if (rc)
      	on_error("excSetBlocking() returned %d",rc);
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

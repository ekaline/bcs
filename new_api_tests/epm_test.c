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

#include <sys/time.h>
#include <chrono>

#include "eka_dev.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Efh.h"
#include "Epm.h"

#include "eka_macros.h"

/* --------------------------------------------- */

volatile bool keep_work = true;
volatile bool serverSet = false;;
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
void tcpChild(int sock, uint port) {
  int bytes_read = -1;
  //  printf ("Running TCP Server for sock=%d, port = %u\n",sock,port);
  do {
    char line[1536] = {};
    bytes_read = recv(sock, line, sizeof(line), 0);
    printf ("%u: %s\n",sock,line);
    fflush(stdout);
    send(sock, line, bytes_read, 0);
  } while (bytes_read > 0 && keep_work);
  printf("%u: bytes_read = %d -- closing\n",sock,bytes_read);
  fflush(stdout);
  close(sock);
  keep_work = false;
  return;
}
/* --------------------------------------------- */
void tcpServer(std::string ip, uint16_t port) {
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
    printf("Connected from: %s:%d -- sock=%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),childSock);
    std::thread child(tcpChild,childSock,be16toh(addr.sin_port));
    child.detach();
  }
}

/* --------------------------------------------- */

void printUsage(char* cmd) {
  printf("USAGE: %s -s <Server IP> -c <Client IP> -p <TCP Port> \n",cmd); 
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[], std::string* serverIp, std::string* clientIp, uint16_t* port) {
  int opt; 
  while((opt = getopt(argc, argv, ":c:s:p:h")) != -1) {  
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
	*port = atoi(optarg);
	printf("port = %u\n", *port);  
	break;  
      case 'h':  
	printUsage(argv[0]);
	break;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  

  if ((*clientIp).empty() || (*serverIp).empty() || *port == 0) {
    printUsage(argv[0]);
    on_error("missing params: clientIp=%s, serverIp=%s, port=%u",
	     (*clientIp).c_str(),(*serverIp).c_str(),*port);
  }
  return 0;
}
/* --------------------------------------------- */


/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  std::string clientIp, serverIp;
  uint16_t port = 0;

  getAttr(argc,argv,&serverIp,&clientIp,&port);

  keep_work = true;
  serverSet = false;

  std::thread server = std::thread(tcpServer,serverIp,port);
  server.detach();

  while (keep_work && ! serverSet) { sleep (0); }

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

  struct sockaddr_in dst = {};
  dst.sin_addr.s_addr = inet_addr(serverIp.c_str());
  dst.sin_family      = AF_INET;
  dst.sin_port        = be16toh(port);

  int sock = excSocket(dev,coreId,0,0,0);
  if (sock < 0) on_error("failed to open sock");

  EKA_LOG ("Trying to connect to %s:%u",EKA_IP2STR(dst.sin_addr.s_addr),be16toh(dst.sin_port));
  ExcConnHandle conn = excConnect(dev,sock,(struct sockaddr*) &dst, sizeof(struct sockaddr_in));
  if (conn < 0) on_error("excConnect %s:%u",EKA_IP2STR(dst.sin_addr.s_addr),be16toh(dst.sin_port));

  const char* pkt = "Hello world!";
  char rx_buf[1000] = {};

  excSend (dev, conn, pkt, strlen(pkt));


  int rxsize = 0;
  do {
    rxsize = excRecv(dev,conn, rx_buf, 1000);
  } while (keep_work && rxsize < 1);
  printf ("RX: %s\n",rx_buf);

  /* ============================================== */

  const char* pkt2send_1 = "Pkt 1 FIRED";


  EpmAction epmAction_1 = {
    .token         = static_cast<epm_token_t>(0), ///< Security token
    .hConn         = conn,                        ///< TCP connection where segments will be sent
    .offset        = 0,                           ///< Offset to payload in payload heap
    .length        = strlen(pkt2send_1),                 ///< Payload length
    .actionFlags   = AF_Valid,                    ///< Behavior flags (see EpmActionFlag)
    .nextAction    = EPM_LAST_ACTION,             ///< Next action in sequence, or EPM_LAST_ACTION
    .enable        = 0x01,                        ///< Enable bits
    .postLocalMask = 0x01,                        ///< Post fire: enable & mask -> enable
    .postStratMask = 0x01,                        ///< Post fire: strat-enable & mask -> strat-enable
    .user          = NULL                         ///< Opaque value copied into `EpmFireReport`.
  };

  EpmStrategyParams strat_1 = {
    .numActions  = 1,             ///< No. of actions entries used by this strategy
    .triggerAddr = NULL,          ///< Address to receive trigger packets
    .reportCb    = fireReportCb,  ///< Callback function to process fire reports
    .cbCtx       = NULL           ///< Opaque value passed into reportCb
  };









  /* ============================================== */


  excClose(dev,conn);

  sleep (1);
  printf("Closing device\n");
  ekaDevClose(dev);
  sleep (1);

  return 0;
}

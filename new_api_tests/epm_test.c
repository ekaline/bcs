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

#include "ekaline.h"
#include "Exc.h"
#include "Eka.h"
#include "Efc.h"

//#include "exc_debug_module.h"

//to avt-6
#define DST_AVT6_IP0 "192.168.0.93"
#define DST_AVT6_IP1 "192.168.0.93"

//to xn03
#define DST_XN03_IP0 "10.0.0.30"
#define DST_XN03_IP1 "10.0.0.31"

//to xn01
#define DST_XN01_IP0 "10.0.0.10"
#define DST_XN01_IP1 "10.0.0.11"

//to xn02
#define DST_XN02_IP0 "200.0.0.20"
#define DST_XN02_IP1 "200.0.0.21"

#define DST_PORT_BASE 22222

#define MAX_PAYLOAD_SIZE 192
#define MIN_PAYLOAD_SIZE 1

#define CHAR_SIZE  (26+1)

#define CORES 2
#define SESSIONS 32

volatile bool keep_work = true;
volatile bool serverSet = false;;

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

#include "eka_default_callbacks4tests.incl"
#include "eka_exc_debug_module.h"
#include "eka_macros.h"

int createThread(const char* name, EkaThreadType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}

int credAcquire(EkaCredentialType credType, EkaSource source, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s\n",user,EKA_EXCH_DECODE(source));
  return 0;
}

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}

void fastpath_thread_f(EkaDev* dev, ExcConnHandle sess_id,uint p2p_delay) {
  //  char tcpsenddata[CHAR_SIZE] = "abcdefghijklmnopqrstuvwxyz";
  EKA_LOG("Launching for sess_id %u",sess_id);

  uint statistics_period = 10000;

  uint8_t core = excGetCoreId(sess_id);
  uint8_t sess = excGetSessionId(sess_id);

  const char* test_pkt_prefix = "EXCPKT";

  eka_tcp_test_pkt pkt = {};
  memcpy(&pkt.prefix,test_pkt_prefix,EXC_TEST_PKT_PREFIX_SIZE);
  pkt.core = core;
  pkt.sess = sess;
  while (keep_work) {

    char rx_buf[MAX_PAYLOAD_SIZE];
    int rxsize = 0;
    sprintf(pkt.free_text,"%u_%2u_%07ju",core,sess,pkt.cnt);

    excSend (dev, sess_id, &pkt, sizeof(pkt));

    do {
      rxsize = excRecv(dev,sess_id, rx_buf, 1000);
    } while (keep_work && rxsize < 1);
    if (! keep_work) return;
    //    hexDump("RCV",rx_buf,rxsize);

    if ((int)sizeof(pkt) != rxsize)  on_error("%u@%u : sizeof(pkt) (= %d) != rxsize (= %d)",sess,core,(int)sizeof(pkt),rxsize);
    if (memcmp(&pkt,rx_buf,sizeof(pkt)) != 0) { 
      hexDump("RX BUF",rx_buf,sizeof(pkt));
      hexDump("TX BUF",(void*)&pkt,sizeof(pkt));

      on_error("RX != TX for core %u sess %u",core,sess); 
    }

    if (pkt.cnt % statistics_period == 0) {
      EKA_LOG ("Sess %u, pkt.cnt: %ju",sess_id,pkt.cnt);
    }

    usleep (p2p_delay);
    pkt.cnt++;
    
    //    if (pkt.cnt == 2) {keep_work = false; return;}
  }
  return;
}

/* --------------------------------------------- */
void Child(int sock, uint port) {
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
    std::thread child(Child,childSock,be16toh(addr.sin_port));
    child.detach();
  }
}

/* --------------------------------------------- */

void printUsage(char* cmd) {
  printf("USAGE: %s -s <Server IP> -c <Client IP> -p <TCP Port> \n",cmd); 
}

/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  std::string clientIp, serverIp;
  uint16_t port = 0;
  int opt; 
  while((opt = getopt(argc, argv, ":c:s:p:h")) != -1) {  
    switch(opt) {  
      case 's':  
	serverIp = std::string(optarg);
	printf("serverIp = %s\n", serverIp.c_str());  
	break;  
      case 'c':  
	clientIp = std::string(optarg);
	printf("clientIp = %s\n", clientIp.c_str());  
	break;  
      case 'p':  
	port = atoi(optarg);
	printf("port = %u\n", port);  
	break;  
      case 'h':  
	printUsage(argv[0]);
	break;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  

  if (clientIp.empty() || serverIp.empty() || port == 0) {
    printUsage(argv[0]);
    on_error("missing params: clientIp=%s, serverIp=%s, port=%u",
	     clientIp.c_str(),serverIp.c_str(),port);
  }


  keep_work = true;

  std::thread server = std::thread(tcpServer,serverIp,port);
  server.detach();

  while (keep_work && ! serverSet) { sleep (0); }

  EkaDev* dev = NULL;
  EkaCoreId coreId = 0;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&dev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  EkaCoreInitCtx ekaCoreInitCtx = {
    .coreId = coreId,
    .attrs = {
      .host_ip = inet_addr(clientIp.c_str()),
      .netmask = inet_addr("255.255.255.0"),
      .gateway = inet_addr(clientIp.c_str()),
      .nexthop_mac  = {0x00,0x0F,0x53,0x08,0xED,0xD5},
      .src_mac_addr = {0x00,0x21,0xB2,0x19,0x0F,0x30},
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

  printf ("Trying to connect to %s:%u",EKA_IP2STR(dst.sin_addr.s_addr),be16toh(dst.sin_port));
  ExcConnHandle conn = excConnect(dev,sock,(struct sockaddr*) &dst, sizeof(struct sockaddr_in));
  if (conn < 0) on_error("failed to connect to %s:%u",EKA_IP2STR(dst.sin_addr.s_addr),be16toh(dst.sin_port));
  printf("Connected\n");

  const char* pkt = "Hello world!";
  char rx_buf[1000] = {};

  excSend (dev, conn, pkt, strlen(pkt));
  int rxsize = 0;
  do {
    rxsize = excRecv(dev,conn, rx_buf, 1000);
  } while (keep_work && rxsize < 1);
  printf ("RX: %s\n",rx_buf);

  excClose(dev,conn);

  sleep (1);
  printf("Closing device\n");
  ekaDevClose(dev);
  sleep (1);

  return 0;
}

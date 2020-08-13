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

/* --------------------------------------------- */
void tcpChild(EkaDev* dev, int sock, uint port) {
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

  bool gtsRun = false;
  char hostname[1024] = {};
  gethostname(hostname, 1024);
  if (strncmp(hostname, "nqxlxavt059d",6) == 0) gtsRun = true;

  EkaDev* dev = NULL;
  EkaCoreId coreId = 0;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.createThread = createThread;

  ekaDevInit(&dev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  std::string serverIp  = gtsRun ? "10.120.115.53" : "10.0.0.10";
  std::string clientIp  = gtsRun ? "10.120.115.56" : "10.0.0.110";
  uint16_t serverTcpPort  = 22222;

  TEST_LOG("hostname=%s, gtsRun=%d, tcpServer=%s:%u",
	   hostname,gtsRun,serverIp.c_str(),serverTcpPort);

  std::thread server = std::thread(tcpServer,dev,serverIp,serverTcpPort);
  server.detach();
  
  while (keep_work && ! serverSet) { sleep (0); }

  EkaCoreInitCtx ekaCoreInitCtx = {
    .coreId = coreId,
    .attrs = {}
  };

  char gtsServerMac[6] = {0x00,0x0F,0x53,0x42,0x5a,0x40};
  if (gtsRun) memcpy (&ekaCoreInitCtx.attrs.nexthop_mac,&gtsServerMac,6);

  ekaDevConfigurePort (dev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);

  struct sockaddr_in serverAddr = {};
  serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
  serverAddr.sin_family      = AF_INET;
  serverAddr.sin_port        = be16toh(serverTcpPort);

  int sock = excSocket(dev,coreId,0,0,0);
  if (sock < 0) on_error("failed to open sock");

  EKA_LOG ("Trying to connect to %s:%u on sock = %d",
	   EKA_IP2STR(serverAddr.sin_addr.s_addr),
	   be16toh(serverAddr.sin_port),
	   sock);
  ExcConnHandle conn = excConnect(dev,sock,(struct sockaddr*) &serverAddr, sizeof(struct sockaddr_in));
  if (conn < 0) on_error("excConnect %s:%u",EKA_IP2STR(serverAddr.sin_addr.s_addr),be16toh(serverAddr.sin_port));

  /* ============================================== */
  const char* pkt = "\n\n TCP session is ESTABLISHED!!! \n\n";
  excSend (dev, conn, pkt, strlen(pkt));

  while (keep_work) {
    char rxBuf[1000] = {};
    int rxsize = -1;
    if (excReadyToRecv(dev,conn))
      rxsize = excRecv(dev,conn, rxBuf, sizeof(rxBuf));
    else
      EKA_LOG("No RX data ready");
    //   if (rxsize < 1) on_error("rxsize < 1");
    if (rxsize > 0) 
      EKA_LOG("\n%s\n",rxBuf);
    else 
      EKA_LOG("rxsize = %d",rxsize);

  }

  while (keep_work) sleep(0);
  keep_work = false;
  sleep(3);

  /* ============================================== */


  excClose(dev,conn);
  printf("Closing device\n");

  sleep (1);
  ekaDevClose(dev);

  return 0;
}

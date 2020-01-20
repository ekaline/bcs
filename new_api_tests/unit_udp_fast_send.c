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
#define MAX_THREADS 32

#define MC_DST_PORT 50000
#define MC_DST_GROUP "239.0.0.1"
//#define MC_DST_GROUP "233.54.12.148"

#define MY_IP "10.0.0.30"

volatile bool keep_work;
void hexDump (const char* desc, void *addr, int len);

#include "eka_default_callbacks4tests.incl"
#include "eka_exc_debug_module.h"
#include "eka_macros.h"

int excSendFastUdp (EkaDev* dev, uint8_t core, uint8_t sess, const void *dataptr, size_t size, const struct sockaddr *to);

struct message_t {
  char     free_text[8];
  uint64_t seq;
  uint     thread_id;
  char     pad[24];
};


void send_mc_thread(EkaDev* dev, uint thread_id, uint pktPerThread) {
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(MC_DST_GROUP);
  addr.sin_port = htons(MC_DST_PORT + thread_id);


  message_t message = {};
  message.seq = 0;
  message.thread_id = thread_id;
  sprintf(message.free_text,"SND_PKT");

  printf("%s: Thread %u: sending to %s:%u\n",__func__,thread_id,inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));

  int sk = excUdpSocket(dev);
  if (sk < 0) on_error("socket");

  for (uint i = 0; i < pktPerThread; i++) {
    message.seq ++;
    excSendFastUdp (dev, 0, thread_id, (const void*) &message, (size_t) sizeof(message), (const struct sockaddr *) &addr);
    if (! keep_work) break;
    /* if(excSendTo(dev,sk,(const void*) &message, (size_t) sizeof(message),(const struct sockaddr *) &addr) < 0)  */
    /*   on_error("sendto failed for iteration %ju at thread %u",message.seq,thread_id); */
    //    usleep(0;
    //    for (uint j = 0; j < 100; j++) {}
  }

  return;
}


int main(int argc, char *argv[]) {
  keep_work = true;
  EkaDev* pEkaDev = NULL;
  //-----------------------------------------
  //  uint8_t testCores = 1;  
  uint numThreads = argc > 1 ? atoi(argv[1]) : 1;
  if (numThreads > MAX_THREADS) on_error("numThreads (%u) > MAX_THREADS (%u)",numThreads,MAX_THREADS);

  uint pktPerThread = argc > 2 ? atoi(argv[2]) : 100000;


/*   uint8_t testSessions = argc > 1 ? atoi(argv[1]) : 1; */
/*   uint    p2p_delay    = argc > 2 ? atoi(argv[2]) : 10; */
/* //----------------------------------------- */

/*   printf ("Running test with %u sessions (=threads), %uus pkt-to-pkt delay\n",testSessions,p2p_delay); */

  signal(SIGINT, INThandler);

  char hostname[1024];
  gethostname(hostname, 1024);

  char dst_ip[CORES][1024];

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  EkaCoreInitCtx ekaCoreInitCtx = {};

  if (strcmp(hostname, "xn03") == 0) {
    strcpy(dst_ip[0],DST_XN01_IP0);
    strcpy(dst_ip[1],DST_XN01_IP1);
  } else if (strcmp(hostname, "xn01") == 0) {
    strcpy(dst_ip[0],DST_XN03_IP0);
    strcpy(dst_ip[1],DST_XN03_IP1);
    inet_aton("10.0.0.117",(struct in_addr*)&ekaCoreInitCtx.attrs.host_ip);

    uint8_t src_mac[6] = {0x00,0x21,0xb2,0x19,0x17,0x90};
    memcpy(&ekaCoreInitCtx.attrs.src_mac_addr,src_mac,6);

  } else if (strcmp(hostname, "nylxlabavt-7") == 0) {
    strcpy(dst_ip[0],DST_AVT6_IP0);
    strcpy(dst_ip[1],DST_AVT6_IP1);
    inet_aton("192.168.0.89",(struct in_addr*)&ekaCoreInitCtx.attrs.host_ip);
    uint8_t next_hop[6] = {0x00,0x0F,0x53,0x2D,0x82,0xA0};
    memcpy(&ekaCoreInitCtx.attrs.nexthop_mac,next_hop,6);

    uint8_t src_mac[6] = {0x00,0x21,0xb2,0x19,0x17,0x90};
    memcpy(&ekaCoreInitCtx.attrs.src_mac_addr,src_mac,6);
  } else on_error("Unsupported host %s\n", hostname);

  EkaDev* dev = pEkaDev;

  // to initialize LWIP
  int dummy_socket =  excSocket(pEkaDev,0,0,0,0);
  printf ("stam dummy_socket = %d\n",dummy_socket);
  std::thread t[MAX_THREADS];

  for (uint i = 0; i < numThreads; i ++) {
    printf ("Spin OFF %u\n",i);
    t[i] = std::thread(send_mc_thread,dev,i,pktPerThread);
  }

  for (uint i = 0; i < numThreads; i ++) {
    t[i].join();
  }

  printf ("%s: sent in total %u packets from %u threads\n",argv[0],numThreads * pktPerThread,numThreads);


  sleep (1);
  printf("Closing device\n");
  ekaDevClose(pEkaDev);
  sleep (1);

  return 0;
}

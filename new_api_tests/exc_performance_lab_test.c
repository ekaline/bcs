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

volatile bool keep_work;

#include "eka_default_callbacks4tests.incl"

#define ITERATIONS 100000

void fastpath_thread_f(EkaDev* pEkaDev, ExcConnHandle sess_id,uint p2p_delay) {
  uint64_t iteration = 0;
  //  char tcpsenddata[CHAR_SIZE] = "abcdefghijklmnopqrstuvwxyz";
  EkaDev* dev = pEkaDev;

  EKA_LOG("Launching for sess_id %u",sess_id);

  while (keep_work) {
    char tx_buf[MAX_PAYLOAD_SIZE] = {};
    sprintf (tx_buf,"XYU_%u_%08ju",sess_id,iteration);
    int send_size = strlen(tx_buf);

    char rx_buf[MAX_PAYLOAD_SIZE];
    int rxsize = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (uint i = 0 ; i < ITERATIONS && keep_work; i++) {
      excSend (pEkaDev, sess_id, tx_buf, send_size);
      do {
	rxsize = excRecv(pEkaDev,sess_id, rx_buf, 1000);
      } while (keep_work && rxsize < 1);
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto rtt = std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
    EKA_LOG ("%u, %ju: %u transactions, %juns, %ju trans per second",sess_id,iteration,ITERATIONS,rtt, (uint)(ITERATIONS * 1e9 / rtt) );


    if (! keep_work) return;
    /* if (send_size != rxsize)  on_error("%u@%u : send_size (= %d) != rxsize (= %d)",excGetSessionId(sess_id),excGetCoreId(sess_id),send_size,rxsize); */
    /* if (memcmp(tx_buf,rx_buf,MAX_PAYLOAD_SIZE) != 0) on_error("%u@%u : tx_buf (= %s) != rx_buf (= %s)",excGetSessionId(sess_id),excGetCoreId(sess_id),tx_buf,rx_buf); */
    /* //    EKA_LOG("%u@%u : iter %08ju: tx_buf (= %s) == rx_buf (= %s)",excGetSessionId(sess_id),excGetCoreId(sess_id),iteration,tx_buf,rx_buf); */
    /* if (iteration % ITERATIONS == 0) { */
    /*   EKA_LOG ("Sess %u, iteration: %ju",sess_id,iteration); */
    /* } */

    //    usleep (p2p_delay);
    iteration++;
  }
  return;
}


int main(int argc, char *argv[]) {
  keep_work = true;
  EkaDev* pEkaDev = NULL;
  //-----------------------------------------
  uint8_t testCores = 1;  
  uint8_t testSessions = argc > 1 ? atoi(argv[1]) : 1;
  uint    p2p_delay    = argc > 2 ? atoi(argv[2]) : 10;
//-----------------------------------------

  printf ("Running test with %u sessions (=threads), %uus pkt-to-pkt delay\n",testSessions,p2p_delay);

  signal(SIGINT, INThandler);

  char hostname[1024];
  gethostname(hostname, 1024);

  char dst_ip[CORES][1024];

  if (strcmp(hostname, "xn03") == 0) {
    strcpy(dst_ip[0],DST_XN01_IP0);
    strcpy(dst_ip[1],DST_XN01_IP1);
  } 
  else if (strcmp(hostname, "xn01") == 0) {
    strcpy(dst_ip[0],DST_XN03_IP0);
    strcpy(dst_ip[1],DST_XN03_IP1);
  }
  else on_error("Unsupported host %s\n", hostname);

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);
  EkaCoreInitCtx ekaCoreInitCtx = {};
  EkaDev* dev = pEkaDev;

  std::thread fast_path_thread[CORES][SESSIONS];
  static ExcConnHandle sess_id[CORES][SESSIONS];
  int sock[CORES][SESSIONS];

  for (uint8_t c = 0; c < testCores; c ++) {
    ekaCoreInitCtx.coreId = c;
    ekaDevConfigurePort (pEkaDev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);
    struct sockaddr_in dst = {};
    dst.sin_addr.s_addr = inet_addr(dst_ip[c]);
    dst.sin_family = AF_INET;
    dst.sin_port = htons((uint16_t)DST_PORT_BASE);

    for (uint8_t s = 0; s < testSessions ;s ++) {
      if ((sock[c][s] = excSocket(pEkaDev,c,0,0,0)) < 0) on_error("failed to open sock[%u][%u]",c,s);
      EKA_LOG("sock[%u][%u] = %d",c,s,sock[c][s]);

      EKA_LOG("Trying to connect connect sock[%u][%u] on %s:%u",c,s,inet_ntoa(dst.sin_addr),DST_PORT_BASE);
      if ((sess_id[c][s] = excConnect(pEkaDev,sock[c][s],(struct sockaddr*) &dst, sizeof(struct sockaddr_in))) < 0) 
	on_error("failed to connect sock[%u][%u] on port %u",c,s,DST_PORT_BASE);
      sleep (1);
      //      EKA_LOG("Connected connect sock[%u][%u] on %s:%u -- sess_id[%u][%u]=%u",c,s,inet_ntoa(dst.sin_addr),DST_PORT_BASE,excGetCoreId(sess_id[c][s]),excGetSessionId(sess_id[c][s]),sess_id[c][s]);

      fast_path_thread[c][s] = std::thread(fastpath_thread_f,pEkaDev,sess_id[c][s],p2p_delay);
      fast_path_thread[c][s].detach();
    }
  }

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
  sleep (1);

  return 0;
}

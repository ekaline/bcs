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

#include "EkaDev.h"
#include "eka_fh.h"
#include "eka_macros.h"
#include "Efh.h"
#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "EkaCtxs.h"

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

volatile bool keep_work;

/* void hexDump (const char* desc, void *addr, int len) { */
/*     int i; */
/*     unsigned char buff[17]; */
/*     unsigned char *pc = (unsigned char*)addr; */
/*     if (desc != NULL) printf ("%s:\n", desc); */
/*     if (len == 0) { printf("  ZERO LENGTH\n"); return; } */
/*     if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; } */
/*     for (i = 0; i < len; i++) { */
/*         if ((i % 16) == 0) { */
/*             if (i != 0) printf ("  %s\n", buff); */
/*             printf ("  %04x ", i); */
/*         } */
/*         printf (" %02x", pc[i]); */
/*         if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.'; */
/*         else buff[i % 16] = pc[i]; */
/*         buff[(i % 16) + 1] = '\0'; */
/*     } */
/*     while ((i % 16) != 0) { printf ("   "); i++; } */
/*     printf ("  %s\n", buff); */
/* } */

#include "eka_default_callbacks4tests.incl"
#include "eka_exc_debug_module.h"
#include "eka_macros.h"

int createThread(const char* name, EkaThreadType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}

void fastpath_thread_f(EkaDev* pEkaDev, ExcConnHandle sess_id,uint p2p_delay) {
  //  char tcpsenddata[CHAR_SIZE] = "abcdefghijklmnopqrstuvwxyz";
  EkaDev* dev = pEkaDev;

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

    excSend (pEkaDev, sess_id, &pkt, sizeof(pkt));

    do {
      rxsize = excRecv(pEkaDev,sess_id, rx_buf, 1000);
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



  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  EkaCoreInitCtx ekaCoreInitCtx = {};

  if (strcmp(hostname, "xn03") == 0) {
    strcpy(dst_ip[0],DST_XN01_IP0);
    strcpy(dst_ip[1],DST_XN01_IP1);
  } else if (strcmp(hostname, "xn01") == 0) {
    strcpy(dst_ip[0],DST_XN03_IP0);
    strcpy(dst_ip[1],DST_XN03_IP1);
    //    inet_aton("10.0.0.117",(struct in_addr*)&ekaCoreInitCtx.attrs.host_ip);

    uint8_t src_mac[6] = {0x00,0x21,0xb2,0x19,0x17,0x90};
    memcpy(&ekaCoreInitCtx.attrs.src_mac_addr,src_mac,6);

  } else if (strcmp(hostname, "nylxlabavt-7") == 0) {
    strcpy(dst_ip[0],DST_AVT6_IP0);
    strcpy(dst_ip[1],DST_AVT6_IP1);
    inet_aton("192.168.0.89",(struct in_addr*)&ekaCoreInitCtx.attrs.host_ip);
    uint8_t next_hop[6] = {0x00,0x0F,0x53,0x0C,0x76,0x74};
    memcpy(&ekaCoreInitCtx.attrs.nexthop_mac,next_hop,6);

    uint8_t src_mac[6] = {0x00,0x21,0xb2,0x19,0x17,0x90};
    memcpy(&ekaCoreInitCtx.attrs.src_mac_addr,src_mac,6);
  } else on_error("Unsupported host %s\n", hostname);

  EkaDev* dev = pEkaDev;

  std::thread fast_path_thread[CORES][SESSIONS];
  static ExcConnHandle sess_id[CORES][SESSIONS];
  int sock[CORES][SESSIONS];

  for (uint8_t c = 0; c < testCores; c ++) {
    ekaCoreInitCtx.coreId = c;

    ((uint8_t*) &ekaCoreInitCtx.attrs.src_mac_addr)[5] += c; 

    ekaDevConfigurePort (pEkaDev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);

    struct sockaddr_in dst = {};
    dst.sin_addr.s_addr = inet_addr(dst_ip[c]);
    dst.sin_family = AF_INET;
    dst.sin_port = htons((uint16_t)DST_PORT_BASE);

    for (uint8_t s = 0; s < testSessions ;s ++) {
      if ((sock[c][s] = excSocket(pEkaDev,c,0,0,0)) < 0) on_error("failed to open sock[%u][%u]",c,s);
      EKA_LOG("sock[%u][%u] = %d",c,s,sock[c][s]);

      EKA_LOG("Trying to connect connect sock[%u][%u] on %s:%u",c,s,inet_ntoa(dst.sin_addr),DST_PORT_BASE);
      fflush(stderr);
      if ((sess_id[c][s] = excConnect(pEkaDev,sock[c][s],(struct sockaddr*) &dst, sizeof(struct sockaddr_in))) < 0) 
	on_error("failed to connect sock[%u][%u] on port %u",c,s,DST_PORT_BASE);
    }
  }
  //  sleep (1);
  for (uint8_t c = 0; c < testCores; c ++) {
    for (uint8_t s = 0; s < testSessions ;s ++) {
      //if (s == 0) continue; // PATCH !!!!

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

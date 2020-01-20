// TWO cores must be connected


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
#include <assert.h>


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

#define DST_PORT_BASE 22222

#define MAX_PAYLOAD_SIZE 192
#define MIN_PAYLOAD_SIZE 1

#define CHAR_SIZE  26+1

#define CORES 1

char tcpsenddata[CHAR_SIZE] = "abcdefghijklmnopqrstuvwxyz";

void     INThandler(int);
volatile bool keep_work;
//static uint16_t sess_id[32];
static ExcConnHandle sess_id[32];

//static pthread_t fastpath_thread_id[32] = {};

static  FILE *rx_file[32];
static  FILE *tx_file[32];

static int sock[32];
static  int num_of_sessions;
static FILE* subscriptions_file = NULL;
void hexDump (char *desc, void *addr, int len);

static EkaDev* pEkaDev = NULL;

EkaCoreInitCtx ekaCoreInitCtx = {};

EkaProp cboeInitCtxEntries[] = {
  {"efh.group.0.mcast.addr","233.130.124.39:30132"},
  {"efh.group.1.mcast.addr","233.130.124.39:30131"},
  {"efh.group.2.mcast.addr","233.130.124.36:30119"},
  {"efh.group.3.mcast.addr","233.130.124.37:30124"},
  {"efh.group.4.mcast.addr","233.130.124.32:30101"},
  {"efh.group.5.mcast.addr","233.130.124.36:30117"},
  {"efh.group.6.mcast.addr","233.130.124.38:30125"},
  {"efh.group.7.mcast.addr","233.130.124.39:30130"},
  {"efh.group.8.mcast.addr","233.130.124.37:30122"},
  {"efh.group.9.mcast.addr","233.130.124.38:30126"},
  {"efh.group.10.mcast.addr","233.130.124.38:30127"},
  {"efh.group.11.mcast.addr","233.130.124.33:30107"},
  {"efh.group.12.mcast.addr","233.130.124.36:30118"},
  {"efh.group.13.mcast.addr","233.130.124.35:30115"},
  {"efh.group.14.mcast.addr","233.130.124.35:30114"},
  {"efh.group.15.mcast.addr","233.130.124.33:30108"},
  {"efh.group.16.mcast.addr","233.130.124.37:30121"},
  {"efh.group.17.mcast.addr","233.130.124.33:30106"},
  {"efh.group.18.mcast.addr","233.130.124.32:30103"},
  {"efh.group.19.mcast.addr","233.130.124.34:30109"},
  {"efh.group.20.mcast.addr","233.130.124.37:30123"},
  {"efh.group.21.mcast.addr","233.130.124.34:30110"},
  {"efh.group.22.mcast.addr","233.130.124.32:30102"},
  {"efh.group.23.mcast.addr","233.130.124.35:30113"},
  {"efh.group.24.mcast.addr","233.130.124.39:30129"},
  {"efh.group.25.mcast.addr","233.130.124.32:30104"},
  {"efh.group.26.mcast.addr","233.130.124.36:30120"},
  {"efh.group.27.mcast.addr","233.130.124.35:30116"},
  {"efh.group.28.mcast.addr","233.130.124.38:30128"},
  {"efh.group.29.mcast.addr","233.130.124.34:30111"},
  {"efh.group.30.mcast.addr","233.130.124.34:30112"},
  {"efh.group.31.mcast.addr","233.130.124.33:30105"}
};
//EfcInitCtx cboeInitCtx = {0,cboeInitCtxEntries,false};
EkaProps cboeEkaProps = {
  .numProps = std::size(cboeInitCtxEntries),
  .props = cboeInitCtxEntries
};

EfcInitCtx cboeInitCtx = {&cboeEkaProps,NULL};


EkaProp miaxInitCtxEntries[] = {
  {"efh.group.0.mcast.addr","224.0.105.10:51010"},
  {"efh.group.1.mcast.addr","224.0.105.11:51011"},
  {"efh.group.2.mcast.addr","224.0.105.12:51012"},
  {"efh.group.3.mcast.addr","224.0.105.13:51013"},
  {"efh.group.4.mcast.addr","224.0.105.14:51014"},
  {"efh.group.5.mcast.addr","224.0.105.1:51001"},
  {"efh.group.6.mcast.addr","224.0.105.15:51015"},
  {"efh.group.7.mcast.addr","224.0.105.16:51016"},
  {"efh.group.8.mcast.addr","224.0.105.17:51017"},
  {"efh.group.9.mcast.addr","224.0.105.18:51018"},
  {"efh.group.10.mcast.addr","224.0.105.19:51019"},
  {"efh.group.11.mcast.addr","224.0.105.20:51020"},
  {"efh.group.12.mcast.addr","224.0.105.21:51021"},
  {"efh.group.13.mcast.addr","224.0.105.22:51022"},
  {"efh.group.14.mcast.addr","224.0.105.23:51023"},
  {"efh.group.15.mcast.addr","224.0.105.24:51024"},
  {"efh.group.16.mcast.addr","224.0.105.2:51002"},
  {"efh.group.17.mcast.addr","224.0.105.3:51003"},
  {"efh.group.18.mcast.addr","224.0.105.4:51004"},
  {"efh.group.19.mcast.addr","224.0.105.5:51005"},
  {"efh.group.20.mcast.addr","224.0.105.64:51049"},
  {"efh.group.21.mcast.addr","224.0.105.6:51006"},
  {"efh.group.22.mcast.addr","224.0.105.65:51050"},
  {"efh.group.23.mcast.addr","224.0.105.66:51051"},
  {"efh.group.24.mcast.addr","224.0.105.67:51052"},
  {"efh.group.25.mcast.addr","224.0.105.68:51053"},
  {"efh.group.26.mcast.addr","224.0.105.69:51054"},
  {"efh.group.27.mcast.addr","224.0.105.70:51055"},
  {"efh.group.28.mcast.addr","224.0.105.71:51056"},
  {"efh.group.29.mcast.addr","224.0.105.72:51057"},
  {"efh.group.30.mcast.addr","224.0.105.73:51058"},
  {"efh.group.31.mcast.addr","224.0.105.74:51059"},
  {"efh.group.32.mcast.addr","224.0.105.7:51007"},
  {"efh.group.33.mcast.addr","224.0.105.75:51060"},
  {"efh.group.34.mcast.addr","224.0.105.76:51061"},
  {"efh.group.35.mcast.addr","224.0.105.77:51062"},
  {"efh.group.36.mcast.addr","224.0.105.78:51063"},
  {"efh.group.37.mcast.addr","224.0.105.79:51064"},
  {"efh.group.38.mcast.addr","224.0.105.80:51065"},
  {"efh.group.39.mcast.addr","224.0.105.81:51066"},
  {"efh.group.40.mcast.addr","224.0.105.82:51067"},
  {"efh.group.41.mcast.addr","224.0.105.83:51068"},
  {"efh.group.42.mcast.addr","224.0.105.84:51069"},
  {"efh.group.43.mcast.addr","224.0.105.8:51008"},
  {"efh.group.44.mcast.addr","224.0.105.85:51070"},
  {"efh.group.45.mcast.addr","224.0.105.86:51071"},
  {"efh.group.46.mcast.addr","224.0.105.87:51072"},
  {"efh.group.47.mcast.addr","224.0.105.9:51009"},

  {"efh.miax.recovery.group.0.addr","224.0.105.10:51010"},
  {"efh.miax.recovery.group.1.addr","224.0.105.11:51011"},
  {"efh.miax.recovery.group.2.addr","224.0.105.12:51012"},
  {"efh.miax.recovery.group.3.addr","224.0.105.13:51013"},
  {"efh.miax.recovery.group.4.addr","224.0.105.14:51014"},
  {"efh.miax.recovery.group.5.addr","224.0.105.1:51001"},
  {"efh.miax.recovery.group.6.addr","224.0.105.15:51015"},
  {"efh.miax.recovery.group.7.addr","224.0.105.16:51016"},
  {"efh.miax.recovery.group.8.addr","224.0.105.17:51017"},
  {"efh.miax.recovery.group.9.addr","224.0.105.18:51018"},
  {"efh.miax.recovery.group.10.addr","224.0.105.19:51019"},
  {"efh.miax.recovery.group.11.addr","224.0.105.20:51020"},
  {"efh.miax.recovery.group.12.addr","224.0.105.21:51021"},
  {"efh.miax.recovery.group.13.addr","224.0.105.22:51022"},
  {"efh.miax.recovery.group.14.addr","224.0.105.23:51023"},
  {"efh.miax.recovery.group.15.addr","224.0.105.24:51024"},
  {"efh.miax.recovery.group.16.addr","224.0.105.2:51002"},
  {"efh.miax.recovery.group.17.addr","224.0.105.3:51003"},
  {"efh.miax.recovery.group.18.addr","224.0.105.4:51004"},
  {"efh.miax.recovery.group.19.addr","224.0.105.5:51005"},
  {"efh.miax.recovery.group.20.addr","224.0.105.64:51049"},
  {"efh.miax.recovery.group.21.addr","224.0.105.6:51006"},
  {"efh.miax.recovery.group.22.addr","224.0.105.65:51050"},
  {"efh.miax.recovery.group.23.addr","224.0.105.66:51051"}
};
//EfcInitCtx miaxInitCtx = {0,miaxInitCtxEntries,false};
//EkaProps miaxEkaProps(miaxInitCtxEntries);

EkaProps miaxEkaProps = {
  .numProps = std::size(miaxInitCtxEntries),
  .props = miaxInitCtxEntries
};

EfcInitCtx miaxInitCtx = {&miaxEkaProps,NULL};

EkaProp phlxInitCtxEntries[] = {
  //  UDP configurations OPTIONS
  {"efh.group.32.mcast.addr","233.47.179.104:18016"},
  {"efh.group.33.mcast.addr","233.47.179.105:18017"},
  {"efh.group.34.mcast.addr","233.47.179.106:18018"},
  {"efh.group.35.mcast.addr","233.47.179.107:18019"},
  {"efh.group.36.mcast.addr","233.47.179.110:18022"},
  {"efh.group.37.mcast.addr","233.47.179.111:18023"},
  {"efh.group.38.mcast.addr","233.47.179.112:18032"},
  {"efh.group.39.mcast.addr","233.47.179.113:18033"},
  {"efh.group.40.mcast.addr","233.47.179.114:18034"},
  {"efh.group.41.mcast.addr","233.47.179.115:18035"},
  {"efh.group.42.mcast.addr","233.47.179.118:18038"},
  {"efh.group.43.mcast.addr","233.47.179.119:18039"},

  //  UDP configurations ORDERS
  {"efh.group.0.mcast.addr","233.47.179.120:18064"},
  {"efh.group.1.mcast.addr","233.47.179.121:18065"},
  {"efh.group.2.mcast.addr","233.47.179.122:18066"},
  {"efh.group.3.mcast.addr","233.47.179.123:18067"},
  {"efh.group.4.mcast.addr","233.47.179.124:18080"},
  {"efh.group.5.mcast.addr","233.47.179.125:18081"},
  {"efh.group.6.mcast.addr","233.47.179.126:18082"},
  {"efh.group.7.mcast.addr","233.47.179.127:18083"}
};
//EfcInitCtx phlxInitCtx = {0,phlxInitCtxEntries,false};
//EkaProps phlxEkaProps(phlxInitCtxEntries);

EkaProps phlxEkaProps = {
  .numProps = std::size(phlxInitCtxEntries),
  .props = phlxInitCtxEntries
};
EfcInitCtx phlxInitCtx = {&phlxEkaProps,NULL};

EkaProp nasdaqInitCtxEntries[] = {
  //  UDP configurations 
  {"efh.group.0.mcast.addr","233.54.12.74:18002"},
  {"efh.group.1.mcast.addr","233.54.12.72:18000"},
  {"efh.group.2.mcast.addr","233.54.12.73:18001"},
  {"efh.group.3.mcast.addr","233.54.12.75:18003"}
};
//EfcInitCtx nasdaqInitCtx = {0,nasdaqInitCtxEntries,false};
//EkaProps nasdaqEkaProps(nasdaqInitCtxEntries);
EkaProps nasdaqEkaProps = {
  .numProps = std::size(nasdaqInitCtxEntries),
  .props = nasdaqInitCtxEntries
};
EfcInitCtx nasdaqInitCtx = {&nasdaqEkaProps,NULL};


EkaProp gemInitCtxEntries[] = {
  {"efh.group.0.mcast.addr","233.54.12.148:18003"}
};
//EfcInitCtx gemInitCtx = {0,gemInitCtxEntries,false};
//EkaProps gemEkaProps(gemInitCtxEntries);
EkaProps gemEkaProps = {
  .numProps = std::size(gemInitCtxEntries),
  .props = gemInitCtxEntries
};

EfcInitCtx gemInitCtx = {&gemEkaProps,NULL};

eka_dev_t* global_dev = NULL;
void  INThandler(int sig) {
     signal(sig, SIG_IGN);
     printf("Ctrl-C detected, quitting\n");
     keep_work = false;
}




void *fastpath_thread_f(void* ptr, uint8_t s) {
  //  fh_pthread_args_t* args = (fh_pthread_args_t*) ptr;
  //  eka_dev_t* dev = args->dev;

  //  EfcCtx* pEfcCtx = (EfcCtx*) ptr;

  //  uint8_t s = args->gr_id;
  //  free(args);
  int iteration = 0;
  char rx_buffer[1000];
  char payload[MAX_PAYLOAD_SIZE] = {};

  printf ("%s: starting fastpath for session %d, session id %d\n", __func__, s, sess_id[s]);

  while (keep_work) {
    //int send_size = MIN_PAYLOAD_SIZE + random () % (MAX_PAYLOAD_SIZE-MIN_PAYLOAD_SIZE);
       int send_size = 3;
    for (int character=0; character<send_size ;character++) {
      payload[character] = tcpsenddata[random () % (CHAR_SIZE-1) ];
      //      payload[character] = 'z';
    }
    //          payload[0]++;

    //    exc_send( sess_id[s], payload, send_size);
    printf ("%s: trying to send %u bytes\n",__func__,send_size);
    excSend (pEkaDev, sess_id[s], payload, send_size);
    printf("TX:");
    //    write(fileno(tx_file[s]), payload, send_size);
    //    fprintf(tx_file[s],"\n");
    //    fflush(tx_file[s]);

    //    int rxsize = exc_recv(sess_id[s], rx_buffer, 1000);
    int rxsize = excRecv(pEkaDev,sess_id[s], rx_buffer, 1000);

    printf("RX %d bytes (iteration %d)\n",rxsize,iteration++);
    //    hexDump("RX_BUFFER ",rx_buffer,rxsize);

    //    write(fileno(rx_file[s]), rx_buffer, send_size);
    //    fprintf(rx_file[s],"\n");
    //    fflush(rx_file[s]);
           sleep (1);
	   //    usleep(1000000);
  }

  printf ("%s: terminating for session %d\n", __func__, s);

  return NULL;
}

void configure_phlx(EfcCtx* pEfcCtx) {
  for (int s=0; s<64 ;s++) {
    efcSetGroupSesCtx(pEfcCtx,s,0);
  }

  // Subscribing
  //  add_subscription(NULL,0x2ebec);
  //pkt:1928, msg:1: size:14, PHLX_OPTIONS_MSG_BEST_BID_UPDATE_SHORT: Nanoseconds: 4938900 (0x4b5c94), OptionID: 191468 (0x2ebec), QuoteCondition: ' ' (Alpha), Price: 2000 (0x7d0), Size: 10 (0xa),

  //  add_subscription(NULL,0x7bd6f);
  //pkt 4015
  uint64_t SecurityIds[] = { 0x2ebec, 0x7bd6f };
  uint numSec2subscr = sizeof(SecurityIds) / sizeof(uint64_t);
  efcEnableFiringOnSec(pEfcCtx, SecurityIds, numSec2subscr);

  //  subscribe_done(NULL);

  // SEC CTX
  /* sec_ctx_t ctx = {}; */
  /* ctx.lower_bytes_of_sec_id = 0xec; */
  /* ctx.bid_min_price = 1; */
  /* ctx.ask_max_price = 210; //x100 */
  /* ctx.size = 12; */
  /* ctx.ver_num = 0xaf; */

  SecCtx secCtx = {};
  secCtx.bidMinPrice = 1; //x100, should be nonzero
  secCtx.askMaxPrice = 210;  //x100
  secCtx.size = 12;
  secCtx.verNum = 0xaf;

  for (uint i = 0; i < numSec2subscr; i ++) {
    EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, SecurityIds[i]);
    if (handle < 0) on_error ("handle = %ld for SecurityId %ju\n",handle,SecurityIds[i]);
    secCtx.lowerBytesOfSecId = SecurityIds[i] & 0xFF;
    efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
  }

  /* set_full_ctx( 0x2ebec , &ctx, 0); */

  /* ctx.lower_bytes_of_sec_id = 0x6f; */
  /* set_full_ctx( 0x7bd6f , &ctx, 0); */

  // SESSION CTX
  SesCtx session_ctx[32];

  for (int s=0; s<num_of_sessions ;s++) {
    printf("Configuring session number %d \n", s);
    session_ctx[s].clOrdId = s*100;
    session_ctx[s].nextSessionId = (s==(num_of_sessions-1)) ? 255 : s + 1;
    session_ctx[s].equote_mpid_sqf_badge = s*10;
    efcSetSesCtx(pEfcCtx, s , &session_ctx[s]);
    efcSetSesCtx(pEfcCtx, s+128 , &session_ctx[s]);
  }

}

void configure_nasdaq(EfcCtx* pEfcCtx) {
  //  UDP configurations 
  eka_dev_t* dev = pEfcCtx->dev;
  for (int s=0; s<64 ;s++) {
    efcSetGroupSesCtx(pEfcCtx,s,0);
  }

  subscriptions_file = fopen("/local/dumps/nasdaq/itto4.0/new_itto.20180221.add_order.pid", "r");
  if (!subscriptions_file)
    {
      printf("couldn't open file /local/dumps/nasdaq/itto4.0/new_itto.20180221.add_order.pid\n");
      exit(1);
    }
  
  uint64_t subscription_id = 0;
  uint i = 0;
  while (EOF != fscanf(subscriptions_file, "%jx\n", &subscription_id))  {
    if (subscription_id) {
      dev->subscr.sec_id[i++] = subscription_id;
      //	  retval = add_subscription(NULL,subscription_id);
      //	  printf ("Subscribe for 0x%x\n",subscription_id);	    
    }
  }
  dev->subscr.cnt = i;
  //  subscribe_done(NULL);
  printf ("Subscribing on %u securities\n",dev->subscr.cnt);
  efcEnableFiringOnSec(pEfcCtx, dev->subscr.sec_id, dev->subscr.cnt);

  rewind (subscriptions_file);
  // NASDAQ_NOM_MSG_ADD_ORDER_SHORT (a): TrackingNumber: 0 (0x0),Timestamp: 34849111084071 (0x1fb1f0e30c27),OrderReferenceNumberDelta: 3153424616 (0xbbf570e8),MarketSide: B (Alphanumeric),OptionID: 162095 (0x2792f),Price: 6500 (0x1964),Volume: 20 (0x14),

  // SEC CTX
  SecCtx secCtx = {};
  secCtx.bidMinPrice = 1; //x100, should be nonzero
  secCtx.askMaxPrice = 0;  //x100
  secCtx.size = 1;
  secCtx.verNum = 0xaf;
  secCtx.lowerBytesOfSecId = 0;

  for (uint i = 0; i < dev->subscr.cnt; i++) {
    EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, dev->subscr.sec_id[i]);
    if (handle < 0) on_error ("%s: Security ID %ju is not subscribed\n",__func__,dev->subscr.sec_id[i]);
    printf ("%s: Security ID %ju, Handle %ld\n",__func__,dev->subscr.sec_id[i],handle);
    secCtx.lowerBytesOfSecId = dev->subscr.sec_id[i] & 0xff;
    EkaOpResult res = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
    if (res != EKA_OPRESULT__OK) on_error ("%s: Failed on efcSetStaticSecCtx for Security %ju\n",__func__,dev->subscr.sec_id[i]);
  }

  // SESSION CTX
  SesCtx session_ctx[32];

  for (int s=0; s<num_of_sessions ;s++) {
    printf("Configuring session number %d and %d\n", s, s+128);
    
    session_ctx[s].clOrdId = s*100;
    session_ctx[s].nextSessionId = (s==(num_of_sessions-1)) ? 255 : s + 1;
    session_ctx[s].equote_mpid_sqf_badge = s*10;
    efcSetSesCtx(pEfcCtx, s , &session_ctx[s]);
    efcSetSesCtx(pEfcCtx, s+128 , &session_ctx[s]);
  }

}

void configure_gem(EfcCtx* pEfcCtx) {
  for (int s=0; s<48 ;s++) {
    efcSetGroupSesCtx(pEfcCtx,s,0);
  }

  // Subscribing
  //  add_subscription(NULL,0x9d3ec);
  //GEM_TQF_MSG_BEST_ASK_UPDATE_SHORT (a): Timestamp: 34209642902104 (0x1f1d0d9cf258),OptionID: 644076 (0x9d3ec),QuoteCondition:   (Alphanumeric),MarketOrderSize: 0 (0x0),Price: 3500 (0xdac),Size: 2 (0x2),CustSize: 5 (0x5),ProCustSize: 2 

  //  subscribe_done(NULL);

  uint64_t SecurityIds[] = { 0x9d3ec };
  uint numSec2subscr = sizeof(SecurityIds) / sizeof(uint64_t);
  printf ("numSec2subscr = %u\n",numSec2subscr);
  efcEnableFiringOnSec(pEfcCtx, SecurityIds, numSec2subscr);

  // SEC CTX
  /* sec_ctx_t ctx = {}; */
  /* ctx.lower_bytes_of_sec_id = 0xec; */
  /* ctx.bid_min_price = 0; */
  /* ctx.ask_max_price = 36; //x100 */
  /* ctx.size = 1; */
  /* ctx.ver_num = 0xaf; */
  /* set_full_ctx( 0x9d3ec , &ctx, 0); */

  SecCtx secCtx = {};
  secCtx.bidMinPrice = 0;
  secCtx.askMaxPrice = 36;
  secCtx.size = 1;
  secCtx.verNum = 0xaf;
  secCtx.lowerBytesOfSecId = 0;

  for (uint i = 0; i < numSec2subscr; i ++) {
    EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, SecurityIds[i]);
    if (handle < 0) on_error ("handle = %ld for SecurityId %ju\n",handle,SecurityIds[i]);
    secCtx.lowerBytesOfSecId = SecurityIds[i] & 0xFF;
    efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
  }

  // SESSION CTX
  SesCtx session_ctx[32];

  for (int s=0; s<num_of_sessions ;s++) {
    printf("Configuring session number %d \n", s);
    session_ctx[s].clOrdId = s*100;
    session_ctx[s].nextSessionId = (s==(num_of_sessions-1)) ? 255 : s + 1;
    session_ctx[s].equote_mpid_sqf_badge = s*10;
    efcSetSesCtx(pEfcCtx, s , &session_ctx[s]);
    efcSetSesCtx(pEfcCtx, s+128 , &session_ctx[s]);
  }

}

void configure_miax(EfcCtx* pEfcCtx) {
 
  for (int s=0; s<48 ;s++) {
    efcSetGroupSesCtx(pEfcCtx,s,0);
  }

  // Subscribing
  //  add_subscription(NULL,0x2000a8a);
  uint64_t SecurityIds[] = { 0x2000a8a };
  uint numSec2subscr = sizeof(SecurityIds) / sizeof(uint64_t) ;
  efcEnableFiringOnSec(pEfcCtx, SecurityIds, numSec2subscr);


  //MIAX_TOM_MSG_ORDER_ASK_SHORT: MessageType: 'O', Timestamp: 508354762 (0x1e4ce0ca), ProductID: 33557130 (0x2000a8a), MBBOPrice: 5000 (0x1388), MBBOSize: 15 (0xf), MBBOPriorityCustomerSize: 15 (0xf), MBBOCondition: 'B',

  //  subscribe_done(NULL);

  // SEC CTX

  SecCtx secCtx = {};
  secCtx.bidMinPrice = 0;
  secCtx.askMaxPrice = 51;  //x100
  secCtx.size = 1;
  secCtx.verNum = 0xaf;
  //  secCtx.lowerBytesOfSecId = SecurityIds[0] & 0xFF;

  /* sec_ctx_t ctx = {}; */
  /* ctx.lower_bytes_of_sec_id = 0x8a; */
  /* ctx.bid_min_price = 0; */
  /* ctx.ask_max_price = 51; //x100 */
  /* ctx.size = 1; */
  /* ctx.ver_num = 0xaf; */
  //  set_full_ctx( 0x2000a8a , &ctx, 0);

  for (uint i = 0; i < numSec2subscr; i ++) {
    EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, SecurityIds[i]);
    if (handle < 0) on_error ("handle = %ld for SecurityId %ju\n",handle,SecurityIds[i]);
    secCtx.lowerBytesOfSecId = SecurityIds[i] & 0xFF;
    efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
  }

  // SESSION CTX
  SesCtx session_ctx[32];

  for (int s=0; s<num_of_sessions ;s++) {
    printf("Configuring session number %d \n", s);
    session_ctx[s].clOrdId = s*100;
    session_ctx[s].nextSessionId = (s==(num_of_sessions-1)) ? 255 : s + 1;
    session_ctx[s].equote_mpid_sqf_badge = s*10;
    efcSetSesCtx(pEfcCtx, s , &session_ctx[s]);
    if (CORES==2) efcSetSesCtx(pEfcCtx, s+128 , &session_ctx[s]);
  }
  

}


void configure_cboe(EfcCtx* pEfcCtx) {
   eka_dev_t* dev = pEfcCtx->dev;

  for (int s=0; s<48 ;s++) {
    efcSetGroupSesCtx(pEfcCtx,s,0);
  }

  subscriptions_file = fopen("/local/dumps/cboe_pitch/symbols-bats.txt", "r");
  if (!subscriptions_file)
    {
      printf("couldn't open file /local/dumps/cboe_pitch/symbols-bats.txt\n");
      exit(1);
    }

  // Subscribing
  uint64_t subscription_id = 0;
  uint32_t i = 0;

  printf("%s: trying to subscribe\n",__func__);

  while (EOF != fscanf(subscriptions_file, "%s\n", (char*)&subscription_id))  {
    if (subscription_id) {
      subscription_id = be64toh(subscription_id)>>16;
      dev->subscr.sec_id[i++] = subscription_id;
      //	  retval = add_subscription(NULL,subscription_id);
    }
  }
  dev->subscr.cnt = i;
  printf ("Subscribing on %u securities\n",dev->subscr.cnt);
  efcEnableFiringOnSec(pEfcCtx, dev->subscr.sec_id, dev->subscr.cnt);

  rewind (subscriptions_file);

  //  uint64_t SecurityIds[] = { 0x303163477276 }; // "01cGrv" 
  //  uint numSec2subscr = sizeof(SecurityIds) / sizeof(uint64_t) ;
  //  efcEnableFiringOnSec(pEfcCtx, SecurityIds, numSec2subscr);

  // PKT: 5 MSG: 0 CBOE_MSG_ADD_ORDER_SHORT: TimeOffset:  (0xfbb3ca8) Side: B  Quantity: 1 (0x1) Symbol: 01cGrv  Price: 13000 (0x32c8) AddFlags: 1 (0x1)

  // SEC CTX
  SecCtx secCtx = {};
  secCtx.bidMinPrice = 1;
  secCtx.askMaxPrice = 5100;  //x100
  secCtx.size = 1;
  secCtx.verNum = 0xaf;

  for (uint i = 0; i < dev->subscr.cnt; i++) {
    EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, dev->subscr.sec_id[i]);
    if (handle < 0) on_error ("%s: Security ID %ju is not subscribed\n",__func__,dev->subscr.sec_id[i]);
    printf ("%s: Security ID %ju, Handle %ld\n",__func__,dev->subscr.sec_id[i],handle);
    secCtx.lowerBytesOfSecId = dev->subscr.sec_id[i] & 0xff;
    EkaOpResult res = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
    if (res != EKA_OPRESULT__OK) on_error ("%s: Failed on efcSetStaticSecCtx for Security %ju\n",__func__,dev->subscr.sec_id[i]);
  }

  /* for (uint i = 0; i < dev->subscr.cnt; i ++) { */
  /*   EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, SecurityIds[i]); */
  /*   if (handle < 0) on_error ("handle = %ld for SecurityId %ju\n",handle,SecurityIds[i]); */
  /*   secCtx.lowerBytesOfSecId = SecurityIds[i] & 0xFF; */
  /*   efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0); */
  /* } */

  // SESSION CTX
  SesCtx session_ctx[32];

  for (int s=0; s<num_of_sessions ;s++) {
    printf("Configuring session number %d \n", s);
    session_ctx[s].clOrdId = s*100;
    session_ctx[s].nextSessionId = (s==(num_of_sessions-1)) ? 255 : s + 1;
    session_ctx[s].equote_mpid_sqf_badge = s*10;
    efcSetSesCtx(pEfcCtx, s , &session_ctx[s]);
    if (CORES==2) efcSetSesCtx(pEfcCtx, s+128 , &session_ctx[s]);
  }
  

}


void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fire_report_buf, size_t size) {
  printf ("%s: pEfcCtx=%p, \n",__func__,pEfcCtx);
  if (size == 0) {
    //      printf ("No report\n");
    //      set_full_ctx( 0x8005fff , &ctx, 0);
    //      sleep (1);
    on_error ("%s: size = 0\n",__func__);
  }
  printf ("\n");
  efcPrintFireReport(pEfcCtx, (EfcReportHdr*)fire_report_buf);
  printf ("Rearming...\n");
  efcEnableController(pEfcCtx,1);
}


int main(int argc, char *argv[]) {
  keep_work = true;

  // Parsing first argument for number of sessions, default 1
  bool send_tcp_traffic = true;

  if ( argc == 2 ) {
    num_of_sessions = strtoumax(argv[1], NULL,10);
    if (num_of_sessions > 32) on_error ("%s: maximum number of sessions is 32\n", __func__);
    if (num_of_sessions == 0) {
      printf ("Zero sessions specified - running with single TCP sessions without traffic\n");
      num_of_sessions = 1;
      send_tcp_traffic = false;
    }
  } else {
    num_of_sessions = 1;
  }

  signal(SIGINT, INThandler);

  // Opening device
  //  struct exc_init_attrs attrs = {};
  //  exc_init(0,&attrs);

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  for (uint c=0; c<CORES; c++) {
    ekaCoreInitCtx.coreId = c;
    ekaDevConfigurePort (pEkaDev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);
  }

  EfhInitCtx efhInitCtx = {};
  efhInitCtx.recvSoftwareMd = false;

  EfhCtx* pEfhCtx = NULL;
  EfcCtx* pEfcCtx = NULL;

  efhInit(&pEfhCtx, pEkaDev, &efhInitCtx ); // doing nothing

  eka_dev_t* dev = pEkaDev;
  global_dev = dev;

  char hostname[1024];
  gethostname(hostname, 1024);

  char dst_ip0[1024];
  char dst_ip1[1024];

  if (strcmp(hostname, "xn03") == 0) {
    strcpy(dst_ip0,DST_XN01_IP0);
    strcpy(dst_ip1,DST_XN01_IP1);
  } 
  else if (strcmp(hostname, "xn01") == 0) {
    strcpy(dst_ip0,DST_XN03_IP0);
    strcpy(dst_ip1,DST_XN03_IP1);
  }
  else {
    printf("Trying to run on unknown host %s\n", hostname);
    return (0);
  }

  EfcInitCtx* pEfcInitCtx = NULL;
  printf("\n\n------------ TEST PARAMETERS -----------------\n");
  printf("Hostname:\t%s\n",hostname);

  switch(dev->hw.feed_ver) {
  case SN_GEMX:
    printf("HW:\t\tGEM");
    pEfcInitCtx = &gemInitCtx;
    //    pEfcInitCtx->numEntries = sizeof(gemInitCtxEntries) / sizeof(EfcInitCtxEntry);
    break;
  case SN_NASDAQ:
    printf("HW:\t\tNASDAQ");
    pEfcInitCtx = &nasdaqInitCtx;
    //    pEfcInitCtx->numEntries = sizeof(nasdaqInitCtxEntries) / sizeof(EfcInitCtxEntry);
    break;
  case SN_PHLX:
    printf("HW:\t\tPHLX");
    pEfcInitCtx = &phlxInitCtx;
    //    pEfcInitCtx->numEntries = sizeof(phlxInitCtxEntries) / sizeof(EfcInitCtxEntry);
    break;
  case SN_MIAX:
    printf("HW:\t\tMIAX");
    pEfcInitCtx = &miaxInitCtx;
    //    pEfcInitCtx->numEntries = sizeof(miaxInitCtxEntries) / sizeof(EfcInitCtxEntry);
    break;
  case SN_CBOE:
    printf("HW:\t\tCBOE");
    pEfcInitCtx = &cboeInitCtx;
    //    pEfcInitCtx->numEntries = sizeof(cboeInitCtxEntries) / sizeof(EfcInitCtxEntry);
    break;
  default:
    on_error ("Unknown dev->hw.feed_ver: %u\n",dev->hw.feed_ver);
  }

  //  EfcCtx* pEfcCtx = &efc_ctx;
  efcInit( &pEfcCtx,pEkaDev, pEfcInitCtx);

  printf("Connect to DST_IP core0:%s core1:%s . Base DST_PORT is %d\n",dst_ip0,dst_ip1,DST_PORT_BASE);
  printf("Test will use %d TCP sessions at Core 0 AND Core1\n",num_of_sessions);

  printf("\n\n------------ TEST PARAMETERS -----------------\n\n\n");
  //  printf ("efc_ctx.dev = %p, devid = %d\n",efc_ctx.dev, efc_ctx.dev->dev_id);


  switch(dev->hw.feed_ver) {
  case SN_GEMX:
    configure_gem(pEfcCtx);
    break;
  case SN_NASDAQ:
    configure_nasdaq(pEfcCtx);
    break;
  case SN_PHLX:
    configure_phlx(pEfcCtx);
    break;
  case SN_MIAX:
    configure_miax(pEfcCtx);
    break;
  case SN_CBOE:
    configure_cboe(pEfcCtx);
    break;
  }

  // Starting
  EfcStratGlobCtx app_params = {};
  app_params.report_only = 0;
  //  app_params.no_report_on_exception = 0;
  app_params.debug_always_fire = 0;
  app_params.debug_always_fire_on_unsubscribed = 0;
  app_params.max_size = 1000;
  app_params.watchdog_timeout_sec = 100000;

  //  init_strategy(&app_params,NULL);
  efcInitStrategy(pEfcCtx, &app_params);

  //  eka_write(dev,P4_FATAL_DEBUG,(uint64_t) 0xefa0beda);
  //  char fire_report_buf[1280] = {};
  
  char filename[64] = {};

  // Opening Files
  for (int s=0; s<num_of_sessions ;s++) {
    sprintf(filename, "/tmp/fastsend_rx_session_%d.txt", s);
    rx_file[s] = fopen(filename, "w+");
    sprintf(filename, "/tmp/fastsend_tx_session_%d.txt", s);
    tx_file[s] = fopen(filename, "w+");
  }
  /* for (int s=0; s<1 ;s++) { */
  /*   sprintf(filename, "/tmp/fastsend_rx_session_%d.txt", s); */
  /*   rx_file[s] = fopen(filename, "w+"); */
  /*   sprintf(filename, "/tmp/fastsend_tx_session_%d.txt", s); */
  /*   tx_file[s] = fopen(filename, "w+"); */
  /* } */

 
  assert(pEfcCtx != NULL);
  assert(pEfcCtx->dev != NULL);

  printf ("Before efcRun: pEfcCtx=%p, pEfcCtx->dev=%p\n",pEfcCtx,pEfcCtx->dev);

  EfcRunCtx runCtx = {};
  runCtx.onEfcFireReportCb = onFireReport;


  efcRun(pEfcCtx, &runCtx );
  while (! (pEfcCtx->dev->serv_thr).active) {
    usleep(0);
  }

  // Creating sockets
  for (int s=0; s<num_of_sessions ;s++) {
    printf("Trying to create socket number %d ..... ", s);
    sock[s] = excSocket(pEkaDev,0,      0,0,0); // on Core#0, ignoring other params
    printf("created socket number %d\n", sock[s]);
  }

  if (CORES==2) {
    for (int s=num_of_sessions; s<2*num_of_sessions ;s++) {
      printf("Trying to create socket number %d ..... ", s);
      sock[s] = excSocket(pEkaDev,1,      0,0,0); // on Core#1, ignoring other params
      printf("created socket number %d\n", sock[s]);
    }
  }
  
  // Creating socket parameters and connecting
  struct sockaddr_in dst[32] = {};
  for (int s=0; s<num_of_sessions ;s++) {
    dst[s].sin_addr.s_addr = inet_addr(dst_ip0);
    dst[s].sin_family = AF_INET;
    dst[s].sin_port = htons((uint16_t)(DST_PORT_BASE));
    //    dst[s].sin_port = htons((uint16_t)(DST_PORT_BASE + s));
    printf("Trying to connect to socket number %d ..... \n", s);
    sess_id[s] = excConnect(pEkaDev,sock[s],(struct sockaddr*) &dst[s], sizeof(struct sockaddr_in));
    printf("connected with sessions id  %d \n",sess_id[s]);
  }

  if (CORES==2) {
    printf("Moving to next core\n");
    for (int s=num_of_sessions; s<2*num_of_sessions ;s++) {
      dst[s].sin_addr.s_addr = inet_addr(dst_ip1);
      dst[s].sin_family = AF_INET;
      //      dst[s].sin_port = htons((uint16_t)(DST_PORT_BASE + s));
      dst[s].sin_port = htons((uint16_t)(DST_PORT_BASE));
      printf("Trying to connect to socket number %d ..... \n", s);
      sess_id[s] = excConnect(pEkaDev,sock[s],(struct sockaddr*) &dst[s], sizeof(struct sockaddr_in));
      printf("connected with sessions id  %d \n",sess_id[s]);
    }
  }

  // Creating TX/RX threads
  if (send_tcp_traffic) {
    for (int s=0; s<num_of_sessions ;s++) {
      printf ("Creating fastpath threads for session %d\n",s);
      /*     pthread_t curr_fastpath_thread_id = fastpath_thread_id[s]; */
      /*     //      void* s_ptr = (void*)&s; */

      /* fh_pthread_args_t* gap_args = (fh_pthread_args_t*)malloc(sizeof(fh_pthread_args_t)); */
      /* if (gap_args == NULL) on_error ("%s: malloc failed\n",__func__); */
      /* gap_args->dev = efc_ctx.dev; */
      /* gap_args->gr_id = (uint8_t)s; */
      /* //  pthread_create (&(dev->fh.gr[gr_id].gap_closing_thread_id),NULL,*fh_gap_closing_thread, (void*)gap_args); // Launching GAP per group */

      /*     pthread_create( */
      /* 		     &curr_fastpath_thread_id,  */
      /* 		     NULL,  */
      /* 		     *fastpath_thread_f, */
      /* 		     (void*)gap_args); */
      /*   } */
      std::thread fast_path_thread(fastpath_thread_f,(void*)pEfcCtx,(uint8_t)s);
      fast_path_thread.detach();
    }
  }


  // Enabling strategy
  printf("Enabling strategy\n");
  //  enable_strategy(1);
  efcEnableController(pEfcCtx,1);


  //  printf ("EFH OPEN\n");
  //  efh_open(NULL);
  while (keep_work) {
    usleep(0);
  }

  /* if (send_tcp_traffic) { */
  /*   for (int s=0; s<num_of_sessions ;s++) { */
  /*     printf ("Terminating fastpath threads for session %d out of %u\n",s,num_of_sessions); */
  /*     pthread_join(fastpath_thread_id[s], NULL); */
  /*   } */
  /* } */

  /* printf ("All fastpath threads joined\n"); */
  /* sleep(3); */

  // Closing Sockets
  printf("Closing %d sockets\n",num_of_sessions);
  for (int s=0; s<num_of_sessions ;s++) {
    close(sock[s]);
  }
  
  printf("Closing device\n");
  ekaDevClose(pEkaDev);

  for (int s=0; s<num_of_sessions ;s++) {
    fclose(rx_file[s]);
    fclose(tx_file[s]);
  }
  /* for (int s=0; s<1 ;s++) { */
  /*   fclose(rx_file[s]); */
  /*   fclose(tx_file[s]); */
  /* } */

  
  return 0;
}

/* void hexDump (char *desc, void *addr, int len) { */
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

#include <arpa/inet.h>
#include <inttypes.h>
#include <math.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#if 0
#include "EkaCmeBook.h"
#include "EkaCmeParser.h"
#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaHwInternalStructs.h"
#include "EkaIceBook.h"
#include "EkaIceParser.h"
#include "EkaProd.h"
#include "EkaStrat.h"
#include "EkaUdpChannel.h"
#include "EkaUdpSess.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"

void setThreadAffinityName(EkaDev *dev, pthread_t thread,
                           const char *name, uint cpuCore);

/* ----------------------------------------------------- */
void disableHwUdp(EkaDev* dev) {
  uint64_t fire_rx_tx_en = eka_read(dev,ENABLE_PORT);
  fire_rx_tx_en &= 0xffffffff7fffffff; //[31] = 0, kill udp
  EKA_LOG ("Prebook, turn off udp = 0x%016jx",fire_rx_tx_en);
  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);
  sleep(1);
}
/* ----------------------------------------------------- */

void enableHwUdp(EkaDev* dev) {
  sleep(1);
  uint64_t fire_rx_tx_en = eka_read(dev,ENABLE_PORT);
  fire_rx_tx_en |= 1ULL << (31); //[31] = 1, pass udp
  EKA_LOG ("Post book, turn on udp = 0x%016jx",fire_rx_tx_en);
  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);
}

/* ----------------------------------------------------- */

int createBooks(EkaStrat* strat,uint8_t coreId) {
  if (strat == NULL) on_error("strat == NULL");
  EKA_LOG("Creating books for %u products",strat->prodNum);
  for (uint i = 0; i < strat->prodNum; i++) {
    EKA_LOG("Trying prod %u,prodNum = %u",i,strat->prodNum); fflush(stderr);
    if (strat->prod[i] == NULL) on_error("strat->prod[%u] == NULL",i);

    if (! strat->prod[i]->is_book) continue;
    EKA_LOG("Prod %u is book, prodNum = %u",i,strat->prodNum); fflush(stderr);

    if (coreId != strat->prod[i]->mdCoreId) {
      EKA_LOG("skipping product %s (mdCoreId=%u) for core %u",
	      EKA_RESOLVE_PROD(strat->hwFeedVer,strat->prod[i]->product_id),
	      strat->prod[i]->mdCoreId,coreId);
      continue;
    }
    if (strcmp(EKA_RESOLVE_PROD(strat->hwFeedVer,strat->prod[i]->product_id),"---") == 0)
      on_error("Unsupported product id %u",(uint)strat->prod[i]->product_id);
    EKA_LOG("Creating Book for: %s on core %u",
	    EKA_RESOLVE_PROD(strat->hwFeedVer,strat->prod[i]->product_id),coreId);
    strat->createBook(coreId,strat->prod[i]);
  }

  return 0;
}

/* ----------------------------------------------------- */

int subscribeOnMc(EkaStrat* strat, uint8_t coreId) {
  if (strat == NULL) on_error("strat == NULL");

  for (uint i = 0; i < strat->udpSessions; i++) {

    if (strat->udpSess[i] == NULL) on_error("strat->udpSess[%u] == NULL",i);

    if (strat->dev->core[coreId] == NULL) on_error("strat->dev->core[%u] == NULL",coreId);

    if (coreId != strat->udpSess[i]->coreId) continue;

    uint32_t srcIp   = strat->dev->core[coreId]->srcIp;
    uint16_t port    = 0; // subscribing to all ports
    uint32_t mcastIp = strat->udpSess[i]->ip;
    uint16_t vlanTag = strat->udpSess[i]->vlanTag; // = 0

    EKA_LOG("Subscribing Udp sess %u on %s:<ALL_PORTS>",i,EKA_IP2STR(mcastIp));
    if (strat->udpChannel[coreId] == NULL)
      strat->udpChannel[coreId] = new EkaUdpChannel(strat->dev,coreId);

    if (strat->udpChannel[coreId] == NULL) on_error("strat->udpChannel[%u] == NULL",coreId);

    strat->udpChannel[coreId]->igmp_mc_join(srcIp,mcastIp,port,vlanTag);
  }
  return 0;
}

/* ----------------------------------------------------- */

static bool productsBelongTo (EkaStrat* strat, uint8_t coreId) {
  for (uint i = 0; i < strat->prodNum; i++) {
    if (strat->prod[i] == NULL) on_error("strat->prod[%u] == NULL",i);
    if (! strat->prod[i]->is_book) continue;

    if (coreId == strat->prod[i]->mdCoreId)
      return true;
  }

  return false;
}


/* ----------------------------------------------------- */
int ekaWriteTob(EkaDev* dev, void* params, uint paramsSize, eka_product_t product_id, eka_side_type_t side) {

  uint hw_product_index = dev->exch->fireLogic->findProdHw(product_id,__func__);

  if (hw_product_index>=EKA_MAX_BOOK_PRODUCTS)
    on_error ("SW TOB is supported only on book product (not on %u)\n",hw_product_index);

  uint64_t* wr_ptr = (uint64_t*) params;
  int iter = paramsSize/8 + !!(paramsSize%8); // num of 8Byte words
  for(int i = 0; i < iter; i++){
    eka_write(dev,0x70000+i*8,*(wr_ptr + i)); //data loop
    //    printf("config %d = %ju\n",i,*(wr_ptr + i));
  }
  union large_table_desc tob_desc = {};
  tob_desc.ltd.src_bank   = 0;
  tob_desc.ltd.src_thread = 0;
  tob_desc.ltd.target_idx = hw_product_index*2+(side==BUY ? 1 : 0);
  eka_write (dev,0xf0410,tob_desc.lt_desc); //desc
  return 0;
}


/* ----------------------------------------------------- */

void bookLoopThread_f(EkaStrat* strat) {
  EkaDev* dev = strat->dev;
  setThreadAffinityName(dev,pthread_self(),"EkalineBookLoop",dev->book_thread_affinity_core);
  //  uint64_t pktNum = 0;
  MdOut mdOut = {};
  mdOut.dev = dev;
  strat->threadActive = true;

  while (strat->threadActive) {
    for (uint coreId = 0; coreId < EkaDev::CONF::MAX_CORES; coreId++) {
      if (strat->udpChannel[coreId] == NULL) continue;
      if (! strat->udpChannel[coreId]->has_data()) continue;
      uint8_t* pkt = NULL;
      if ((pkt = (uint8_t*) strat->udpChannel[coreId]->get()) == NULL) on_error ("p == NULL");

      EkaIpHdr*   ipH     = (EkaIpHdr*) (pkt - 8 - 20);
      EkaUdpHdr*  udpH    = (EkaUdpHdr*)(pkt - 8);
      EkaUdpSess* udpSess = strat->findUdpSess(ipH->dest,be16toh((udpH->dest)));
      if (udpSess == NULL) {
#ifdef _EKA_PARSER_PRINT_ALL_
	EKA_LOG("%s:%u packet does not belog to our UDP sessions",EKA_IP2STR((ipH->dest)),be16toh(udpH->dest));
#endif
	strat->udpChannel[coreId]->next();
	continue;
      }
      uint payloadSize = strat->udpChannel[coreId]->getPayloadLen();
#ifdef _EKA_PARSER_PRINT_ALL_
      //    EKA_LOG("Pkt: %u byte ",payloadSize);
#endif
      pkt += strat->parser->processPkt(&mdOut,ProcessAction::UpdateBookOnly,pkt,payloadSize);
      if (! udpSess->isCorrectSeq(mdOut.pktSeq,mdOut.msgNum)) { /* on_error("Sequence Gap"); */ }

      strat->udpChannel[coreId]->next();
    }
  }
}

/* ----------------------------------------------------- */

/* void feedServerLoopThread_f(EkaDev* dev, uint8_t coreId) { */
/*   EKA_LOG("Running on coreId=%u",coreId); */
/*   if (dev->core[coreId] == NULL) on_error("dev->core[%u] == NULL",coreId); */
/*   if (dev->core[coreId]->udpChannel == NULL) on_error("dev->core[%u]->udpChannel == NULL",coreId); */

/*   setThreadAffinity(dev,coreId,dev->book_thread_affinity_core); */
/*   //  uint64_t pktNum = 0; */
/*   MdOut mdOut = {}; */
/*   mdOut.dev = dev; */
/*   dev->exch->feedServerThreadActive = true; */

/*   while (dev->exch->feedServerThreadActive) { */
/*     if (! dev->core[coreId]->udpChannel->has_data()) continue; */
/*     uint8_t* pkt = NULL; */
/*     if ((pkt = (uint8_t*)dev->core[coreId]->udpChannel->get()) == NULL) on_error ("p == NULL");  */

/*     EkaIpHdr*   ipH     = (EkaIpHdr*) (pkt - 8 - 20); */
/*     EkaUdpHdr*  udpH    = (EkaUdpHdr*)(pkt - 8); */
/*     EkaUdpSess* udpSess = dev->core[coreId]->findUdpSess(ipH->dest,be16toh((udpH->dest))); */
/*     if (udpSess == NULL) { */
/* #ifdef _EKA_PARSER_PRINT_ALL_ */
/*       EKA_LOG("%s:%u packet does not belog to our UDP sessions",EKA_IP2STR((ipH->dest)),be16toh(udpH->dest)); */
/* #endif */
/*       dev->core[coreId]->udpChannel->next(); */
/*       continue; */
/*     } */
/*     uint payloadSize = dev->core[coreId]->udpChannel->getPayloadLen(); */
/* #ifdef _EKA_PARSER_PRINT_ALL_ */
/*     //    EKA_LOG("Pkt: %u byte ",payloadSize); */
/* #endif */
/*     pkt += dev->exch->parser->processPkt(&mdOut,ProcessAction::UpdateBookOnly,pkt,payloadSize); */
/*     if (! udpSess->isCorrectSeq(mdOut.pktSeq,mdOut.msgNum)) { /\* on_error("Sequence Gap"); *\/ } */

/*     dev->core[coreId]->udpChannel->next(); */
/*   } */
/* } */

/* ----------------------------------------------------- */

void  runBook(EkaStrat* strat) {
  disableHwUdp(strat->dev);
  for (uint8_t coreId = 0; coreId < EkaDev::CONF::MAX_CORES; coreId++) {
    if (productsBelongTo(strat,coreId)) {
      if (strat->dev->core[coreId] == NULL) on_error("dev->core[%u] == NULL",coreId);
      createBooks(strat,coreId);
      subscribeOnMc(strat,coreId);
    }
  }
  strat->threadActive = false;
  strat->loopThread = std::thread(bookLoopThread_f,strat);
  strat->loopThread.detach();
  while (! strat->threadActive) usleep(1);
  enableHwUdp(strat->dev);
}

/* ----------------------------------------------------- */

#endif

#include <inttypes.h>

#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EkaCore.h"
#include "EkaSnDev.h"
#include "EkaUserChannel.h"

#include "eka_macros.h"

void ekaProcesTcpRx (EkaDev* dev, const uint8_t* pkt, uint32_t len);

/* ----------------------------------------------- */
static inline void sendDummyFastPathPkt(EkaDev* dev, const uint8_t* payload) {
  uint8_t vlan_size = /* dev->use_vlan ? 4 : */ 0;

  EkaIpHdr*  iph   = (EkaIpHdr*) (payload + sizeof(dma_report_t) + sizeof(EkaEthHdr) + vlan_size);
  EkaTcpHdr* tcph = (EkaTcpHdr*) ((uint8_t*)iph + sizeof(EkaIpHdr));

  uint8_t* data = (uint8_t*) tcph + sizeof(EkaTcpHdr);
  uint16_t len = be16toh(iph->_len) - sizeof(EkaIpHdr) - sizeof(EkaTcpHdr);
  
  //  hexDump("sendDummyFastPathPkt: Dummy Pkt to send",(void*)payload, 54);

  EkaTcpSess* tcpSess = dev->findTcpSess(iph->src, be16toh(tcph->src), iph->dest, be16toh(tcph->dest));
  if (tcpSess == NULL) 
    on_error("Tcp Session %s:%u --> %s:%u not found",
	     EKA_IP2STR((iph->src)),be16toh(tcph->src),EKA_IP2STR((iph->dest)),be16toh(tcph->dest));
  tcpSess->sendDummyPkt(data, len);

  return;
}
/* ----------------------------------------------- */

static inline void sendDummyFirePkt(EkaDev* dev, const uint8_t* payload) {
  //  if (dev->debug_no_tcpsocket == 1) return; //why is that?

  // PATCH
  uint fireSessId = 7 /* ((fire_report_t*)payload)->normalized_report.common_header.fire_session */;
  uint fireCoreId = 7 /* ((fire_report_t*)payload)->normalized_report.common_header.fire_ei */;

  if (dev->core[fireCoreId] == NULL) on_error("Wrong fireCoreId %u",fireCoreId);
  if (dev->core[fireCoreId]->tcpSess[fireSessId] == NULL) on_error("Wrong fireSessId %u",fireSessId);

  char dummyMsg[1024] = {};
  dev->core[fireCoreId]->tcpSess[fireSessId]->sendDummyPkt(dummyMsg, dev->hwRawFireSize);

  return;
}

/* ----------------------------------------------- */
static inline void sendDate2Hw(EkaDev* dev) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); 
  uint64_t current_time_ns = ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 + (uint64_t)(t.tv_nsec));
  current_time_ns += static_cast<uint64_t>(6*60*60) * static_cast<uint64_t>(1e9); //+6h UTC time
  int current_time_seconds = current_time_ns/(1000*1000*1000);
  time_t tmp = current_time_seconds;
  struct tm lt;
  (void) localtime_r(&tmp, &lt);
  char result[32] = {};
  strftime(result, sizeof(result), "%Y%m%d-%H:%M:%S.000", &lt); //20191206-20:17:32.131 
  uint64_t* wr_ptr = (uint64_t*) &result;
  for (int z=0; z<3; z++) {
    eka_write(dev,0xf0300+z*8,*wr_ptr++); //data
  }
  return;
}
/* ----------------------------------------------- */

static inline void sendHb2HW(EkaDev* dev) {
  eka_read(dev,0xf0f08);
}
/* ----------------------------------------------- */

void ekaServThread(EkaDev* dev) {
  EKA_LOG("Launching");
  uint32_t fire_counter = 0;
  //  uint32_t null_cnt = 0;
  dev->servThreadActive = true;
  pthread_t thread = pthread_self();
  pthread_setname_np(thread,"EkaServThread");

  while (dev->servThreadActive) {
    /* ----------------------------------------------- */

    if (dev->snDev->fastPath->hasData()) {
      const uint8_t* payload = dev->snDev->fastPath->get();
      uint len = dev->snDev->fastPath->getPayloadSize();

      /* hexDump("RX pkt",(void*)payload,len); */
      switch ((EkaUserChannel::DMA_TYPE)((dma_report_t*)payload)->type) {
      case EkaUserChannel::DMA_TYPE::FAST_PATH_DUMMY_PKT:
      case EkaUserChannel::DMA_TYPE::EPM:
	//	hexDump("FastPathPkt at ekaServThread",(uint8_t*)payload + sizeof(dma_report_t),len - sizeof(dma_report_t));

	sendDummyFastPathPkt(dev,payload);
	break;
      case EkaUserChannel::DMA_TYPE::FIRE:
	sendDummyFirePkt(dev,payload);
	fire_counter++;
	break;
      default:
	/* ekaProcesTcpRx (dev, payload, len); */
	hexDump("RX pkt at FAST_PATH User Channel",(void*)payload,len);
	on_error("Unexpected packet with DMA type %d",(int)((dma_report_t*)payload)->type);
      }
      dev->snDev->fastPath->next();
    }
    /* ----------------------------------------------- */
    if (! dev->servThreadActive) break;

    if (dev->snDev->tcpRx->hasData()) {
      const uint8_t* payload = dev->snDev->tcpRx->get();
      uint len = dev->snDev->tcpRx->getPayloadSize();

      /* hexDump("RX pkt",(void*)payload,len); */
    
      //      ekaProcesTcpRx (dev, payload, len - 4 /* FCS */);
      ekaProcesTcpRx (dev, payload, len);
      dev->snDev->tcpRx->next();
    }
    /* ----------------------------------------------- */
    if (! dev->servThreadActive) break;

    /* if (((null_cnt++)%3000000)==0) { */
    /*   sendDate2Hw(dev); */
    /*   sendHb2HW(dev); */
    /* } */
  }
  EKA_LOG("Terminated");
  return;
}

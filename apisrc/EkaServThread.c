#include <inttypes.h>

#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EkaCore.h"
#include "EkaSnDev.h"
#include "EkaUserChannel.h"
#include "EkaUdpChannel.h"
#include "EkaEpm.h"
#include "EkaUserReportQ.h"

#include "eka_macros.h"

void ekaProcesTcpRx (EkaDev* dev, const uint8_t* pkt, uint32_t len);

/* ----------------------------------------------- */
static inline int sendDummyFastPathPkt(EkaDev* dev, const uint8_t* payload) {
  uint8_t vlan_size = /* dev->use_vlan ? 4 : */ 0;

  EkaIpHdr*  iph   = (EkaIpHdr*) (payload + sizeof(EkaEthHdr) + vlan_size);
  EkaTcpHdr* tcph = (EkaTcpHdr*) ((uint8_t*)iph + sizeof(EkaIpHdr));

  uint8_t* data = (uint8_t*) tcph + sizeof(EkaTcpHdr);
  uint16_t len = be16toh(iph->_len) - sizeof(EkaIpHdr) - sizeof(EkaTcpHdr);
  
  if (len == 0 || len > 1536 - 54) on_error("len %u is wrong",len);
  
  //  hexDump("Dummy Pkt to send",(void*)data, len);

  EkaTcpSess* tcpSess = dev->findTcpSess(iph->src, be16toh(tcph->src), iph->dest, be16toh(tcph->dest));
  if (tcpSess == NULL) {
    hexDump("Dummy Pkt with unknown TcpSess",(void*)data, len);fflush (stdout);

    on_error("Tcp Session %s:%u --> %s:%u not found",
	     EKA_IP2STR((iph->src)),be16toh(tcph->src),EKA_IP2STR((iph->dest)),be16toh(tcph->dest)); 
  }

  return tcpSess->sendDummyPkt(data, len);
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
  uint32_t null_cnt = 0;

  dev->servThreadActive = true;

  uint64_t fire_rx_tx_en = eka_read(dev,ENABLE_PORT);
  fire_rx_tx_en &= ~(1ULL << 32); //turn on tcprx
  EKA_LOG ("Turning on tcprx = 0x%016jx",fire_rx_tx_en);
  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);
  dev->servThreadTerminated = false;

  while (dev->servThreadActive) {
    /* ----------------------------------------------- */

    if (dev->lwipPath->hasData()) {
      const uint8_t* payload = dev->lwipPath->get();
      uint len = dev->lwipPath->getPayloadSize();
      //      hexDump("Serv Thread Pkt",payload,len);

      EkaUserChannel::DMA_TYPE dmaType = (EkaUserChannel::DMA_TYPE)(*payload);

      switch (dmaType) {
	/* ----------------------------------------------- */
      case EkaUserChannel::DMA_TYPE::EPM : {
	feedback_dma_report_t* feedbackDmaReport = (feedback_dma_report_t*) payload;
	/* hexDump("Serv Thread EPM Pkt",payload,len); */
	/* EKA_LOG("EPM PKT: bitparams = 0x%0x, dummy_en = %d, expect_report = %d", */
	/* 	feedbackDmaReport->bitparams,feedbackDmaReport->bitparams.dummy_en,feedbackDmaReport->bitparams.expect_report); */


	if (feedbackDmaReport->bitparams.dummy_en == 1) {
	  if (sendDummyFastPathPkt(dev,payload) <= 0) break; // LWIP is busy
	}
	if (feedbackDmaReport->bitparams.expect_report == 1) {
	  EKA_LOG("User Report # %ju is pushed to Q",feedbackDmaReport->index);
	  dev->userReportQ->push(payload,len);
	}
	dev->lwipPath->next();
      }
	break;
	/* ----------------------------------------------- */

      case EkaUserChannel::DMA_TYPE::TCPRX : {
	//	tcprx_dma_report_t* tcprxDmaReport = (tcprx_dma_report_t*) payload;

	/* hexDump("ServThread TCPRX Pkt",payload,len); */

	uint8_t* data = (uint8_t*)payload + sizeof(tcprx_dma_report_t);
	ekaProcesTcpRx (dev, data, len - sizeof(tcprx_dma_report_t));
	dev->lwipPath->next();
      }
	break;
	/* ----------------------------------------------- */
      default:
	hexDump("Unexpected dmaType pkt",payload,64);
	on_error("Unexpected dmaType %d",(int)dmaType);
      }
    }
    /* ----------------------------------------------- */
    if ((null_cnt % 3000000)==0) {
      sendDate2Hw(dev);
      sendHb2HW(dev);
    }
    null_cnt++;
  }

  fire_rx_tx_en = eka_read(dev,ENABLE_PORT);
  fire_rx_tx_en |= (1ULL << 32); //turn off tcprx
  EKA_LOG ("Turning off tcprx = 0x%016jx",fire_rx_tx_en);
  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);

  dev->servThreadTerminated = true;

  EKA_LOG("Exiting");
  return;
}


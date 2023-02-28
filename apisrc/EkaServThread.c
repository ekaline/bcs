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
/* ----------------------------------------------- */

static inline int sendDummyFastPathPkt(EkaDev* dev, const uint8_t* payload) {
  uint8_t vlan_size = /* dev->use_vlan ? 4 : */ 0;

  EkaIpHdr*  iph   = (EkaIpHdr*) (payload + sizeof(EkaEthHdr) + vlan_size);
  EkaTcpHdr* tcph = (EkaTcpHdr*) ((uint8_t*)iph + sizeof(EkaIpHdr));

  uint8_t* data = (uint8_t*) tcph + sizeof(EkaTcpHdr);
  uint16_t len = be16toh(iph->_len) - sizeof(EkaIpHdr) - sizeof(EkaTcpHdr);
  
  if (len > EkaTcpSess::MAX_PAYLOAD_SIZE)
    on_error("TCP segment len %hu > TCP_MSS (%u)",len,EkaTcpSess::MAX_PAYLOAD_SIZE);
  
  //  hexDump("Dummy Pkt to send",(void*)data, len);

  EkaTcpSess* tcpSess = dev->findTcpSess(iph->src, be16toh(tcph->src), iph->dest, be16toh(tcph->dest));
  if (tcpSess == NULL) {
    hexDump("Dummy Pkt with unknown TcpSess",(void*)data, len);fflush (stdout);

    on_error("Tcp Session %s:%u --> %s:%u not found",
	     EKA_IP2STR((iph->src)),be16toh(tcph->src),EKA_IP2STR((iph->dest)),be16toh(tcph->dest)); 
  }

  return tcpSess->lwipDummyWrite(data, len);
}

/* ----------------------------------------------- */

void ekaServThread(EkaDev* dev) {
  const char* threadName = "ServThread";
  EKA_LOG("Launching %s",threadName);
  pthread_setname_np(pthread_self(),threadName);

  dev->servThreadTerminated = false;
  dev->servThreadActive = true;

  while (dev->servThreadActive) {
    if (! dev->epmFeedback->has_data())
      continue;

    const uint8_t* payload = dev->epmFeedback->get();
    uint len = dev->epmFeedback->getPayloadSize();
    //      hexDump("Serv Thread Pkt",payload,len);

    auto dmaType = (EkaUserChannel::DMA_TYPE)(*payload);

    if (dmaType != EkaUserChannel::DMA_TYPE::EPM)
      on_error("Unexpected dmaType %d",(int)dmaType);

    auto feedbackDmaReport = (const feedback_dma_report_t*) payload;
    /* EKA_LOG("EPM PKT: bitparams = 0x%0x, feedbck_en = %d, report_en = %d, type = %d, length = %d, index = %d, dmalen = %d", */
    /* 	feedbackDmaReport->bitparams,feedbackDmaReport->bitparams.feedbck_en,feedbackDmaReport->bitparams.report_en, */
    /* 	feedbackDmaReport->type,feedbackDmaReport->length,feedbackDmaReport->index,len); */
    /* hexDump("Serv Thread EPM Pkt",payload,len);fflush(stdout); */


    if (feedbackDmaReport->bitparams.bitmap.feedbck_en == 1) {
      int rc = sendDummyFastPathPkt(dev,payload);
      if (rc <= 0) {
	// LWIP is busy?
	char hexBuf[8192];
	if (std::FILE *const hexBufFile = fmemopen(hexBuf, sizeof hexBuf, "w")) {
	  hexDump("error TCP pkt",payload,len,hexBufFile);
	  (void)std::fwrite("\0", 1, 1, hexBufFile);
	  (void)std::fclose(hexBufFile);
	} else {
	  std::snprintf(hexBuf, sizeof hexBuf, "fmemopen error: %s (%d)",strerror(errno),errno);
	}
	EKA_WARN("sendDummyFastPathPkt returned error: rc=%d, \'%s\' (%d), pkt is:\n%s",
		 rc, strerror(errno), errno, hexBuf);

	dev->epmFeedback->next();
	break;
      }
    }
    if (feedbackDmaReport->bitparams.bitmap.report_en == 1) {
      /* EKA_LOG("User Report # %u is pushed to Q", */
      /* 	  feedbackDmaReport->index); */
      /* hexDump("Payload push to Q",payload,len); */
      dev->userReportQ->push(payload,len);
    }
    dev->epmFeedback->next();
  }

  dev->servThreadTerminated = true;

  EKA_LOG("Exiting");
  return;
}


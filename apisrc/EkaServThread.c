#include <inttypes.h>

#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpm.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpChannel.h"
#include "EkaUserChannel.h"
#include "EkaUserReportQ.h"
#include "ekaNW.h"

#include "eka_macros.h"
/* ----------------------------------------------- */

static inline int
sendDummyFastPathPkt(EkaDev *dev, const uint8_t *payload,
                     uint8_t originatedFromHw) {
  uint8_t vlan_size = /* dev->use_vlan ? 4 : */ 0;

  EkaIpHdr *iph =
      (EkaIpHdr *)(payload + sizeof(EkaEthHdr) + vlan_size);
  EkaTcpHdr *tcph =
      (EkaTcpHdr *)((uint8_t *)iph + sizeof(EkaIpHdr));

  uint8_t *data = (uint8_t *)tcph + sizeof(EkaTcpHdr);
  uint16_t len = be16toh(iph->_len) - sizeof(EkaIpHdr) -
                 sizeof(EkaTcpHdr);

  if (len > EkaTcpSess::MAX_PAYLOAD_SIZE) {
    char badPktStr[10000] = {};
    hexDump2str("Bad Pkt", payload, len, badPktStr,
                sizeof(badPktStr));
    EKA_WARN("TCP segment len %hu > TCP_MSS (%u) "
             "(originatedFromHw = %d): \n %s",
             len, EkaTcpSess::MAX_PAYLOAD_SIZE,
             originatedFromHw, badPktStr);
    on_error("TCP segment len %hu > TCP_MSS (%u)", len,
             EkaTcpSess::MAX_PAYLOAD_SIZE);
  }
  //  hexDump("Dummy Pkt to send",(void*)data, len);

  auto tcpSess =
      dev->findTcpSess(iph->src, be16toh(tcph->src),
                       iph->dest, be16toh(tcph->dest));
  if (!tcpSess) {
    hexDump("Dummy Pkt with unknown TcpSess", (void *)data,
            len);
    fflush(stdout);

    on_error("Tcp Session %s:%u --> %s:%u not found",
             EKA_IP2STR((iph->src)), be16toh(tcph->src),
             EKA_IP2STR((iph->dest)), be16toh(tcph->dest));
  }

  return tcpSess->lwipDummyWrite(data, len,
                                 originatedFromHw);
}

/* ----------------------------------------------- */

void ekaServThread(EkaDev *dev) {
  auto epm = dev->epm;
  if (!epm)
    on_error("!epm");

  const char *threadName = "ServThread";
  EKA_LOG("Launching %s", threadName);
  pthread_setname_np(pthread_self(), threadName);

  dev->servThreadTerminated = false;
  dev->servThreadActive = true;

  while (dev->servThreadActive) {
    if (!dev->epmFeedback->has_data()) {
      std::this_thread::yield();
      continue;
    }

    const uint8_t *payload = dev->epmFeedback->get();
    uint len = dev->epmFeedback->getPayloadSize();
    //      hexDump("Serv Thread Pkt",payload,len);

    auto dmaType = (EkaUserChannel::DMA_TYPE)(*payload);

    if (dmaType != EkaUserChannel::DMA_TYPE::EPM)
      on_error("Unexpected dmaType %d", (int)dmaType);

    auto feedbackDmaReport =
        (const feedback_dma_report_t *)payload;

    if (!feedbackDmaReport->bitparams.bitmap.feedbck_en)
      on_error("EPM feedback DMA channel got pkt with "
               "feedbck_en==0");

    int rc = 0;

    auto isHwFire = feedbackDmaReport->bitparams.bitmap
                        .originatedFromHw;

    if (isHwFire) {
      // dont send dummy pkt to lwip if it's fire at
      // report_only
      auto efc = dynamic_cast<EkaEfc *>(
          epm->strategy[EFC_STRATEGY]);
      if (!efc)
        on_error("!efc");
      if (efc->isReportOnly())
        goto SKIP_LWIP_DUMMY;
    }

    rc = sendDummyFastPathPkt(dev, payload, isHwFire);
    if (rc <= 0) {
      // LWIP is busy?
      char hexBuf[8192];
      if (std::FILE *const hexBufFile =
              fmemopen(hexBuf, sizeof hexBuf, "w")) {
        hexDump("error TCP pkt", payload, len, hexBufFile);
        (void)std::fwrite("\0", 1, 1, hexBufFile);
        (void)std::fclose(hexBufFile);
      } else {
        std::snprintf(hexBuf, sizeof hexBuf,
                      "fmemopen error: %s (%d)",
                      strerror(errno), errno);
      }
      EKA_WARN("sendDummyFastPathPkt returned error: "
               "rc=%d, \'%s\' (%d), pkt is:\n%s",
               rc, strerror(errno), errno, hexBuf);

      dev->epmFeedback->next();
      break;
    }

  SKIP_LWIP_DUMMY:
    if (feedbackDmaReport->bitparams.bitmap.report_en) {
      /* EKA_LOG("User Report # %u is pushed to Q", */
      /* 	  feedbackDmaReport->index); */
      /* hexDump("Payload push to Q",payload,len); */
      dev->userReportQ->push(payload, len);
    }
    dev->epmFeedback->next();
  }

  dev->servThreadTerminated = true;

  EKA_LOG("Exiting");
  return;
}

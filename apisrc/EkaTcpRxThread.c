#include <inttypes.h>

#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEpm.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpChannel.h"
#include "EkaUserChannel.h"
#include "EkaUserReportQ.h"
#include "ekaNW.h"

#include "eka_macros.h"

void ekaProcessTcpRx(EkaDev *dev, const uint8_t *pkt,
                     uint32_t len);

void ekaTcpRxThread(EkaDev *dev) {
  const char *threadName = "TcpRxThread";
  EKA_LOG("Launching %s", threadName);
  pthread_setname_np(pthread_self(), threadName);
  if (dev->affinityConf.tcpRxThreadCpuId >= 0) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(dev->affinityConf.tcpRxThreadCpuId, &cpuset);
    int rc = pthread_setaffinity_np(
        pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc < 0)
      on_error("Failed to set affinity");
    EKA_LOG("Affinity is set to CPU %d",
            dev->affinityConf.tcpRxThreadCpuId);
  }

  if (dev->tcpRxThreadActive)
    on_error("tcpRxThreadActive");

  uint64_t fire_rx_tx_en = eka_read(dev, ENABLE_PORT);
  EKA_LOG("Turning on tcprx = 0x%016jx", fire_rx_tx_en);
  fire_rx_tx_en &= ~(1ULL << 32); // turn on tcprx
  eka_write(dev, ENABLE_PORT, fire_rx_tx_en);

  dev->tcpRxThreadTerminated = false;
  dev->tcpRxThreadActive = true;

  while (dev->tcpRxThreadActive) {
    /* ----------------------------------------------- */

    if (!dev->lwipRx->has_data()) {
      std::this_thread::yield();
      continue;
    }

    const uint8_t *payload = dev->lwipRx->get();
    uint len = dev->lwipRx->getPayloadSize();

    /* hexDump("TcpRx Thread Pkt",payload,len); */

    auto dmaType = (EkaUserChannel::DMA_TYPE)(*payload);

    if (dmaType != EkaUserChannel::DMA_TYPE::LWIP)
      on_error("Unexpected dmaType %d", (int)dmaType);

    uint8_t *data =
        (uint8_t *)payload + sizeof(tcprx_dma_report_t);
    ekaProcessTcpRx(dev, data,
                    len - sizeof(tcprx_dma_report_t));
    dev->lwipRx->next();
  }

  fire_rx_tx_en = eka_read(dev, ENABLE_PORT);
  fire_rx_tx_en |= (1ULL << 32); // turn off tcprx
  EKA_LOG("Turning off tcprx = 0x%016jx", fire_rx_tx_en);
  eka_write(dev, ENABLE_PORT, fire_rx_tx_en);

  dev->tcpRxThreadTerminated = true;

  EKA_LOG("Exiting");
  return;
}

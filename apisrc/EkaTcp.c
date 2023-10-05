#include <cstdio>
#include <malloc.h>
#include <shared_mutex>
#include <stdlib.h>
#include <thread>

#include "eka_macros.h"

#include "lwip/api.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"

#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaTcp.h"
#include "EkaTcpSess.h"
#include "lwipopts.h"

#include "eka_hw_conf.h"

// Note: internal flags, must be kept in sync with
// `lwip/src/core/ipv4/etharp.c`
#define ETHARP_FLAG_TRY_HARD 1
#define ETHARP_FLAG_STATIC_ENTRY 4

extern EkaDev *g_ekaDev;

extern sys_mbox_t tcpip_mbox;
extern volatile bool tcpip_thread_terminated;
extern volatile bool ekaTerminateTcpThread;

void sys_mbox_free(sys_mbox_t *mbox);

static err_t eka_netif_init(struct netif *netif) {
  //  EkaDev* dev = ((struct
  //  LwipNetifState*)netif->state)->pEkaDev;
  //  EKA_LOG("Initializing");

  return ERR_OK;
}

extern "C" netif *
eka_route_ipv4_src(const ip4_addr_t *src,
                   const ip4_addr_t *dst) {
  if (!g_ekaDev) {
    std::fprintf(
        stderr,
        "error: lwIP source-based routing function called, "
        "but no open Ekaline devices exist\n");
    return nullptr;
  }
  auto dev = g_ekaDev;
  // Look through each initialized core, trying to find
  // the core whose source IP address matches the source IP
  // address of the packet we're trying to route. If we find
  // a match, tell lwIP to use that core's associated netif.
  // FIXME: need to acquire any locks here to protect cores
  // / core members?
  for (EkaCoreId c = 0; c < EkaDev::MAX_CORES; ++c) {
    if (dev->core[c] && dev->core[c]->srcIp == src->addr)
      return dev->core[c]->pLwipNetIf;
  }

  return nullptr;
}

static void ekaLwipInit(void *arg) {
  sys_sem_t *init_sem;
  LWIP_ASSERT("arg != NULL", arg != NULL);
  init_sem = (sys_sem_t *)arg;
  sys_sem_signal(init_sem);

  //  printf("%s: Im here\n",__func__);
}

err_t ekaLwipSendRaw(struct netif *netif, struct pbuf *p) {
  EkaDev *dev =
      ((struct LwipNetifState *)netif->state)->pEkaDev;
  EkaCoreId coreId =
      ((struct LwipNetifState *)netif->state)->lane;
  if (dev == NULL)
    return ERR_CLSD;

  //  dev->getControlTcpSess(coreId)->sendEthFrame(p->payload,p->len);
  EkaTcpSess *controlTcpSess =
      dev->core[coreId]->tcpSess[EkaCore::CONTROL_SESS_ID];
  controlTcpSess->sendEthFrame(p->payload, p->len);
  return ERR_OK;
}

err_t ekaLwipSend(struct netif *netif, struct pbuf *p) {
  /* hexDump("ekaLwipSend",p->payload,p->len); */
  EkaDev *dev =
      ((struct LwipNetifState *)netif->state)->pEkaDev;
  EkaCoreId coreId =
      ((struct LwipNetifState *)netif->state)->lane;
  if (dev == NULL)
    return ERR_CLSD;
  if (!dev->exc_active)
    return ERR_CLSD;
  if (dev->core[coreId] == NULL)
    on_error("dev->core[%u] == NULL", coreId);

  if (p == NULL)
    on_error("struct pbuf *p == NULL");
  if (p->len != p->tot_len)
    on_error("p->len (%u) != p->tot_len (%u): IP "
             "fragmentation not supported",
             p->len, p->tot_len);

  uint8_t *pkt = (uint8_t *)p->payload;

  if (EKA_IS_TCP_PKT(pkt)) {
    EkaTcpSess *tcpSess = dev->findTcpSess(
        EKA_IPH_SRC(pkt), EKA_TCPH_SRC(pkt),
        EKA_IPH_DST(pkt), EKA_TCPH_DST(pkt));
    if (tcpSess != NULL) {
      tcpSess->sendStackEthFrame(pkt, p->len);
      return ERR_OK;
    }
  }

  EkaTcpSess *controlTcpSess =
      dev->core[coreId]->tcpSess[EkaCore::CONTROL_SESS_ID];
  controlTcpSess->sendEthFrame(pkt, p->len);
  return ERR_OK;
}

void setNetifIpSrc(EkaDev *dev, EkaCoreId coreId,
                   const uint32_t *pSrcIp) {
  LOCK_TCPIP_CORE();
  netif_set_ipaddr(dev->core[coreId]->pLwipNetIf,
                   (const ip4_addr_t *)pSrcIp);
  UNLOCK_TCPIP_CORE();
  return;
}

void setNetifGw(EkaDev *dev, EkaCoreId coreId,
                const uint32_t *pGwIp) {
  LOCK_TCPIP_CORE();
  netif_set_gw(dev->core[coreId]->pLwipNetIf,
               (const ip4_addr_t *)pGwIp);
  UNLOCK_TCPIP_CORE();
  return;
}

void setNetifNetmask(EkaDev *dev, EkaCoreId coreId,
                     const uint32_t *pNetmaskIp) {
  LOCK_TCPIP_CORE();
  netif_set_netmask(dev->core[coreId]->pLwipNetIf,
                    (const ip4_addr_t *)pNetmaskIp);
  UNLOCK_TCPIP_CORE();
  return;
}

void setNetifIpMacSa(EkaDev *dev, EkaCoreId coreId,
                     const uint8_t *macSa) {
  memcpy(dev->core[coreId]->pLwipNetIf->hwaddr, macSa, 6);
  return;
}

int ekaAddArpEntry(EkaDev *dev, EkaCoreId coreId,
                   const uint32_t *protocolAddr,
                   const uint8_t *hwAddr) {
  netif *const nif = dev->core[coreId]->pLwipNetIf;
  LOCK_TCPIP_CORE();
  const err_t lwipErr = etharp_update_arp_entry(
      nif, (const ip4_addr_t *)protocolAddr,
      (struct eth_addr *)hwAddr,
      (u8_t)(ETHARP_FLAG_TRY_HARD |
             ETHARP_FLAG_STATIC_ENTRY));
  UNLOCK_TCPIP_CORE();
  if (lwipErr != ERR_OK) {
    errno = err_to_errno(lwipErr);
    return -1;
  }
  return 0;
}

struct netif *initLwipNetIf(EkaDev *dev, EkaCoreId coreId,
                            uint8_t *macSa,
                            uint32_t srcIp) {
  if (dev == NULL)
    on_error("dev == NULL");

  struct netif *netIf =
      (struct netif *)calloc(1, sizeof(struct netif));
  if (netIf == NULL)
    on_error("cannot allocate netIf");

  if ((netIf->state = calloc(1, sizeof(LwipNetifState))) ==
      NULL)
    on_error("cannot allocate LwipNetifState");

  ((struct LwipNetifState *)netIf->state)->pEkaDev = dev;
  ((struct LwipNetifState *)netIf->state)->lane = coreId;

  //  for (int i = 0; i < 6; ++i) netIf->hwaddr[i] =
  //  macSa[i];
  memcpy(netIf->hwaddr, macSa, 6);

  netIf->input = tcpip_input;
  netIf->output = etharp_output;
#ifdef RAW_LWIP_SEND
  netIf->linkoutput = ekaLwipSendRaw;
#else
  netIf->linkoutput = ekaLwipSend;
#endif
  netIf->mtu = 1500;
  netIf->hwaddr_len = 6;

  EKA_LOG("running netif_add with ip=%s, macSa=%s",
          EKA_IP2STR(srcIp), EKA_MAC2STR(macSa));

  LOCK_TCPIP_CORE();
  struct netif *netIf_rc =
      netif_add(netIf, (const ip4_addr_t *)&srcIp,
                NULL, // netmask
                NULL, // gw
                netIf->state, eka_netif_init, tcpip_input);

  if (netIf_rc == NULL)
    on_error("Failed to add netif on core[%u]", coreId);

  netIf->flags |= NETIF_FLAG_UP | NETIF_FLAG_BROADCAST |
                  NETIF_FLAG_LINK_UP | NETIF_FLAG_ETHARP |
                  NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;
  netIf->output = etharp_output;

  netif_set_up(netIf);
  netif_set_link_up(netIf);
  UNLOCK_TCPIP_CORE();

  EKA_LOG("NIC %u: netif->num = %d, I/F is %s, link is %s, "
          "MAC = %s",
          coreId, netIf->num,
          netif_is_up(netIf) ? "UP" : "DOWN",
          netif_is_link_up(netIf) ? "UP" : "DOWN",
          EKA_MAC2STR(netIf->hwaddr));

  return netIf_rc;
}

void ekaInitLwip(EkaDev *dev) {
  EKA_LOG("Initializing User Space TCP Stack");
  dev->exc_active = true;

  err_t err;
  err = sys_sem_new((sys_sem_t *)&dev->lwip_sem, 0);

  LWIP_ASSERT("failed to create init_sem", err == ERR_OK);

  LWIP_UNUSED_ARG(err);
  // for gtest

  tcpip_init(ekaLwipInit, (sys_sem_t *)&dev->lwip_sem);
  /* we have to wait for initialization to finish before */
  /* calling update_adapter()! */
  sys_sem_wait((sys_sem_t *)&dev->lwip_sem);
  sys_sem_free((sys_sem_t *)&dev->lwip_sem);

  netconn_thread_init();
  //  default_netif_poll();

#ifdef EXC_LAB_TEST
  dev->exc_debug = new exc_debug_module(dev, "EXCPKT");
#endif

  return;
}

extern pthread_t g_ekaTcpThreadHandle;
extern pthread_t lwip_core_lock_holder_thread_id;

struct sys_timeo **eka_get_next_timeout(void);

void deleteLwipTimers() {
  auto rootP = eka_get_next_timeout();

  for (auto t = *rootP; t != NULL;) {
    auto next = t->next;
    memp_free(MEMP_SYS_TIMEOUT, t);
    t = next;
  }

  *rootP = NULL;
}

extern struct tcp_pcb **const tcp_pcb_lists[];
extern const size_t n_tcp_pcb_lists;

void deleteLwipPCBs() {
  for (auto i = 0; i < n_tcp_pcb_lists; i++) {
    EKA_LOG("Deleting PCB list %d", i);
    for (auto p = *tcp_pcb_lists[i]; p != NULL;) {
      auto next = p->next;
      memp_free(MEMP_TCP_PCB, p);
      p = next;
    }
    *tcp_pcb_lists[i] = NULL;
  }
}

void ekaCloseLwip(EkaDev *dev) {
  EKA_LOG("Closing LWIP Thread");
  struct tcpip_msg *msg;
  msg = (struct tcpip_msg *)memp_malloc(MEMP_TCPIP_MSG_API);
  if (msg == NULL) {
    on_error("!msg");
  }
  msg->type = EKA_TERMINATE;
  sys_mbox_post(&tcpip_mbox, msg);
  // sys_sem_wait((sys_sem_t *)&dev->lwip_sem);
  // tcpip_mbox = 0;

  ekaTerminateTcpThread = true;
  // pthread_cancel(g_ekaTcpThreadHandle);

  EKA_LOG("Waiting for tcpip_thread_terminated");

  while (!tcpip_thread_terminated)
    sleep(0);
  sys_mbox_free(&tcpip_mbox);
  sys_mbox_set_invalid(&tcpip_mbox);
  UNLOCK_TCPIP_CORE();
  lwip_core_lock_holder_thread_id = 0;

  deleteLwipTimers();
  deleteLwipPCBs();

  // netconn_thread_cleanup();
  // sys_sem_free((sys_sem_t *)&dev->lwip_sem);

  EKA_LOG("LWIP is released");
}

void ekaPushLwipRxPkt(EkaDev *dev, EkaCoreId rxCoreId,
                      const void *pkt, uint32_t len) {
  if (!dev->core[rxCoreId] ||
      !dev->core[rxCoreId]->pLwipNetIf)
    return;

  struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
  if (!p)
    on_error("failed to get new PBUF");
  memcpy(p->payload, pkt, len);

  struct netif *netIf = dev->core[rxCoreId]->pLwipNetIf;

  if (netIf->input(p, netIf) != ERR_OK) {
    char hexBuf[8192];
    hexDump2str("Unexpected RX TCP pkt", pkt, len, hexBuf,
                sizeof(hexBuf));
    EKA_WARN("Dropping RX pkt \n%s", hexBuf);
    pbuf_free(p);
  }
#if DEBUG_PRINTS
  hexDump("Pushed RX pkt to LWIP", pkt, len);
#endif

  return;
}

void ekaProcessTcpRx(EkaDev *dev, const uint8_t *pkt,
                     uint32_t len) {
#if DEBUG_PRINTS
  hexDump("ekaProcessTcpRx RX pkt", pkt, len);
#endif

  uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF,
                             0xFF, 0xFF, 0xFF};
  if (memcmp(pkt, broadcastMac, 6) == 0) { // broadcast
    for (EkaCoreId rxCoreId = 0;
         rxCoreId < EkaDev::MAX_CORES; rxCoreId++) {
      ekaPushLwipRxPkt(dev, rxCoreId, pkt, len);
    }
    return;
  }

  EkaCoreId rxCoreId = dev->findCoreByMacSa(pkt);
  if (rxCoreId < 0) {
    char hexBuf[8192];
    hexDump2str("Ignored RX TCP pkt", pkt, len, hexBuf,
                sizeof(hexBuf));
    TEST_LOG("Ignored RX pkt:\n%s", hexBuf);
    return; // ignoring "not mine" pkt
  }
  if (EKA_IS_TCP_PKT(pkt)) {
    EkaTcpSess *tcpSess = dev->findTcpSess(
        EKA_IPH_DST(pkt), EKA_TCPH_DST(pkt),
        EKA_IPH_SRC(pkt), EKA_TCPH_SRC(pkt));
    if (!tcpSess) {
      if (dev->core[rxCoreId]->connectedL1Switch)
        // dont print warning at Metamux case
        return;

      // TCP packet not corresponding to any session we know
      // about. This could be a retransmission of a
      // connection that once existed, or any number of
      // improper-TCP-close errors that cause us to receive
      // a packet not meant for us. Warn and ignore.
      char hexBuf[8192];
      hexDump2str("Unexpected RX TCP pkt", pkt, len, hexBuf,
                  sizeof(hexBuf));
      EKA_WARN(
          "RX pkt TCP session %s:%u-->%s:%u not found, "
          "pkt is:\n%s",
          EKA_IP2STR(EKA_IPH_DST(pkt)), EKA_TCPH_DST(pkt),
          EKA_IP2STR(EKA_IPH_SRC(pkt)), EKA_TCPH_SRC(pkt),
          hexBuf);
      return;
    } // unexpected TCP pkt

    tcpSess->updateRx(pkt, len);
  }

  ekaPushLwipRxPkt(dev, rxCoreId, pkt, len);
}

#include <stdlib.h>
#include <thread>
#include <malloc.h>

#include "eka_macros.h"

#include "lwip/sockets.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/api.h"

#include "lwipopts.h"

#include "EkaDev.h"
#include "EkaTcp.h"
#include "EkaTcpSess.h"
#include "EkaCore.h"

#include "eka_hw_conf.h"


static err_t eka_netif_init (struct netif *netif) {
  //  EkaDev* dev = ((struct LwipNetifState*)netif->state)->pEkaDev;
  //  EKA_LOG("Initializing");

  return ERR_OK;
}

static void ekaLwipInit (void * arg) {
  sys_sem_t *init_sem;
  LWIP_ASSERT("arg != NULL", arg != NULL);
  init_sem = (sys_sem_t*)arg;
  sys_sem_signal(init_sem);

  //  printf("%s: Im here\n",__func__);
}

err_t ekaLwipSendRaw(struct netif *netif, struct pbuf *p) {
  EkaDev* dev    = ((struct LwipNetifState*)netif->state)->pEkaDev;
  EkaCoreId coreId = ((struct LwipNetifState*)netif->state)->lane;
  if (dev == NULL) return ERR_CLSD;

  //  dev->getControlTcpSess(coreId)->sendFullPkt(p->payload,p->len);
  EkaTcpSess* controlTcpSess = dev->core[coreId]->tcpSess[EkaCore::CONTROL_SESS_ID];
  controlTcpSess->sendFullPkt(p->payload,p->len);
  return ERR_OK;
}

err_t ekaLwipSend(struct netif *netif, struct pbuf *p) {
  /* hexDump("ekaLwipSend",p->payload,p->len); */
  EkaDev* dev    = ((struct LwipNetifState*)netif->state)->pEkaDev;
  EkaCoreId coreId = ((struct LwipNetifState*)netif->state)->lane;
  if (dev == NULL) return ERR_CLSD;
  if (dev->core[coreId] == NULL) on_error("dev->core[%u] == NULL",coreId);

  if (p == NULL) on_error("struct pbuf *p == NULL");
  if (p->len != p->tot_len) 
    on_error("p->len (%u) != p->tot_len (%u): IP fragmentation not supported",p->len,p->tot_len);
  if (p->len < 1) on_error("p->len < 1");//return ERR_OK;

  uint8_t* pkt = (uint8_t*) p->payload;

  if (EKA_IS_TCP_PKT(pkt)) {
    EkaTcpSess* tcpSess = dev->findTcpSess(EKA_IPH_SRC(pkt),
					   EKA_TCPH_SRC(pkt),
					   EKA_IPH_DST(pkt),
					   EKA_TCPH_DST(pkt));
    if (tcpSess != NULL) {
      tcpSess->sendStackPkt(pkt,p->len);
      return ERR_OK;
    }
  }

  EkaTcpSess* controlTcpSess = dev->core[coreId]->tcpSess[EkaCore::CONTROL_SESS_ID];
  controlTcpSess->sendFullPkt(pkt,p->len);
  return ERR_OK;
}

void setNetifIpSrc(EkaDev* dev, EkaCoreId coreId, const uint32_t* pSrcIp) {
  LOCK_TCPIP_CORE();
  netif_set_ipaddr(dev->core[coreId]->pLwipNetIf,(const ip4_addr_t*)pSrcIp); 
  UNLOCK_TCPIP_CORE();
  return;
}

void setNetifGw(EkaDev* dev, EkaCoreId coreId, const uint32_t* pGwIp) {
  LOCK_TCPIP_CORE();
  netif_set_gw (dev->core[coreId]->pLwipNetIf,(const ip4_addr_t*)pGwIp); 
  UNLOCK_TCPIP_CORE();
  return;
}

void setNetifNetmask(EkaDev* dev, EkaCoreId coreId, const uint32_t* pNetmaskIp) {
  LOCK_TCPIP_CORE();
  netif_set_netmask (dev->core[coreId]->pLwipNetIf,(const ip4_addr_t*)pNetmaskIp); 
  UNLOCK_TCPIP_CORE();
  return;
}

void setNetifIpMacSa(EkaDev* dev, EkaCoreId coreId, const uint8_t* macSa) {
  memcpy(dev->core[coreId]->pLwipNetIf->hwaddr,macSa,6);
  return;
}

struct netif* initLwipNetIf(EkaDev* dev, EkaCoreId coreId, uint8_t* macSa, uint8_t* macDa, uint32_t srcIp) {
  if (dev == NULL) on_error("dev == NULL");

  struct netif* netIf = (struct netif*) calloc(1,sizeof(struct netif));
  if (netIf == NULL) on_error("cannot allocate netIf");

  if ((netIf->state = calloc(1,sizeof(LwipNetifState))) == NULL) 
    on_error("cannot allocate LwipNetifState");

  ((struct LwipNetifState*)netIf->state)->pEkaDev = dev;
  ((struct LwipNetifState*)netIf->state)->lane = coreId;

  //  for (int i = 0; i < 6; ++i) netIf->hwaddr[i] = macSa[i];
  memcpy(netIf->hwaddr,macSa,6);

  netIf->input = tcpip_input;
  netIf->output = etharp_output;
#ifdef RAW_LWIP_SEND
  netIf->linkoutput = ekaLwipSendRaw;
#else
  netIf->linkoutput = ekaLwipSend;
#endif
  netIf->mtu = 1500;
  netIf->hwaddr_len = 6;

  EKA_LOG("running netif_add with ip=%s, macSa=%s, macDa=%s",
	  EKA_IP2STR(srcIp),EKA_MAC2STR(macSa),EKA_MAC2STR(macDa));

  LOCK_TCPIP_CORE();
  struct netif* netIf_rc = netif_add(netIf,
				     (const ip4_addr_t *) & srcIp,
				     NULL, // netmask
				     NULL, // gw
				     netIf->state,
				     eka_netif_init,
				     tcpip_input);

  if (netIf_rc == NULL) 
    on_error("Failed to add netif on core[%u]",coreId);

  netIf->flags   |= 
    NETIF_FLAG_UP        | 
    NETIF_FLAG_BROADCAST | 
    NETIF_FLAG_LINK_UP   | 
    NETIF_FLAG_ETHARP    | 
    NETIF_FLAG_ETHERNET  | 
    NETIF_FLAG_IGMP;
  netIf->output = etharp_output;

  //  netif_set_default (netIf);

  netif_set_up(netIf);
  netif_set_link_up(netIf);
  UNLOCK_TCPIP_CORE();

  EKA_LOG("NIC %u: netif->num = %d, I/F is %s, link is %s, MAC = %s",
	  coreId, netIf->num, netif_is_up(netIf) ? "UP" : "DOWN",netif_is_link_up(netIf)  ? "UP" : "DOWN",
	  EKA_MAC2STR(netIf->hwaddr));

  return netIf_rc;
}

void ekaInitLwip (EkaDev* dev) {
  EKA_LOG("Initializing User Space TCP Stack");
  dev->exc_active = true;

  err_t err;
  err = sys_sem_new((sys_sem_t*)&dev->lwip_sem, 0);

  LWIP_ASSERT("failed to create init_sem", err == ERR_OK);

  LWIP_UNUSED_ARG(err);
  tcpip_init(ekaLwipInit,(sys_sem_t*)&dev->lwip_sem);
  /* we have to wait for initialization to finish before */
  /* calling update_adapter()! */
  sys_sem_wait((sys_sem_t*)&dev->lwip_sem);
  sys_sem_free((sys_sem_t*)&dev->lwip_sem);

  netconn_thread_init();
  //  default_netif_poll();

#ifdef EXC_LAB_TEST
  dev->exc_debug = new exc_debug_module(dev,"EXCPKT");
#endif

  return;
}

void ekaProcessTcpRx (EkaDev* dev, const uint8_t* pkt, uint32_t len) {

  uint8_t broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  if (memcmp(pkt,broadcastMac,6) == 0) { // broadcast 
    for (EkaCoreId rxCoreId = 0; rxCoreId < EkaDev::MAX_CORES; rxCoreId++) {
      if (dev->core[rxCoreId] != NULL && dev->core[rxCoreId]->pLwipNetIf != NULL) {

	struct pbuf* p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
	if (p == NULL) on_error ("failed to get new PBUF");
	memcpy(p->payload,pkt,len);

	struct netif* netIf = dev->core[rxCoreId]->pLwipNetIf;

	if (netIf->input(p,netIf) != ERR_OK) {
	  hexDump("Dropping RX pkt",(void*)pkt,len);
	  pbuf_free(p);
	}
      }
    }
  } else {
    EkaCoreId rxCoreId = dev->findCoreByMacSa(pkt);
    if (rxCoreId < 0) return 0; // ignoring "not mine" pkt

    if (dev->core[rxCoreId] != NULL && dev->core[rxCoreId]->pLwipNetIf != NULL) {
      struct netif* netIf = dev->core[rxCoreId]->pLwipNetIf;
	
      if (EKA_IS_TCP_PKT(pkt)) {
        EkaTcpSess* tcpSess = dev->findTcpSess(EKA_IPH_DST(pkt),EKA_TCPH_DST(pkt),EKA_IPH_SRC(pkt),EKA_TCPH_SRC(pkt));
        if (!tcpSess) {
          // TCP packet not corresponding to any session we know about. This could
          // be a retransmission of a connection that once existed, or any number
          // of improper-TCP-close errors that cause us to receive a packet not
          // meant for us. Warn and ignore.
          char hexBuf[8192]; // approximate MTU * 4 bytes to hold the hexdump
          hexBuf[0] = '\0';  // In case something goes wrong.
          if (std::FILE *const hexBufFile = fmemopen(hexBuf, sizeof hexBuf, "w")) {
            hexDump("Unexpected RX TCP pkt",pkt,len,hexBufFile);
            (void)std::fwrite("\0", 1, 1, hexBufFile);
            (void)std::fclose(hexBufFile);
          }
          EKA_WARN("RX pkt TCP session %s:%u-->%s:%u not found,pkt is:\n%s",
                   EKA_IP2STR(EKA_IPH_DST(pkt)),EKA_TCPH_DST(pkt),
                   EKA_IP2STR(EKA_IPH_SRC(pkt)),EKA_TCPH_SRC(pkt),
                   hexBuf);
          return;
        }

        tcpSess->updateRx(pkt,len);
      }

      struct pbuf* p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
      if (p == NULL) on_error ("failed to get new PBUF");
      memcpy(p->payload,pkt,len);

      if (netIf->input(p,netIf) != ERR_OK) 
	pbuf_free(p);
    }
  }
  /* hexDump("ekaProcesTcpRx",(void*)pkt,len); */
}



#ifndef _EKA_SN_DEV_H_
#define _EKA_SN_DEV_H_

// SHURIK

#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include "smartnic.h"
#include "eka_macros.h"
#include "ctls.h"
#include "eka.h"
#include "EkaUserChannel.h"

#include "eka_sn_addr_space.h"

class EkaDev;

#ifndef SC_IOCTL_EKALINE_DATA
#define SC_IOCTL_EKALINE_DATA SMARTNIC_EKALINE_DATA
#define SC_NO_NETWORK_INTERFACES SN_NO_NETWORK_LANES
#endif

class EkaSnDev {
 public:
  EkaSnDev(EkaDev* _dev) {
    dev = _dev;
    if (dev == NULL) on_error("dev == NULL");
    if ((dev_id = SN_OpenDevice(NULL, NULL)) == NULL) on_error("Cannot open Smartnic Device");
    //    reset();
    write(SW_STATISTICS,(uint64_t) 0); // Clearing SW Statistics
    write(P4_STRAT_CONF,(uint64_t) 0); // Clearing Strategy params
    write(P4_FATAL_DEBUG,(uint64_t) 0); // Clearing Fatal Debug
    write(ENABLE_PORT,1ULL << (24)); // No VLAN no ports, udp killed

    dev->snDevUserLogicRegistersPtr = SC_GetUserLogicRegistersBase(dev_id);
  }

//################################################
  ~EkaSnDev() {
    EKA_LOG("Closing Smartnic device");
    /* if (epmReport != NULL) delete epmReport; */
    /* if (lwipPath   != NULL) delete lwipPath; */
    EKA_LOG("SN_CloseDevice"); fflush(stderr);
    SN_CloseDevice(dev_id);
  }
//################################################

  inline uint64_t read(uint64_t addr) {
    uint64_t ret;
    if (SN_ERR_SUCCESS != SN_ReadUserLogicRegister(dev_id, addr/8, &ret))
      on_error("SN_Read returned smartnic error code : %d",SN_GetLastErrorCode());
    return ret;
  }
//################################################

  inline void write(uint64_t addr, uint64_t val) {
    if (SN_ERR_SUCCESS != SN_WriteUserLogicRegister(dev_id, addr/8, val))
      on_error("SN_Write returned smartnic error code : %d",SN_GetLastErrorCode());
  }
//################################################

  void reset() {
    EKA_LOG("Resetting the FPGA");
    if (SN_ResetFPGA(dev_id) != SN_ERR_SUCCESS ) on_error("Failed to reset FPGA");    
  }
//################################################

  bool       getLinkStatus(uint8_t lane, bool* sfp_present, bool* link) {
    SN_StatusInfo statusInfo;
    int rc = SN_GetStatusInfo(dev_id, &statusInfo);
    if (rc != SN_ERR_SUCCESS) on_error("Failed to get status info, error code %d",rc);
    SN_LinkStatus *s = (SN_LinkStatus *) &statusInfo.LinkStatus[lane];
    bool sfp = s->SFP_Present ? true : false;
    bool l = s->Link ? true : false;
    if (sfp_present != NULL) *sfp_present = sfp;
    if (link != NULL)  *link = l;
    EKA_LOG("Core %u: sfp = %d, link = %d",lane,sfp,l);
    return sfp && l;
  }
//################################################

  bool       hasLink(uint8_t lane) {
    SN_StatusInfo statusInfo;
    int rc = SN_GetStatusInfo(dev_id, &statusInfo);
    if (rc != SN_ERR_SUCCESS) on_error("Failed to get status info, error code %d",rc);
    SN_LinkStatus *s = (SN_LinkStatus *) &statusInfo.LinkStatus[lane];
    EKA_LOG("Core %u: sfp = %d, link = %d",lane,s->SFP_Present, s->Link);

    return s->SFP_Present && s->Link;
  }
//################################################

  void       getIpMac(uint8_t lane, uint32_t* ip, uint8_t* mac) {
    if (SC_GetLocalAddress(dev_id,lane, (struct in_addr*) ip, NULL, NULL, mac) != SC_ERR_SUCCESS) on_error("Error on SC_GetLocalAddress");
    in_addr ip2set = {*ip};
    in_addr netmask2set = {};
    in_addr gw2set = {};

    if (SC_SetLocalAddress(dev_id,lane, ip2set, netmask2set, gw2set, mac) != SC_ERR_SUCCESS) 
      EKA_WARN("WARNING: Failed on SC_SetLocalAddress");
    /* else */
    /*   EKA_LOG ("IP is set to %s",EKA_IP2STR(*ip)); */
    return;
  }
//################################################
  void       getIpMac_ioctl(uint8_t lane, uint32_t* ip, uint8_t* mac) {
#if 1
  char          buf[1024];
  struct ifconf ifc;
  struct ifreq *ifr;
  int           sck, nInterfaces;

  std::string portName = std::string("feth") + std::to_string(lane);
  //  EKA_LOG("testing %s, lane = %u",portName.c_str(),lane);

  /* Get a socket handle. */
  sck = socket(AF_INET, SOCK_DGRAM, 0);
  if(sck < 0) on_error ("%s: failed on socket(AF_INET, SOCK_DGRAM, 0) -> ",__func__);

/* Query available interfaces. */
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;
  if(ioctl(sck, SIOCGIFCONF, &ifc) < 0) on_error ("%s: failed on ioctl(sck, SIOCGIFCONF, &ifc)  -> ",__func__);

/* Iterate through the list of interfaces. */
  ifr = ifc.ifc_req;
  nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

  for(int i = 0; i < nInterfaces; i++){
    struct ifreq *item = &ifr[i];
    if (strncmp(item->ifr_name,portName.c_str(),strlen(portName.c_str())) != 0) continue;

    *ip = ((struct sockaddr_in *)&item->ifr_addr)->sin_addr.s_addr;
/* extracting the pre-configured MACSA  */
    struct ifreq s;
    strcpy(s.ifr_name,item->ifr_name);

    if (ioctl(sck, SIOCGIFHWADDR, &s) != 0) on_error("%s: ioctl(fd, SIOCGIFHWADDR, &s) != 0) -> ",__func__);
    memcpy(mac,s.ifr_addr.sa_data,6);
    EKA_LOG("%s: %s %s", item->ifr_name,EKA_IP2STR(*ip),EKA_MAC2STR(mac));

  }
  close (sck);


#else
    in_addr gw, nm;
    if (SN_GetLocalAddress(dev_id,lane, (in_addr*) ip, &gw, &nm, mac) != SN_ERR_SUCCESS) on_error("Error on SN_GetLocalAddress");
    if (*(uint32_t*)ip == 0) on_error("ip = 0");
#endif
    return;
  }
//################################################

  void       set_fast_session (uint8_t core, uint8_t sess, uint32_t srcIp, uint16_t srcUdp, uint32_t dstIp, uint16_t dstUdp) {
    EKA_LOG("core=%u, sess=%u %s:%hu --> %s:%hu",core,sess,EKA_IP2STR(srcIp),srcUdp,EKA_IP2STR(dstIp),dstUdp);

    int fd = SN_GetFileDescriptor(dev_id);
    eka_ioctl_t state = {};
    state.cmd = EKA_SET;
    state.paramA = core;
    state.paramB = sess;
    state.eka_session.active = 1;
    state.eka_session.ip_saddr  = srcIp;
    state.eka_session.ip_daddr  = dstIp;
    state.eka_session.tcp_sport = srcUdp;
    state.eka_session.tcp_dport = dstUdp;

    int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state);
    if (rc < 0) on_error("error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_SET");
    EKA_LOG("Fast Session set: core=%u, sess=%u, %s:%u --> %s:%u",
	    core,sess,EKA_IP2STR(srcIp),srcUdp,EKA_IP2STR(dstIp),dstUdp);

    return;
  }
  //  void       ioremap_wc_tx_pkts ();
//################################################

  // uint16_t getWindowSize (uint8_t core, uint8_t sess) {
  //   int fd = SN_GetFileDescriptor(dev_id);
  //   eka_ioctl_t state = {};
  //   state.cmd = EKA_DUMP;
  //   state.nif_num = core;
  //   state.session_num = sess;
  //   int rc = ioctl(fd,SMARTNIC_EKALINE_DATA,&state);
  //   if (rc < 0) on_error("ioctl(fd,SMARTNIC_EKALINE_DATA,&state) EKA_DUMP");
  //   return state.eka_session.tcp_window;
  // }
  
  //################################################

  // void update_fast_session_params (uint8_t coreId, uint8_t sessId, uint8_t* macSa, uint8_t* macDa) {
    // int fd = SN_GetFileDescriptor(dev_id);
    // eka_ioctl_t state = {};
    // state.cmd = EKA_DUMP;
    // state.nif_num = coreId;
    // state.session_num = sessId;

    // int rc = ioctl(fd,SMARTNIC_EKALINE_DATA,&state);

    // if (rc < 0) on_error("error ioctl(fd,SMARTNIC_EKALINE_DATA,&state) EKA_DUMP -> ");

    // memcpy(macSa,state.eka_session.macsa,6);
    // memcpy(macDa,state.eka_session.macda,6);
  // }

  /* EkaUserChannel*           epmReport = NULL; */
  /* EkaUserChannel*           lwipPath = NULL; */

  SN_DeviceId dev_id;

 private:
  enum CONF {
    MAX_CORES   = EKA_MAX_CORES, //8,
    CHANNELS    = 32
  };

  EkaDev*     dev;

};
//################################################

/* void  ioremap_wc_tx_pkts() { */
/* #ifdef EKA_WC */
/*   int fd = SC_GetFileDescriptor(dev_id); */
/*   eka_ioctl_t state = {}; */
/*   state.cmd = EKA_IOREMAP_WC; */
/*   state.nif_num = 0; */
/*   int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state); */
/*   if (rc < 0) on_error("error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_IOREMAP_WC"); */

/*   uint64_t a2wr_from_ioctl = state.wcattr.bar0_wc_va; */

/*   txPktBuf = (volatile uint8_t*) EkalineGetWcBase(dev_id); */

/*   if (a2wr_from_ioctl != (uint64_t)txPktBuf)  */
/*     on_error("txPktBuf from ioctl (%jx ) != EkalineGetWcBase (%jx)",a2wr_from_ioctl,(uint64_t)txPktBuf); */

/*   EKA_LOG("WC txPktBuf = %p",txPktBuf); */
/* #else */
/*   EKA_LOG("WC IS NOT IMPLEMENTED IN THIS RELEASE"); */
/* #endif */
/*   return; */
/* } */

#endif

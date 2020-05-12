#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "eka_sn_dev.h"
#include "eka_data_structs.h"
#include "eka_dev.h"


void eka_open_sn_channels (EkaDev* dev,SN_DeviceId devId);
//################################################

void set_fast_session(EkaDev* dev, uint8_t core,uint8_t sess) {
  EKA_LOG("Setting Fast Session for core = %u, sess = %u",core,sess);
  fflush(stderr);
  dev->sn_dev->set_fast_session(core,sess);
}

//################################################
eka_sn_dev::eka_sn_dev (EkaDev* d) {
  if ((dev_id = SC_OpenDevice(NULL, NULL)) == NULL) on_error("Cannot open Smartnic Device");
  dev = d;

  write(SW_STATISTICS,(uint64_t) 0); // Clearing SW Statistics
  write(P4_STRAT_CONF,(uint64_t) 0); // Clearing Strategy params
  write(P4_FATAL_DEBUG,(uint64_t) 0); // Clearing Fatal Debug
#ifdef EKA_WC
  ioremap_wc_tx_pkts();
#endif
  //  if (dev->exc_inited) {
  if (1) {
    EKA_LOG("Openning USER CHANNELS");
    eka_open_sn_channels(dev,dev_id);
  }
}
//################################################

eka_sn_dev::~eka_sn_dev () {
  //  EKA_LOG("Closing Smartnic device");
  SC_CloseDevice(dev_id);
  //  EKA_LOG("Smartnic device closed");
}
//################################################

void eka_sn_dev::reset() {
  EKA_LOG("Resetting the FPGA");
  if (SC_ResetFPGA(dev_id) != SC_ERR_SUCCESS ) on_error("Failed to resed FPGA");

  }
//################################################

uint64_t eka_sn_dev::read(uint64_t addr) {
  uint64_t ret;
  if (SC_ERR_SUCCESS != SC_ReadUserLogicRegister(dev_id, addr/8, &ret))
    on_error("SC_Read returned smartnic error code : %d",SC_GetLastErrorCode());
  return ret;
}
//################################################

void eka_sn_dev::write(uint64_t addr, uint64_t val) {
  if (SC_ERR_SUCCESS != SC_WriteUserLogicRegister(dev_id, addr/8, val))
    on_error("SC_Write returned smartnic error code : %d",SC_GetLastErrorCode());
}
//################################################

bool eka_sn_dev::get_link_status(uint8_t lane, bool* sfp_present, bool* link) {
  SC_StatusInfo statusInfo;
  int rc = SC_GetStatusInfo(dev_id, &statusInfo);
  if (rc != SC_ERR_SUCCESS) on_error("Failed to get status info, error code %d",rc);
  SC_LinkStatus *s = (SC_LinkStatus *) &statusInfo.LinkStatus[lane];
  bool sfp = s->SFP_Present ? true : false;
  bool l = s->Link ? true : false;
  if (sfp_present != NULL) *sfp_present = sfp;
  if (link != NULL)  *link = l;
  return sfp && l;
}
//################################################

void eka_sn_dev::get_ip_mac(uint8_t lane, uint32_t* ip, uint8_t* mac) {
  if (SC_GetLocalAddress(dev_id,lane, (struct in_addr*) ip, NULL, NULL, mac) != SC_ERR_SUCCESS) on_error("Error on SC_GetLocalAddress");
#if 1
  in_addr ip2set = {*ip};
  in_addr netmask2set = {};
  in_addr gw2set = {};

  if (SC_SetLocalAddress(dev_id,lane, ip2set, netmask2set, gw2set, mac) != SC_ERR_SUCCESS) 
    EKA_WARN("WARNING: Failed on SC_SetLocalAddress");
  else
    EKA_LOG ("IP is set to %s",EKA_IP2STR(*ip));
#endif

  return;
}
//################################################

void  eka_sn_dev::set_fast_session (uint8_t core, uint8_t sess) {
  EKA_LOG("core=%u, sess=%u %s:%hu --> %s:%hu",core,sess,
	  EKA_IP2STR(dev->core[core].src_ip),dev->core[core].tcp_sess[sess].src_port,
	  EKA_IP2STR(dev->core[core].tcp_sess[sess].dst_ip),dev->core[core].tcp_sess[sess].dst_port);

  int fd = SC_GetFileDescriptor(dev_id);
  eka_ioctl_t state = {};
  state.cmd = EKA_SET;
  state.nif_num = core;
  state.session_num = sess;
  state.eka_session.active = 1;
  state.eka_session.drop_next_real_tx_pckt = 1;
  state.eka_session.send_seq2hw = 1;
  state.eka_session.ip_saddr  = dev->core[core].tcp_sess[sess].src_ip;
  state.eka_session.ip_daddr  = dev->core[core].tcp_sess[sess].dst_ip;
  state.eka_session.tcp_sport = dev->core[core].tcp_sess[sess].src_port;
  state.eka_session.tcp_dport = dev->core[core].tcp_sess[sess].dst_port;
  int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state);
  if (rc < 0) on_error("error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_SET");
  EKA_LOG("Fast Session set for: %s:%u --> %s:%u",EKA_IP2STR(dev->core[core].src_ip),dev->core[core].tcp_sess[sess].src_port,EKA_IP2STR(dev->core[core].tcp_sess[sess].dst_ip),dev->core[core].tcp_sess[sess].dst_port);
  fflush(stderr);
  return;
}
//################################################

void  eka_sn_dev::ioremap_wc_tx_pkts() {
#ifdef EKA_WC
  int fd = SC_GetFileDescriptor(dev_id);
  eka_ioctl_t state = {};
  state.cmd = EKA_IOREMAP_WC;
  state.nif_num = 0;
  int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state);
  if (rc < 0) on_error("error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_IOREMAP_WC");

  uint64_t a2wr_from_ioctl = state.wcattr.bar0_wc_va;

  txPktBuf = (volatile uint8_t*) EkalineGetWcBase(dev_id);

  if (a2wr_from_ioctl != (uint64_t)txPktBuf) 
    on_error("txPktBuf from ioctl (%jx ) != EkalineGetWcBase (%jx)",a2wr_from_ioctl,(uint64_t)txPktBuf);

  EKA_LOG("WC txPktBuf = %p",txPktBuf);
#else
  EKA_LOG("WC IS NOT IMPLEMENTED IN THIS RELEASE");
#endif
  return;
}

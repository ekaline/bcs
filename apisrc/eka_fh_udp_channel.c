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
#include <errno.h>
#include <thread>
#include <assert.h>

#include "smartnic.h"
#include "eka_data_structs.h"
#include "eka_dev.h"

#include "eka_macros.h"
#include "eka_fh_udp_channel.h"
#include "eka_sn_addr_space.h"
#include "eka_sn_dev.h"

void eka_write(eka_dev_t* dev, uint64_t addr, uint64_t val); 
uint64_t eka_read(eka_dev_t* dev, uint64_t addr);

void hexDump (const char* desc, void *addr, int len);

bool fh_udp_channel::has_data() {
  pIncomingUdpPacket = SN_GetNextPacket(ChannelId, pPreviousUdpPacket, SN_TIMEOUT_NONE);
  return  pIncomingUdpPacket == NULL ? false : true;
}

const uint8_t* fh_udp_channel::get() {
  pkt_payload_ptr = SN_GetUdpPayload(pIncomingUdpPacket);
  return pkt_payload_ptr;
}

int16_t fh_udp_channel::getPayloadLen() {
  payloadLength = SN_GetPacketPayloadLength(ChannelId,pIncomingUdpPacket);
  return payloadLength;
}

void fh_udp_channel::next() {
  if (pIncomingUdpPacket == NULL) on_error("pIncomingUdpPacket == NULL");

  if (++ptr_update_ctr % 1000 == 0) if (SN_UpdateReceivePtr(ChannelId, pIncomingUdpPacket) != SC_ERR_SUCCESS) on_error ("PIZDETS");
  //  SN_UpdateReceivePtr(ChannelId, pIncomingUdpPacket);
  /* uint payloadLength = SN_GetPacketPayloadLength(0,pIncomingUdpPacket); */
  /* packetBytesTotal += 16 + ((payloadLength + 63) & ~63); */
  /* if (packetBytesTotal > 2 * 1024 * 1024) { */
  /*   SN_UpdateReceivePtr(ChannelId, pIncomingUdpPacket); */
  /*   packetBytesTotal = 0; */
  /* } */

  pPreviousUdpPacket = pIncomingUdpPacket;
  return;
}

fh_udp_channel::fh_udp_channel (EfhCtx* pEfhCtx) {
  dev = pEfhCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  if (dev->sn_dev == NULL) on_error("dev->sn_dev == NULL");
  pkt_payload_ptr = NULL;

  core = pEfhCtx->coreId;

  EKA_LOG("Opening UDP Channel for core %u, dev = %p",core, dev); fflush(stderr);fflush(stdout);

  ptr_update_ctr = 0;
  packetBytesTotal = 0;
  pPreviousUdpPacket = NULL;
  pIncomingUdpPacket = NULL;
  if (!SN_IsUdpLane(dev->sn_dev->dev_id, core)) on_error("Lane %u is not configured as an UDP lane!",core);

  ChannelId = SN_AllocateUdpChannel(dev->sn_dev->dev_id, core, -1, NULL);
  if (ChannelId == NULL) on_error("Cannot open UDP channel");

  const SN_Packet * pPreviousUdpPacket = SN_GetNextPacket(ChannelId, NULL, SN_TIMEOUT_NONE);
  if (pPreviousUdpPacket != NULL) on_error("pIncomingUdpPacket != NULL: Packet is arriving on UDP channel before any packet was sent");
  EKA_LOG("UDP channel for lane %u is opened",core);  
}

void fh_udp_channel::igmp_mc_join (uint32_t src_ip, uint32_t mcast_ip, uint16_t mcast_port) {
  static uint acl_num = 0;
  assert (acl_num < 64);
  uint64_t s = acl_num;

  //  uint8_t c = 0; //use always core#0
  char ip[20];
  sprintf (ip, "%s",EKA_IP2STR(mcast_ip));
  SN_Error errorCode = SN_IgmpJoin(ChannelId,core,(const char*)ip,be16toh(mcast_port),NULL);
  if (errorCode != SN_ERR_SUCCESS) 
    on_error("Failed to join on core %u MC %s:%u, error code %d",core,ip,mcast_port,errorCode);
  //  EKA_LOG("IGMP joined %s:%u for HW UDP Channel from %s",ip,be16toh(mcast_port),EKA_IP2STR(src_ip));

  //----------------------------------------------------
  //  EKA_LOG("ACL enable for %s:%u",EKA_IP2STR(mcast_ip),be16toh(mcast_port));
  uint32_t ip_n = htonl(mcast_ip);
  uint16_t port = be16toh(mcast_port);
  uint64_t tmp_ip   = (uint64_t) acl_num << 32 | ip_n;
  uint64_t tmp_port = (uint64_t) acl_num << 32 | port;
  eka_write (dev,FH_ACL_MC_IP_ENABLE,tmp_ip);
  eka_write (dev,FH_ACL_MC_PORT_ENABLE,tmp_port);
  acl_num++;
  //----------------------------------------------------

  //  EKA_LOG("configuring MC sess %u IP:UDP_PORT %s:%u for FPGA parser",s,EKA_IP2STR(mcast_ip),be16toh(mcast_port));

  eka_write (dev,FH_GROUP_IP,(uint64_t) (s << 32 | be32toh(mcast_ip)));
  eka_write (dev,FH_GROUP_PORT,(uint64_t) (s << 32 | be16toh(mcast_port)));

#if 0
  uint64_t fire_rx_tx_en = eka_read(dev,ENABLE_PORT);
  fire_rx_tx_en |= 1ULL << core; // RX (Parser) core enable
  fire_rx_tx_en |= 1ULL << (16+core); //fire core enable
  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);
#endif

  uint64_t val = eka_read(dev, SW_STATISTICS);
  val &= 0xFFFFFF00FFFFFFFF; 
  val |= (uint64_t) ((s+1) << 32);
  eka_write(dev, SW_STATISTICS, val);
  //  EKA_LOG("IGMP joined %s:%u for HW UDP Channel from %s",ip,mcast_port,EKA_IP2STR(src_ip));
  EKA_LOG("IGMP joined %s:%u for HW UDP Channel from %s",ip,be16toh(mcast_port),EKA_IP2STR(src_ip));

  //----------------------------------------------------

  return;
}

fh_udp_channel::~fh_udp_channel() {
  //  eka_write(dev->dev_id,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,0); // enabling FPGA UDP traffic filter
  //  while (has_data()) next(); // emptying the stale UDP packets
  //  sleep(1);
  //  EKA_LOG("Deallocating UDP Channel: pPreviousUdpPacket=%p, pIncomingUdpPacket=%p",pPreviousUdpPacket,pIncomingUdpPacket);
  SN_Error errorCode = SN_DeallocateChannel(ChannelId);
  if (errorCode != SN_ERR_SUCCESS) on_error("SN_DeallocateChannel failed with error code %d", errorCode);
}

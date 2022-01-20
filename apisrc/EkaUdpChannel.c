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

#include "eka_macros.h"
#include "EkaUdpChannel.h"
#include "EkaSnDev.h"
#include "EkaDev.h"

void hexDump (const char* desc, void *addr, int len);

bool EkaUdpChannel::has_data() {
  pIncomingUdpPacket = SN_GetNextPacket(ChannelId, pPreviousUdpPacket, SN_TIMEOUT_NONE);
  return  pIncomingUdpPacket == NULL ? false : true;
}

const uint8_t* EkaUdpChannel::get() {
  pkt_payload_ptr = SN_GetUdpPayload(pIncomingUdpPacket);
  return pkt_payload_ptr;
}

int16_t EkaUdpChannel::getPayloadLen() {
  payloadLength = SN_GetPacketPayloadLength(ChannelId,pIncomingUdpPacket);
  return payloadLength;
}

void EkaUdpChannel::next() {
  if (pIncomingUdpPacket == NULL) on_error("pIncomingUdpPacket == NULL");

  if (++ptr_update_ctr % 1000 == 0)
    if (SN_UpdateReceivePtr(ChannelId, pIncomingUdpPacket) != SN_ERR_SUCCESS)
      on_error ("PIZDETS");
  pPreviousUdpPacket = pIncomingUdpPacket;
  return;
}

uint64_t EkaUdpChannel::emptyBuffer() {
  uint64_t pktCtr = 0;
  while (1) {
    pPreviousUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);      
    if (pPreviousUdpPacket == NULL) break;
    if (SC_UpdateReceivePtr(ChannelId, pPreviousUdpPacket) != SN_ERR_SUCCESS) 
      on_error ("Failed to sync DMA ReceivePtr");
    pktCtr++;

  }
  pIncomingUdpPacket = NULL;
  return pktCtr;
}

EkaUdpChannel::EkaUdpChannel(EkaDev* ekaDev, uint8_t coreId, int requestedChId) {
  dev = ekaDev;
  pkt_payload_ptr = NULL;

  core = coreId;
  ptr_update_ctr = 0;
  packetBytesTotal = 0;
  pPreviousUdpPacket = NULL;
  pIncomingUdpPacket = NULL;
  if (!SN_IsUdpLane(dev->snDev->dev_id, core)) on_error("Lane %u is not configured as an UDP lane!",core);

  ChannelId = SN_AllocateUdpChannel(dev->snDev->dev_id, core, requestedChId, NULL);
  if (ChannelId == NULL) on_error("Cannot open UDP channel");

  chId = SC_GetChannelNumber(ChannelId);
  if (requestedChId != -1 && requestedChId != chId) {
    on_error("requestedChId %d != chId %d",requestedChId, chId);
  }

  while (1) {
    pPreviousUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);      
    if (pPreviousUdpPacket == NULL) break;
    if (SC_UpdateReceivePtr(ChannelId, pPreviousUdpPacket) != SN_ERR_SUCCESS) 
      on_error ("Failed to sync DMA ReceivePtr");
  }

  EKA_LOG("UDP channel %d for lane %u is opened",chId,core);  
}

void EkaUdpChannel::igmp_mc_join (uint32_t mcast_ip, uint16_t mcast_port, int16_t vlanTag) {
  // MC Port here is in BE (Human) format

  SN_IgmpOptions igmpOptions = {};
  igmpOptions.StructSize = sizeof(igmpOptions);
  igmpOptions.EnableMulticastBypass = true;
  igmpOptions.VLanTag = vlanTag;

  char ip[20] = {};
  sprintf (ip, "%s",EKA_IP2STR(mcast_ip));

  SN_Error errorCode = SN_IgmpJoin(ChannelId,core,(const char*)ip,mcast_port, NULL /* vlanTag ? &igmpOptions : NULL */);
  if (errorCode != SN_ERR_SUCCESS) 
    on_error("Failed to join on core %u MC %s:%u, vlanTag=%d, error code %d",
	     core,ip,mcast_port,vlanTag,(int)errorCode);
  EKA_LOG("IGMP joined %s:%u for HW UDP Channel %d from coreId = %u",
	  ip,mcast_port,chId,core);

  /* saveMcStat(dev,core,mcast_ip); */

  //----------------------------------------------
  /* struct ip_mreq mreq = {}; */
  /* mreq.imr_interface.s_addr = dev->core[0].src_ip; */
  /*     mreq.imr_multiaddr.s_addr = mcast_ip; */
  /*     printf ("%s: joining IGMP for sock_fd=%d, sess %d on core %d, from local addr %s ",__func__,sock_fd,s,c,inet_ntoa(mreq.imr_interface)); */
  /*     printf ("to %s:%d \n",inet_ntoa(mreq.imr_multiaddr),ntohs(dev->core[c].udp_sess[s].mcast.sin_port)); */
  /*     if (setsockopt(dev->core[c].udp_sess[s].sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) */
  /* 	on_error ("%s : fail to join IGMP for sess %d on core %d, MC addr: %s:%d\n",__func__,s,c,inet_ntoa( mreq.imr_multiaddr),ntohs(dev->core[c].udp_sess[s].mcast.sin_port)); */

  return;
}

EkaUdpChannel::~EkaUdpChannel() {
  SN_Error errorCode = SN_DeallocateChannel(ChannelId);
  if (errorCode != SN_ERR_SUCCESS) on_error("SN_DeallocateChannel failed with error code %d", errorCode);
}

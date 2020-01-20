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
#include "eka_user_channel.h"
#include "eka_sn_addr_space.h"

#include "eka_sn_dev.h"

bool eka_get_tcp_pkt(EkaDev* dev, uint8_t** ptr, uint32_t* pLen) {
  if (! dev->tcp_ch->has_data()) return false;
  *ptr = (uint8_t*) dev->tcp_ch->get();
  *pLen = dev->tcp_ch->get_payload_size();
  return true;
}

bool eka_get_uch_pkt(EkaDev* dev, uint8_t** ptr, uint32_t* pLen) {
  if (! dev->fp_ch->has_data()) return false;
  *ptr = (uint8_t*) dev->fp_ch->get();
  *pLen = dev->fp_ch->get_payload_size();
  return true;
}

void eka_next_tcp_pkt(EkaDev* dev) {
  dev->tcp_ch->next();
}

void eka_open_sn_channels (EkaDev* dev,SN_DeviceId devId) {
  EKA_LOG("Openning User Logic Channels");
  fflush(stderr);
  dev->fr_ch  = new eka_user_channel(dev,devId,eka_user_channel::TYPE::FIRE_REPORT);
  dev->fp_ch  = new eka_user_channel(dev,devId,eka_user_channel::TYPE::FAST_PATH);
  //  dev->tcp_ch = new eka_user_channel(dev,eka_user_channel::TYPE::TCP);
}

bool eka_user_channel::has_data() {
  if(dev == NULL) on_error("dev = NULL");
  //  EKA_LOG("ChannelId=%p, pPreviousUdpPacket=%p",ChannelId,pPreviousUdpPacket); fflush(stderr);
  pIncomingUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SN_TIMEOUT_NONE);
  return  pIncomingUdpPacket == NULL ? false : true;
}

const uint8_t* eka_user_channel::get() {
  pkt_payload_ptr = SC_GetUserLogicPayload(pIncomingUdpPacket);
  return pkt_payload_ptr;
}

uint eka_user_channel::get_payload_size() {
  return SC_GetUserLogicPayloadLength(pIncomingUdpPacket);
}

void eka_user_channel::next() {
  if (pIncomingUdpPacket == NULL) on_error("pIncomingUdpPacket == NULL");
  if (++ptr_update_ctr % 200 == 0) if (SC_UpdateReceivePtr(ChannelId, pIncomingUdpPacket) != SC_ERR_SUCCESS) on_error ("PIZDETS");
  pPreviousUdpPacket = pIncomingUdpPacket;
  return;
}

eka_user_channel::eka_user_channel (EkaDev* pEkaDev,SN_DeviceId devId, TYPE t) {
  type = t;

  dev = pEkaDev;
  EKA_LOG("Openning %s User Channel",USR_CH_DECODE(type));
  fflush(stderr);
  dev_id = devId;

  pkt_payload_ptr = NULL;

  ptr_update_ctr = 0;
  packetBytesTotal = 0;

  ChannelId = SC_AllocateUserLogicChannel(dev_id,(int16_t)type, NULL);
  if (ChannelId == NULL) on_error("Cannot open User channel for %s",USR_CH_DECODE(type));
  EKA_LOG("SC_AllocateUserLogicChannel: OK for %s (=%u)",USR_CH_DECODE(type),(int16_t)type);

  if ((pPreviousUdpPacket = SC_GetNextPacket(ChannelId, NULL, SN_TIMEOUT_NONE)) != NULL) on_error("Packet is arriving on User channel before any packet was sent");
  pIncomingUdpPacket = NULL;
}

eka_user_channel::~eka_user_channel() {
  while (has_data()) next(); // emptying the stale UDP packets
  sleep(1);
  EKA_LOG("Deallocating User Logic Channel for %s",USR_CH_DECODE(type));
  SC_Error errorCode = SN_DeallocateChannel(ChannelId);
  if (errorCode != SN_ERR_SUCCESS) on_error("SN_DeallocateChannel failed with error code %d", errorCode);
}

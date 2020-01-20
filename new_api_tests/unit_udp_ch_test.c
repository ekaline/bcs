#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <thread>
#include <assert.h>

#include "smartnic.h"

//void eka_write_devid(SN_DeviceId dev_id, uint64_t addr, uint64_t val);

#define MC_IP "233.54.12.148"

#define on_error(...) { fprintf(stderr, "FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }

#define FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL 0xf0520

static volatile bool keep_work = true;;

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected, closing the MD file and quitting\n",__func__);
  printf ("%s: exitting...\n",__func__);
  fflush(stdout);
  return;
}

void eka_write_devid(SN_DeviceId dev_id, uint64_t addr, uint64_t val) {
    if (SN_ERR_SUCCESS != SN_WriteUserLogicRegister(dev_id, addr/8, val))
      on_error("SN_Write returned smartnic error code : %d",SN_GetLastErrorCode());
}
int main() {
  keep_work = true;
  signal(SIGINT, INThandler);

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  if (!SN_IsUdpLane(dev_id, 0)) on_error("Lane 0 is not configured as an UDP lane!");

  eka_write_devid(dev_id,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,1); // disabling FPGA UDP traffic filter

  SC_ChannelId ChannelId = SC_AllocateUdpChannel(dev_id, 0, -1, NULL);
  if (ChannelId == NULL) on_error("Cannot open UDP channel");

  const SC_Packet * pPreviousUdpPacket = SC_GetNextPacket(ChannelId, NULL, SC_TIMEOUT_NONE);
  if (pPreviousUdpPacket != NULL) on_error("pIncomingUdpPacket != NULL: Packet is arriving on UDP channel before any packet was sent");

  SC_Error errorCode = SC_IgmpJoin(ChannelId,0,(const char*) MC_IP,0,NULL);
  if (errorCode != SC_ERR_SUCCESS) on_error("Failed to IGMP join %s with error code %d",MC_IP,errorCode);
  
  uint8_t* first_p = 0;
  uint i = 0;
  uint64_t acc_size = 0;
  const SC_Packet* pIncomingUdpPacket = NULL;

  while (keep_work) {
    pIncomingUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);
    if (pIncomingUdpPacket == NULL) continue;
    uint pkt_size = SC_GetPacketPayloadLength(0,pIncomingUdpPacket);

    /* if (i == 0) { */
    /*   first_p = (uint8_t*) pIncomingUdpPacket; */
    /*   sleep (2); */
    /* } */
    //    printf ("%5u: pIncomingUdpPacket = %p, pkt_size = %u \n",i,pIncomingUdpPacket,pkt_size);

    if (i == 0) first_p = (uint8_t*) pIncomingUdpPacket;

    i++;
    acc_size += pkt_size;

    SC_UpdateReceivePtr(ChannelId, pIncomingUdpPacket);
    pPreviousUdpPacket = pIncomingUdpPacket;
  }
  uint8_t* last_p = (uint8_t*) pPreviousUdpPacket;

  printf ("After %d packets: first_p = %p, last_p = %p, delta ptrs = %ju Bytes, acc_size=%ju Bytes\n",i,first_p,last_p,(uint64_t)(last_p - first_p),acc_size);
  
  errorCode = SC_DeallocateChannel(ChannelId);
  if (errorCode != SC_ERR_SUCCESS) on_error("SC_DeallocateChannel failed with error code %d", errorCode);
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  return 0;
}

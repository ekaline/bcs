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

#define MC_IP "239.0.0.1"
#define MC_PORT 6000

#define on_error(...) { fprintf(stderr, "FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }

#define FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL 0xf0520

static volatile bool keep_work = true;;
struct message_t {
  uint64_t seq;
  uint     thread_id;
  char     pad[30];
};

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

void rcv_mc_thread(SC_DeviceId dev_id, uint thread_id) {
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  if (!SN_IsUdpLane(dev_id, 0)) on_error("Lane 0 is not configured as an UDP lane!");

  eka_write_devid(dev_id,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,1); // disabling FPGA UDP traffic filter

  SC_ChannelId ChannelId = SC_AllocateUdpChannel(dev_id, 0, -1, NULL);
  if (ChannelId == NULL) on_error("Cannot open UDP channel");

  const SC_Packet * pPreviousUdpPacket = SC_GetNextPacket(ChannelId, NULL, SC_TIMEOUT_NONE);
  if (pPreviousUdpPacket != NULL) on_error("pIncomingUdpPacket != NULL: Packet is arriving on UDP channel before any packet was sent");

  SC_Error errorCode = SC_IgmpJoin(ChannelId,0,(const char*) MC_IP,MC_PORT+thread_id,NULL);
  if (errorCode != SC_ERR_SUCCESS) on_error("Failed to IGMP join %s:%u with error code %d",MC_IP,MC_PORT+thread_id,errorCode);
  
  uint64_t expected = 0;
  const SC_Packet* pIncomingUdpPacket = NULL;

  while (keep_work) {
    pIncomingUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);
    if (pIncomingUdpPacket == NULL) continue;
    ++expected;
    const uint8_t* pkt_payload_ptr = SN_GetUdpPayload(pIncomingUdpPacket);
    if (((message_t*)pkt_payload_ptr)->thread_id != thread_id) 
      on_error ("My thread_id %u != from message thread_id %u",thread_id,((message_t*)pkt_payload_ptr)->thread_id);
    if (((message_t*)pkt_payload_ptr)->seq != expected) {
      printf("ERROR: %u: received (=%ju) != expected (=%ju) : dropped %ju\n",thread_id,((message_t*)pkt_payload_ptr)->seq,expected,((message_t*)pkt_payload_ptr)->seq - expected);
      expected = ((message_t*)pkt_payload_ptr)->seq;
    }
    //    if (expected % 1000000 == 0) printf("%u: %ju packets received\n",thread_id,expected);
    if (expected % 100000 == 0) if (SC_UpdateReceivePtr(ChannelId, pIncomingUdpPacket) != SC_ERR_SUCCESS) on_error ("PIZDETS");
    pPreviousUdpPacket = pIncomingUdpPacket;
  }

  printf("%u: Totally %ju packets received\n",thread_id,expected);

  //  errorCode = SC_DeallocateChannel(ChannelId);
  //  if (errorCode != SC_ERR_SUCCESS) on_error("SC_DeallocateChannel failed with error code %d", errorCode);
  return;
}

int main(int argc, char **argv) {
  keep_work = true;
  signal(SIGINT, INThandler);
  uint numThreads = atoi(argv[1]);

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");

  for (uint i = 0; i < numThreads; i ++) {
    printf ("Spin OFF %u\n",i);
    std::thread r = std::thread(rcv_mc_thread,dev_id,i);
    r.detach();
  }

  while (keep_work) {};

  sleep (1);
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");

  return 0;

}

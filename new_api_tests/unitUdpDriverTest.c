#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <iterator>
#include <thread>

#include <linux/sockios.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <sys/time.h>
#include <chrono>

#include "smartnic.h"

#include "eka_macros.h"

#include <fcntl.h>

/* --------------------------------------------- */
volatile bool keep_work = true;
static const uint SleepTime = 90;
/* --------------------------------------------- */
struct McMsg {
  uint64_t  seq     = 0;
  char      txt[60] = {};
};

struct McParams {
  int8_t      coreId;
  const char* ip;
  uint16_t    port;
};
/* --------------------------------------------- */


void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
}

/* ############################################# */

int sendMcPkt(const McParams* mcParams, const char* srcIp, const char* srcMsg, int64_t id) {
  struct sockaddr_in mcAddr = {};
  mcAddr.sin_family      = AF_INET;  
  mcAddr.sin_addr.s_addr = inet_addr(mcParams->ip);
  mcAddr.sin_port        = be16toh(mcParams->port);
  
  struct sockaddr_in srcAddr = {};
  srcAddr.sin_family      = AF_INET;  
  srcAddr.sin_addr.s_addr = inet_addr(srcIp);
  srcAddr.sin_port        = 0;
  
  int mcSock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (mcSock < 0) on_error("failed to open MC UDP sock");
  if (bind(mcSock,(sockaddr*)&srcAddr, sizeof(sockaddr)) != 0 ) {
    on_error("failed to bind server mcSock to %s:%u",EKA_IP2STR(srcAddr.sin_addr.s_addr),be16toh(srcAddr.sin_port));
  } else {
    TEST_LOG("mcSock is binded to %s:%u",EKA_IP2STR(srcAddr.sin_addr.s_addr),be16toh(srcAddr.sin_port));
  }


  char txMsg[200] = {};
  sprintf(txMsg,"%s %2jd",srcMsg,id);

  if (sendto(mcSock,txMsg,strlen(txMsg),0,(const sockaddr*)&mcAddr,sizeof(sockaddr)) < 0) 
    on_error ("MC Pkt send failed");
  
  return 0;


}

/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);
;
  /* ============================================== */

  McParams setA[] = {
    {0,"224.0.74.1", 30301},
    {0,"224.0.74.2", 30302},
    {0,"224.0.74.3", 30303},
    {0,"224.0.74.4", 30304},
    {0,"224.0.74.5", 30305},
    {0,"224.0.74.6", 30306},
    {0,"224.0.74.7", 30307},
    {0,"224.0.74.8", 30308},
    {0,"224.0.74.9", 30309},
    {0,"224.0.74.10",30310},
    {0,"224.0.74.11",30311},
    {0,"224.0.74.12",30312},
    {0,"224.0.74.13",30313},
    {0,"224.0.74.14",30314},
    {0,"224.0.74.15",30315},
    {0,"224.0.74.16",30316},
    {0,"224.0.74.17",30317},
    {0,"224.0.74.18",30318},
    {0,"224.0.74.19",30319},
    {0,"224.0.74.20",30320},
    {0,"224.0.74.21",30321},
    {0,"224.0.74.22",30322},
    {0,"224.0.74.23",30323},
    {0,"224.0.74.24",30324},
    {0,"224.0.74.25",30325},
    {0,"224.0.74.26",30326},
    {0,"224.0.74.27",30327},
    {0,"224.0.74.28",30328},
    {0,"224.0.74.29",30329},
    {0,"224.0.74.30",30330},
    {0,"224.0.74.31",30331},
    {0,"224.0.74.32",30332},
    {0,"224.0.74.33",30333},
    {0,"224.0.74.34",30334},
    {0,"224.0.74.35",30335},
  };

  McParams setB[] = {
    {2,"224.0.84.1", 30301},
    {2,"224.0.84.2", 30302},
    {2,"224.0.84.3", 30303},
    {2,"224.0.84.4", 30304},
    {2,"224.0.84.5", 30305},
    {2,"224.0.84.6", 30306},
    {2,"224.0.84.7", 30307},
    {2,"224.0.84.8", 30308},
    {2,"224.0.84.9", 30309},
    {2,"224.0.84.10",30310},
    {2,"224.0.84.11",30311},
    {2,"224.0.84.12",30312},
    {2,"224.0.84.13",30313},
    {2,"224.0.84.14",30314},
    {2,"224.0.84.15",30315},
    {2,"224.0.84.16",30316},
    {2,"224.0.84.17",30317},
    {2,"224.0.84.18",30318},
    {2,"224.0.84.19",30319},
    {2,"224.0.84.20",30320},
    {2,"224.0.84.21",30321},
    {2,"224.0.84.22",30322},
    {2,"224.0.84.23",30323},
    {2,"224.0.84.24",30324},
    {2,"224.0.84.25",30325},
    {2,"224.0.84.26",30326},
    {2,"224.0.84.27",30327},
    {2,"224.0.84.28",30328},
    {2,"224.0.84.29",30329},
    {2,"224.0.84.30",30330},
    {2,"224.0.84.31",30331},
    {2,"224.0.84.32",30332},
    {2,"224.0.84.33",30333},
    {2,"224.0.84.34",30334},
    {2,"224.0.84.35",30335},
  };

  /* ============================================== */ 

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  if (!SN_IsUdpLane(dev_id, 0)) on_error("Lane 0 is not configured as an UDP lane!");

  SC_ChannelId ChannelIdA = SC_AllocateUdpChannel(dev_id, 0, -1, NULL);
  if (ChannelIdA == NULL) on_error("Cannot open UDP channel");
  SC_ChannelId ChannelIdB = SC_AllocateUdpChannel(dev_id, 2, -1, NULL);
  if (ChannelIdB == NULL) on_error("Cannot open UDP channel");

  const SC_Packet * pPreviousUdpPacketA = SC_GetNextPacket(ChannelIdA, NULL, SC_TIMEOUT_NONE);
  if (pPreviousUdpPacketA != NULL) {
    on_warning("pPreviousUdpPacket != NULL: Packet is arriving on UDP channel before any packet was sent");
    SC_UpdateReceivePtr(ChannelIdA, pPreviousUdpPacketA);
  }
  const SC_Packet * pPreviousUdpPacketB = SC_GetNextPacket(ChannelIdB, NULL, SC_TIMEOUT_NONE);
  if (pPreviousUdpPacketB != NULL) {
    on_warning("pPreviousUdpPacket != NULL: Packet is arriving on UDP channel before any packet was sent");
    SC_UpdateReceivePtr(ChannelIdB, pPreviousUdpPacketB);
  }

  for (size_t i = 0; i < std::size(setA); i++) {
    SC_Error errorCode = SC_IgmpJoin(ChannelIdA,setA[i].coreId,setA[i].ip,setA[i].port,NULL);
    if (errorCode != SC_ERR_SUCCESS) on_error("Failed to IGMP join coreId=%d, %s:%u with error code %d",
					      setA[i].coreId,setA[i].ip,setA[i].port,errorCode);
  }

  for (size_t i = 0; i < std::size(setB); i++) {
    SC_Error errorCode = SC_IgmpJoin(ChannelIdB,setB[i].coreId,setB[i].ip,setB[i].port,NULL);
    if (errorCode != SC_ERR_SUCCESS) on_error("Failed to IGMP join coreId=%d, %s:%u with error code %d",
					      setB[i].coreId,setB[i].ip,setB[i].port,errorCode);
  }


  /* ============================================== */
  for (size_t i = 0; i < std::size(setA); i++) {
    TEST_LOG("Sending SetA:%2jd",i);
    sendMcPkt(&setA[i],"10.0.0.10","SetA",i);
  }
  for (size_t i = 0; i < std::size(setB); i++) {
    TEST_LOG("Sending SetB:%2jd",i);
    sendMcPkt(&setB[i],"10.0.0.11","SetB",i);
  }

  for (size_t i = 0; i < std::size(setA); i++) {
    const SC_Packet *pIncomingUdpPacketA = SC_GetNextPacket(ChannelIdA, pPreviousUdpPacketA, SC_TIMEOUT_NONE);
    if (pIncomingUdpPacketA == NULL) break;
    auto payload {reinterpret_cast<const char*>(SC_GetUdpPayload(pIncomingUdpPacketA))};
    TEST_LOG("setA: \'%s\'",(const char*)payload);
    pPreviousUdpPacketA = pIncomingUdpPacketA;
  }

  for (size_t i = 0; i < std::size(setB); i++) {
    const SC_Packet *pIncomingUdpPacketB = SC_GetNextPacket(ChannelIdB, pPreviousUdpPacketB, SC_TIMEOUT_NONE);
    if (pIncomingUdpPacketB == NULL) break;
    auto payload {reinterpret_cast<const char*>(SC_GetUdpPayload(pIncomingUdpPacketB))};
    TEST_LOG("setB: \'%s\'",(const char*)payload);
    pPreviousUdpPacketB = pIncomingUdpPacketB;
  }


  /* ============================================== */
  while (keep_work) usleep(0);

  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");

  return 0;
}

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
/* --------------------------------------------- */


void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
}

/* ############################################# */

static void eka_write_devid(SN_DeviceId dev_id, uint64_t addr, uint64_t val) {
    if (SN_ERR_SUCCESS != SN_WriteUserLogicRegister(dev_id, addr/8, val))
      on_error("SN_Write returned smartnic error code : %d",SN_GetLastErrorCode());
}

/* ############################################# */

uint64_t eka_read_devid(SN_DeviceId dev_id, uint64_t addr) {
  uint64_t ret;
  if (SN_ERR_SUCCESS != SN_ReadUserLogicRegister(dev_id, addr/8, &ret))
    on_error("SN_Read returned smartnic error code : %d",SN_GetLastErrorCode());
  return ret;
}
/* ############################################# */

int udpSender(std::string mcIp, uint16_t mcPort, std::string srcIp) {
  struct sockaddr_in mcAddr = {};
  mcAddr.sin_family      = AF_INET;  
  mcAddr.sin_addr.s_addr = inet_addr(mcIp.c_str());
  mcAddr.sin_port        = be16toh(mcPort);
  
  struct sockaddr_in srcAddr = {};
  srcAddr.sin_family      = AF_INET;  
  srcAddr.sin_addr.s_addr = inet_addr(srcIp.c_str());
  srcAddr.sin_port        = 0;
  
  int mcSock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (mcSock < 0) on_error("failed to open MC UDP sock");
  if (bind(mcSock,(sockaddr*)&srcAddr, sizeof(sockaddr)) != 0 ) {
    on_error("failed to bind server mcSock to %s:%u",EKA_IP2STR(srcAddr.sin_addr.s_addr),be16toh(srcAddr.sin_port));
  } else {
    TEST_LOG("mcSock is binded to %s:%u",EKA_IP2STR(srcAddr.sin_addr.s_addr),be16toh(srcAddr.sin_port));
  }

  McMsg msg = {};
  msg.seq = 1;
  while (keep_work) {
    sprintf(msg.txt,"SEQ=%016ju",msg.seq++);

    if (sendto(mcSock,&msg,sizeof(msg),0,(const sockaddr*)&mcAddr,sizeof(sockaddr)) < 0) 
      on_error ("MC trigger send failed");

    usleep(1000000);
  }
  return 0;
}

/* inline auto getPktTimestamp(const uint8_t* pPayload) { */
/*   auto pEthHdr {pPayload - sizeof(EkaUdpHdr) - sizeof(EkaIpHdr) - sizeof(EkaEthHdr)}; */
/*   auto p {pEthHdr}; */
/*   auto pTimestamp {reinterpret_cast<const uint64_t*>(pEthHdr + 6)}; */
/*   return *pTimestamp & 0x0000FFFFFFFFFFFF; */
/*   uint8_t t[8] = {}; */
/*   t[0] = 0; */
/*   t[1] = 0; */
/*   t[2] = p[0]; */
/*   t[3] = p[1]; */
/*   t[4] = p[2]; */
/*   t[5] = p[3]; */
/*   t[6] = p[4]; */
/*   t[7] = p[5]; */

/*   //  return be64toh(*pTimestamp) & 0x0000FFFFFFFFFFFF; */
/*   // return be64toh(*pTimestamp & 0x0000FFFFFFFFFFFF); */
/*   //  return *pTimestamp & 0x0000FFFFFFFFFFFF; */
/*   //return be64toh(*pTimestamp & 0xFFFFFFFFFFFF0000); */
/* } */
/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);


  std::string mcIp = "239.255.119.16"; // Ekaline lab default
  uint16_t mcPort = 18333;          // Ekaline lab default

  std::string mcSrcIp  = "10.0.0.10";      // Ekaline lab default
  //  std::string clientIp  = "100.0.0.110";    // Ekaline lab default

 
  std::thread sender = std::thread(udpSender,mcIp,mcPort,mcSrcIp);
  sender.detach();
  /* ============================================== */


  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  if (!SN_IsUdpLane(dev_id, 0)) on_error("Lane 0 is not configured as an UDP lane!");

  SC_ChannelId ChannelId = SC_AllocateUdpChannel(dev_id, 0, -1, NULL);
  if (ChannelId == NULL) on_error("Cannot open UDP channel");

  const SC_Packet * pPreviousUdpPacket = SC_GetNextPacket(ChannelId, NULL, SC_TIMEOUT_NONE);
  if (pPreviousUdpPacket != NULL) {
    on_warning("pPreviousUdpPacket != NULL: Packet is arriving on UDP channel before any packet was sent");
    SC_UpdateReceivePtr(ChannelId, pPreviousUdpPacket);
  }

  SC_Error errorCode = SC_IgmpJoin(ChannelId,0,(const char*) mcIp.c_str(),0,NULL);
  if (errorCode != SC_ERR_SUCCESS) on_error("Failed to IGMP join %s with error code %d",mcIp.c_str(),errorCode);
  
  const SC_Packet* pIncomingUdpPacket = NULL;

  uint64_t state = eka_read_devid(dev_id,0xf0020);
  state |= (1ULL << 33);
  eka_write_devid(dev_id,0xf0020,state);

  uint64_t cyclesSinceMidnight = nsSinceMidnight() * (EKA_FPGA_FREQUENCY / 1000.0);
  eka_write_devid(dev_id,FPGA_RT_CNTR,cyclesSinceMidnight);

  int i = 0;
  printf("sample,pktCycles,currNs,pktNs,currNs-pktNs\n");
  /* ============================================== */
  while (keep_work) {
    pIncomingUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);
    if (pIncomingUdpPacket == NULL) continue;
    
    auto pPayload {(SC_GetUdpPayload(pIncomingUdpPacket))};

    //   printf ("\n------------\n");

    /* uint64_t fpgaCycles = eka_read_devid(dev_id,FPGA_RT_CNTR); */
    /* char epochTimeString[100] = {}; */
    /* printFpgaTime(epochTimeString,sizeof(epochTimeString),fpgaCycles); */

    auto pktCycles = getPktTimestampCycles(pPayload);
    /* char pktTimeStampString[100] = {}; */
    /* printFpgaTime(pktTimeStampString,sizeof(pktTimeStampString),pktCycles); */
    /* TEST_LOG("FPGA time: %s, Pkt time: %s\n",epochTimeString,pktTimeStampString); */

    uint64_t pktNs = pktCycles / (EKA_FPGA_FREQUENCY / 1000.0);

    //    auto deltaNs = pktNs - nsSinceMidnight();
    //   auto deltaNs = nsSinceMidnight() - pktNs;

    /* TEST_LOG("deltaNs = %jd, fpgaCycles=%ju, pktCycles=%ju", */
    /* 	     deltaNs,fpgaCycles,pktCycles); */
    auto currNs = nsSinceMidnight();

    printf("%d,%ju,%ju,%ju,%jd\n",
	   ++i,pktCycles,currNs,pktNs,currNs-pktNs);
 


    /* ------------------------------------------- */

    SC_UpdateReceivePtr(ChannelId, pIncomingUdpPacket);
    pPreviousUdpPacket = pIncomingUdpPacket;
  }

  /* ============================================== */
  errorCode = SC_DeallocateChannel(ChannelId);
  if (errorCode != SC_ERR_SUCCESS)
    on_error("SC_DeallocateChannel failed with error code %d", errorCode);
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");

  /* TEST_LOG("Last sequence = %ju",currSeq); */
  return 0;
}

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

  TEST_LOG("MC =  %s:%u",EKA_IP2STR(mcAddr.sin_addr.s_addr),be16toh(mcAddr.sin_port));

  McMsg msg = {};
  msg.seq = 1;
  while (keep_work) {
    sprintf(msg.txt,"SEQ=%016ju",msg.seq++);

    if (sendto(mcSock,&msg,sizeof(msg),0,(const sockaddr*)&mcAddr,sizeof(sockaddr)) < 0) 
      on_error ("MC trigger send failed");
  }
  return 0;
}

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
  SC_Error errorCode;

  while (keep_work) {

    SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
    if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");

    /* if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice"); */
    /* dev_id = SC_OpenDevice(NULL, NULL); */
    /* if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device"); */



    if (!SN_IsUdpLane(dev_id, 0)) on_error("Lane 0 is not configured as an UDP lane!");

    SC_ChannelId ChannelId = SC_AllocateUdpChannel(dev_id, 0, -1, NULL);
    if (ChannelId == NULL) on_error("Cannot open UDP channel");

    int chId = SC_GetChannelNumber(ChannelId);
    TEST_LOG("Allocated UdpCh: %d",chId);

    TEST_LOG("Trying SC_IgmpJoin %s",mcIp.c_str());
    errorCode = SC_IgmpJoin(ChannelId,0, mcIp.c_str(),0,NULL);
    if (errorCode != SC_ERR_SUCCESS)
      on_error("SC_IgmpJoin %s failed with error code %d",mcIp.c_str(),errorCode);
  

    TEST_LOG("Trying SC_IgmpLeave %s",mcIp.c_str());
    errorCode = SC_IgmpLeave(ChannelId, 0, mcIp.c_str(), 0, NULL);
    if (errorCode != SC_ERR_SUCCESS)
      on_error("SC_IgmpLeave %s failed with error code %d",mcIp.c_str(),errorCode);

    /* std::string mc = "224.0.0.1"; */
    /* TEST_LOG("Trying SC_IgmpJoin %s",mc.c_str()); */
    /* errorCode = SC_IgmpJoin(ChannelId,0, mc.c_str(),0,NULL); */
    /* if (errorCode != SC_ERR_SUCCESS) */
    /*   on_error("SC_IgmpJoin %s failed with error code %d",mc.c_str(),errorCode); */
  

    /* TEST_LOG("Deallocating UdpCh: %d",chId); */
    /* errorCode = SC_DeallocateChannel(ChannelId); */
    /* if (errorCode != SC_ERR_SUCCESS) */
    /*   on_error("SC_DeallocateChannel failed with error code %d", errorCode); */

    /* TEST_LOG("Reallocating UdpCh: %d",chId); */
    /* ChannelId = SC_AllocateUdpChannel(dev_id, 0, chId, NULL); */
    /* if (ChannelId == NULL) on_error("Cannot open UDP channel"); */


    int trial = 1;
    const SC_Packet * pPreviousUdpPacket = NULL;
    while (keep_work) {
      pPreviousUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);      
      if (pPreviousUdpPacket != NULL) {
	const McMsg* msg = (const McMsg*) SC_GetUdpPayload(pPreviousUdpPacket);
	if (trial % 1000000 == 0) 
	  on_warning("%d: pPreviousUdpPacket %p != NULL, msg->seq=%ju",
		    trial,  pPreviousUdpPacket, msg->seq);
	SC_UpdateReceivePtr(ChannelId, pPreviousUdpPacket);
	trial++;
	continue;
      }
      break;
    }
    TEST_LOG("Emptied after %d pkts",trial); fflush(stderr); fflush(stdout);

    sleep(2);

    trial = 1;
    //    const SC_Packet * pPreviousUdpPacket = NULL;
    while (keep_work) {
      pPreviousUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);      
      if (pPreviousUdpPacket != NULL) {
	const McMsg* msg = (const McMsg*) SC_GetUdpPayload(pPreviousUdpPacket);
	//	if (trial % 1000000 == 0) 
	  on_warning("%d: pPreviousUdpPacket %p != NULL, msg->seq=%ju",
		    trial,  pPreviousUdpPacket, msg->seq);
	SC_UpdateReceivePtr(ChannelId, pPreviousUdpPacket);
	trial++;
	continue;
      }
      break;
    }
    TEST_LOG("Emptied after %d pkts",trial); fflush(stderr); fflush(stdout);
    exit (1);

    TEST_LOG("Trying SC_IgmpJoin %s",mcIp.c_str());
    errorCode = SC_IgmpJoin(ChannelId,0,(const char*) mcIp.c_str(),0,NULL);
    if (errorCode != SC_ERR_SUCCESS) 
      on_error("Failed to IGMP join %s with error code %d",mcIp.c_str(),errorCode);
  
    const SC_Packet* pIncomingUdpPacket = NULL;
    uint64_t currSeq = 0;
    uint64_t maxSeq = 0;

    /* ============================================== */
    int cnt = 0;
    while (keep_work && cnt < 100) {
      pIncomingUdpPacket = SC_GetNextPacket(ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);
      if (pIncomingUdpPacket == NULL) continue;

      const McMsg* msg = (const McMsg*) SC_GetUdpPayload(pIncomingUdpPacket);

      if (currSeq == 0) {
	TEST_LOG("First sequence: %8ju",msg->seq);      
      } else {
	if (msg->seq != currSeq + 1) 
	  on_warning("GAP %ju: msg->seq = %ju,currSeq = %ju",
		     msg->seq - currSeq, msg->seq, currSeq);
      }
      currSeq = msg->seq;
      if (currSeq <= maxSeq) on_warning("WRAPAROUND!!! currSeq %ju <= maxSeq %ju",currSeq,maxSeq);
      maxSeq = currSeq;
      /* ------------------------------------------- */
      if (currSeq % 100000 == 0) {
	TEST_LOG("currSeq = %ju: sleeping %u seconds",currSeq,SleepTime);
	sleep(SleepTime);
      }

      /* ------------------------------------------- */

      SC_UpdateReceivePtr(ChannelId, pIncomingUdpPacket);
      pPreviousUdpPacket = pIncomingUdpPacket;
    }
    TEST_LOG("Closing device after %d packets",cnt);
    errorCode = SC_DeallocateChannel(ChannelId);
    if (errorCode != SC_ERR_SUCCESS) 
      on_error("SC_DeallocateChannel failed with error code %d", errorCode);
    if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  }

  /* ============================================== */

  TEST_LOG("Normal exit");
  return 0;
}

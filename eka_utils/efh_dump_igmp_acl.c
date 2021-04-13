#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <net/if.h>

#include "smartnic.h"
#include "eka_macros.h"
#include "eka_sn_addr_space.h"
#include "EkaMcState.h"
#include "ctls.h"
#include "eka.h"

#define NUM_OF_CORES EKA_MAX_CORES
#define FREQUENCY EKA_FPGA_FREQUENCY

static SN_DeviceId devId;

struct IfParams {
  char     name[50] = {};//{'N','O','_','N','A','M','E'};
  uint32_t ip   = 0;
  uint8_t  mac[6] = {};
  bool     sfp;
  bool     link;

  bool     valid = false;

  int      mcGrps = 0;
  //  uint32_t mcIp[EKA_MAX_UDP_SESSIONS_PER_CORE] = {};

  uint64_t totalRxBytes = 0;
  uint64_t totalRxPkts = 0;
  uint32_t currPPS = 0;
  uint32_t maxPPS  = 0;
  uint64_t currBPS = 0;
  uint64_t maxBPS = 0;
};

struct McChState {
  int          num = 0;
  EkaMcState   grpState[64]  = {};
  EkaHwMcState grpHwState[64] = {};
};

struct McState {
  int          totalNum    = 0;
  int          maxGrPerCh  = 0;
  McChState    chState[32] = {};
};


const char* emptyPrefix     = "                     ";
const char* prefixStrFormat = "%-20s ";
const char* colStringFormat = "| %20s ";
const char* colStringFormatGrn = GRN "| %20s " RESET;
const char* colStringFormatRed = RED "| %20s " RESET;
const char* colSmallNumFieldFormat  = "| %17s%3d ";
const char* colformat="|    %'-16ju  ";
const int   colLen = 22;

//################################################

inline uint64_t reg_read (uint32_t addr) {
    uint64_t value = -1;
    if (SN_ERR_SUCCESS != SN_ReadUserLogicRegister (devId, addr/8, &value))
      on_error("SN_Read returned smartnic error code : %d",SN_GetLastErrorCode());    
    return value;
}
//################################################

inline void reg_write(uint64_t addr, uint64_t val) {
  if (SN_ERR_SUCCESS != SN_WriteUserLogicRegister(devId, addr/8, val))
    on_error("SN_Write returned smartnic error code : %d",SN_GetLastErrorCode());
}

//################################################
int getSnIgmpCtx(McState* mcState, sc_multicast_subscription_t* hwIgmp) {
  if (hwIgmp == NULL) on_error("hwIgmp == NULL");
  memset(hwIgmp,0,sizeof(sc_multicast_subscription_t) * 8 * 64);

  int fd = SN_GetFileDescriptor(devId);
  if (fd < 0) on_error("fd = %d",fd);

  eka_ioctl_t __attribute__ ((aligned(0x1000))) state = {};

  state.cmd = EKA_GET_IGMP_STATE;
  state.paramA = (uint64_t)hwIgmp;

  int rc = ioctl(fd,SMARTNIC_EKALINE_DATA,&state);
  if (rc != 0) on_error("ioctl failed: rc = %d",rc);

  /* for (auto chId = 0; chId < 32; chId++) { */
  /*   mcState->chState[chId].num = 0; */
  /* } */

  for (auto i = 0; i < 512; i++) {
    if (hwIgmp[i].group_address == 0 && hwIgmp[i].ip_port_number == 0) continue;
    printf ("%3d: pos=%3u, lane=%u, ch=%2u, %s:%u\n",
	    i,
	    hwIgmp[i].positionIndex,
	    hwIgmp[i].lane,
	    hwIgmp[i].channel,
	    EKA_IP2STR(be32toh(hwIgmp[i].group_address)),
	    hwIgmp[i].ip_port_number
	    );

    /* int chId = hwIgmp[i].channel - 32; */
    /* if (chId < 0 || chId > 31) continue; //on_error("chId=%d,hwIgmp[%d].channel=%u",chId,i,hwIgmp[i].channel); */

    /* int grId = hwIgmp[i].positionIndex; */
    /* if (grId < 0 || grId > 63) on_error("grId=%d,wIgmp[%d].positionIndex=%u",grId,i,hwIgmp[i].positionIndex); */

    /* if (hwIgmp[i].group_address == 0) continue; */

    /* mcState->chState[chId].grpHwState[grId].coreId = hwIgmp[i].lane; */
    /* mcState->chState[chId].grpHwState[grId].ip     = be32toh(hwIgmp[i].group_address); */
    /* mcState->chState[chId].grpHwState[grId].port   = hwIgmp[i].ip_port_number; */

    /* mcState->chState[chId].num ++; */
  }


  /* for (auto chId = 0; chId < 32; chId++) { */
  /*   if (mcState->chState[chId].num == 0) continue; */
    
  /* } */
  return 0;
}



//################################################
void       getIpMac_ioctl(uint8_t lane, IfParams* params) {
  char          buf[1024];
  struct ifconf ifc;
  struct ifreq *ifr;
  int           sck, nInterfaces;

  std::string portNameFETH   = std::string("feth")   + std::to_string(lane);
  std::string portNameFPGA0  = std::string("fpga0_") + std::to_string(lane);

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
    if (strncmp(item->ifr_name,portNameFETH.c_str(),strlen(portNameFETH.c_str())) != 0 &&
	strncmp(item->ifr_name,portNameFPGA0.c_str(),strlen(portNameFPGA0.c_str())) != 0) continue;

    strcpy(params->name,item->ifr_name);

    params->ip = ((struct sockaddr_in *)&item->ifr_addr)->sin_addr.s_addr;
    /* extracting the pre-configured MACSA  */
    struct ifreq s;
    strcpy(s.ifr_name,item->ifr_name);

    if (ioctl(sck, SIOCGIFHWADDR, &s) != 0) on_error("%s: ioctl(fd, SIOCGIFHWADDR, &s) != 0) -> ",__func__);
    memcpy(params->mac,s.ifr_addr.sa_data,6);
    break;
  }
  close (sck);

  SN_StatusInfo statusInfo;
  int rc = SN_GetStatusInfo(devId, &statusInfo);
  if (rc != SN_ERR_SUCCESS) on_error("Failed to get status info, error code %d",rc);
  SN_LinkStatus *s = (SN_LinkStatus *) &statusInfo.LinkStatus[lane];
  params->sfp  = s->SFP_Present;
  params->link = s->Link;
  return;
}
//################################################

int getNwParams(IfParams coreParams[NUM_OF_CORES]) {
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    char          buf[1024];
    struct ifconf ifc;
    struct ifreq *ifr;
    int           sck, nInterfaces;

    std::string portNameFETH   = std::string("feth")   + std::to_string(coreId);
    std::string portNameFPGA0  = std::string("fpga0_") + std::to_string(coreId);

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
      if (strncmp(item->ifr_name,portNameFETH.c_str(),strlen(portNameFETH.c_str())) != 0 &&
	  strncmp(item->ifr_name,portNameFPGA0.c_str(),strlen(portNameFPGA0.c_str())) != 0) continue;

      strcpy(coreParams[coreId].name,item->ifr_name);

      coreParams[coreId].ip = ((struct sockaddr_in *)&item->ifr_addr)->sin_addr.s_addr;
      /* extracting the pre-configured MACSA  */
      struct ifreq s;
      strcpy(s.ifr_name,item->ifr_name);

      if (ioctl(sck, SIOCGIFHWADDR, &s) != 0) on_error("%s: ioctl(fd, SIOCGIFHWADDR, &s) != 0) -> ",__func__);
      memcpy(coreParams[coreId].mac,s.ifr_addr.sa_data,6);
      coreParams[coreId].valid = true;
      break;
    }
    close (sck);

    SN_StatusInfo statusInfo;
    int rc = SN_GetStatusInfo(devId, &statusInfo);
    if (rc != SN_ERR_SUCCESS) on_error("Failed to get status info, error code %d",rc);
    SN_LinkStatus *s = (SN_LinkStatus *) &statusInfo.LinkStatus[coreId];
    coreParams[coreId].sfp  = s->SFP_Present;
    coreParams[coreId].link = s->Link;
  }
  return 0;
}

//################################################
void checkVer() {
  uint64_t swVer = reg_read(SCRPAD_SW_VER);
  if ((swVer & 0xFFFFFFFF00000000) != EKA_CORRECT_SW_VER) {
    fprintf(stderr,RED "%s: current SW Version 0x%jx is not supported by %s\n" RESET,__func__,swVer,__FILE__);
    //    exit(1);
  }
  //  TEST_LOG("swVer 0x%016jx is OK",swVer);
}
//################################################
int cleanMcState() {
  for (auto chId = 0; chId < 32; chId++) {
    uint64_t chBaseAddr = SCRPAD_MC_STATE_BASE + chId * MAX_MC_GROUPS_PER_UDP_CH * sizeof(EkaMcState);
    for (auto grId = 0; grId < 64; grId++) {
      uint64_t addr = chBaseAddr + grId * sizeof(EkaMcState);
      reg_write(addr,0);
    }
  }
  return 0;
}


//################################################

int main(int argc, char *argv[]) {
  devId = SN_OpenDevice(NULL, NULL);
  if (devId == NULL) on_error ("Cannot open FiberBlaze device. Is driver loaded?");
  IfParams coreParams[NUM_OF_CORES] = {};
  McState mcState = {};

  sc_multicast_subscription_t* hwIgmp = (sc_multicast_subscription_t*)malloc(sizeof(sc_multicast_subscription_t) * 8 * 64);
  if (hwIgmp == NULL) on_error("malloc failed");


  /* ----------------------------------------- */
  checkVer();
  /* ----------------------------------------- */
  cleanMcState();
  /* ----------------------------------------- */
  getNwParams(coreParams);
  /* ----------------------------------------- */
  getSnIgmpCtx(&mcState,hwIgmp);
  /* ----------------------------------------- */

    
  SN_CloseDevice(devId);
  exit(0);


  free(hwIgmp);
  return 0;
}

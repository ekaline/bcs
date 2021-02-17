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
  int          num            = 0;  // num of "logical" groups at this Channel
  int          currHwGr       = 0;
  EkaMcState   grpState[64]   = {};
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
  state.nif_num = 0;
  state.session_num = 0;
  state.wcattr.bar0_pa = (uint64_t)hwIgmp;

  int rc = ioctl(fd,SMARTNIC_EKALINE_DATA,&state);
  if (rc != 0) on_error("ioctl failed: rc = %d",rc);

  for (auto i = 0; i < 32; i++) {
    mcState->chState[i].currHwGr = 0;
  }

  for (auto i = 0; i < 512; i++) {
    int chId = hwIgmp[i].channel - 32;
    if ((hwIgmp[i].group_address != 0) && (chId < 0 || chId > 31)) 
      on_error("chId=%d,hwIgmp[%d].channel=%u",chId,i,hwIgmp[i].channel);
    if (hwIgmp[i].group_address == 0) continue;

    int grId = mcState->chState[chId].currHwGr;
    if (grId < 0 || grId > 63) 
      on_error("chId=%d,grId=%d,hwIgmp[%d].positionIndex=%u,group_address=0x%08x",
	       chId,grId,i,hwIgmp[i].positionIndex,hwIgmp[i].group_address);

    
    mcState->chState[chId].grpHwState[grId].coreId        = hwIgmp[i].lane;
    mcState->chState[chId].grpHwState[grId].ip            = be32toh(hwIgmp[i].group_address);
    mcState->chState[chId].grpHwState[grId].port          = hwIgmp[i].ip_port_number;
    mcState->chState[chId].grpHwState[grId].positionIndex = hwIgmp[i].positionIndex;
    mcState->chState[chId].currHwGr++;
  }

  return 0;
}



//################################################
void       getIpMac_ioctl(uint8_t lane, IfParams* params) {
  char          buf[1024];
  struct ifconf ifc;
  struct ifreq *ifr;
  int           sck, nInterfaces;

  std::string portNameFETH   = std::string("feth")   + std::to_string(lane);
  std::string portNameFPGA0_ = std::string("fpfa0_") + std::to_string(lane);

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
	strncmp(item->ifr_name,portNameFPGA0_.c_str(),strlen(portNameFPGA0_.c_str())) != 0) continue;

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
    std::string portNameFPGA0_ = std::string("fpga0_") + std::to_string(coreId);

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
	  strncmp(item->ifr_name,portNameFPGA0_.c_str(),strlen(portNameFPGA0_.c_str())) != 0) continue;

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
void printTime() {
  uint64_t rt_counter            = reg_read(ADDR_RT_COUNTER);
  uint64_t opendev_counter       = reg_read(ADDR_OPENDEV_COUNTER);
  uint64_t epoch_counter         = reg_read(FPGA_RT_CNTR);
  //  uint64_t var_nw_general_conf	 = reg_read(ADDR_NW_GENERAL_CONF);
  uint64_t var_sw_stats_zero	 = reg_read(ADDR_SW_STATS_ZERO);

  uint64_t seconds = rt_counter * (1000.0/FREQUENCY) / 1000000000;
  uint64_t epcoh_seconds = epoch_counter * (1000.0/FREQUENCY) / 1000000000;
  time_t raw_time = (time_t) epcoh_seconds;
  struct tm *timeinfo = localtime (&raw_time);
  char result[100];
  strftime(result, sizeof(result), "%a, %d %b %Y, %X", timeinfo);
  printf("EFH HW time: \t\t %s\n",result);
  printf("Last Reset Time: \t %dd:%dh:%dm:%ds\n",
	 (int)seconds/86400,(int)(seconds%86400)/3600,(int)(seconds%3600)/60,(int)seconds%60
	 );

  seconds = opendev_counter * (1000.0/FREQUENCY) / 1000000000;
  printf("Open Dev Time: \t\t %dd:%dh:%dm:%ds (%s)\n",
	 (int)seconds/86400,(int)(seconds%86400)/3600,(int)(seconds%3600)/60,(int)seconds%60,
	 ((var_sw_stats_zero>>63)&0x1) ? GRN "OPENED" RESET : RED "CLOSED" RESET
	 );
}
//################################################
void printExceptions() {
  uint64_t var_global_shadow     = reg_read(ADDR_INTERRUPT_SHADOW_RO);
  char* is_exception= (var_global_shadow) ? (char*) RED "YES, run sn_exceptions" RESET : (char*) "--";
  printf("Exceptions:\t\t %s\n",is_exception);
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
int getMcState(McState* state, IfParams coreParams[NUM_OF_CORES]) {
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++)
    coreParams[coreId].mcGrps = 0;

  state->totalNum = 0;
  state->maxGrPerCh = 0;
  for (auto chId = 0; chId < 32; chId++) {
    state->chState[chId].num = 0;
    uint64_t chBaseAddr = SCRPAD_MC_STATE_BASE + chId * MAX_MC_GROUPS_PER_UDP_CH * sizeof(EkaMcState);
    for (auto grId = 0; grId < 64; grId++) {
      uint64_t addr = chBaseAddr + grId * sizeof(EkaMcState);

      uint64_t* pData = (uint64_t*)&state->chState[chId].grpState[grId];

      *pData++ = reg_read(addr);
      *pData++ = reg_read(addr+8);
      //      *(uint64_t*)&state->chState[chId].grpState[grId] = reg_read(addr);
      if (state->chState[chId].grpState[grId].ip == 0) continue;
      state->chState[chId].num++;
      state->totalNum++;
      uint8_t coreId = state->chState[chId].grpState[grId].coreId;
      coreParams[coreId].mcGrps++;
    }
    if (state->chState[chId].num > state->maxGrPerCh)
      state->maxGrPerCh = state->chState[chId].num;
  }
  return 0;
}
//################################################
int printMcGroups(McState* state) {
  if (state->totalNum == 0) return 0;
  printf("\n");
  printf("   ");

  for (auto chId = 0; chId < 32; chId++) {
    if (state->chState[chId].num == 0) continue;
    printf("                   Ch%2d : Core %d, MC %2d                 |",
	   chId,
	   state->chState[chId].grpState[0].coreId,
	   state->chState[chId].num);
  }
  printf("\n");

  for (auto grId = 0; grId < state->maxGrPerCh; grId++) {
    printf ("%2d ",grId);
    for (auto chId = 0; chId < 32; chId++) {
      if (state->chState[chId].num == 0) continue;
      if (state->chState[chId].grpState[grId].ip == 0) {
	printf ("%56s|"," ");
	continue;
      }

      //      const char* fmt = "%16s:%5u (%16s:%5u) %8ju|";
      const char* coloredFmt = 
	state->chState[chId].grpState[grId].ip   == state->chState[chId].grpHwState[grId].ip &&
	state->chState[chId].grpState[grId].port == state->chState[chId].grpHwState[grId].port ?
	"%16s:%5u (%16s:%5u) %8ju|" : RED "%16s:%5u (%16s:%5u) %8ju|" RESET;
      printf (coloredFmt,
	      EKA_IP2STR(state->chState[chId].grpState[grId].ip),
	      state->chState[chId].grpState[grId].port,
	      EKA_IP2STR(state->chState[chId].grpHwState[grId].ip),
	      state->chState[chId].grpHwState[grId].port,
	      state->chState[chId].grpState[grId].pktCnt
	      );
    }
    printf("\n");
  }
  return 0;
}

//################################################

int printHeader(IfParams coreParams[NUM_OF_CORES]) {
  printf("\n");
  printf("%s",emptyPrefix);
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;

    std::string nameStr = std::string(coreParams[coreId].name) + "       ";
    printf (colStringFormat,nameStr.c_str());
  }
  printf("\n");
  /* ----------------------------------------- */
  printf("%s",std::string(strlen(emptyPrefix),'-').c_str());
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    printf("+%s", std::string(colLen,'-').c_str());
  }
  printf("\n");
  /* ----------------------------------------- */

  //  printf("%s",emptyPrefix);
  printf (prefixStrFormat,"Link status");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;

    if (coreParams[coreId].sfp && coreParams[coreId].link) {
      printf (colStringFormat,"SFP: YES, LINK: YES");
    } else {
      std::string sfpStr = coreParams[coreId].sfp  ? std::string("SFP: YES, ")  : std::string("SFP: NO, ");
      std::string lnkStr = coreParams[coreId].link ? std::string("LINK: YES ") : std::string("LINK: NO ");
      printf (colStringFormatRed,(sfpStr + lnkStr).c_str());
    }
  }
  printf("\n");
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf (prefixStrFormat,"IP");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    if (coreParams[coreId].ip == 0) printf(colStringFormatRed,"NO IP SET  ");
    else printf (colStringFormat,EKA_IP2STR(coreParams[coreId].ip));
  }
  printf("\n");
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf (prefixStrFormat,"MAC");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;

    if (coreParams[coreId].mac == 0) printf(colStringFormatRed,"NO MAC SET  ");
    else printf (colStringFormat,EKA_MAC2STR(coreParams[coreId].mac));
  }
  printf("\n");
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf (prefixStrFormat,"Joined MC groups");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;

    //    printf(colSmallNumFieldFormat,"",coreParams[coreId].mcGrps);
    printf (colformat,coreParams[coreId].mcGrps);

  }
  printf("\n");
  /* ----------------------------------------- */

  return 0;
}

//################################################

int getCurrTraffic(IfParams coreParams[NUM_OF_CORES]) {
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;

    uint64_t pps = reg_read(ADDR_STATS_RX_PPS + coreId * 0x1000);
    coreParams[coreId].currPPS = pps & 0xFFFFFFFF;
    coreParams[coreId].maxPPS  = (pps >> 32) & 0xFFFFFFFF;
    coreParams[coreId].totalRxBytes = reg_read(ADDR_STATS_RX_BYTES_TOT  + coreId * 0x1000);
    coreParams[coreId].totalRxPkts  = reg_read(ADDR_STATS_RX_PKTS_TOT   + coreId * 0x1000);
    coreParams[coreId].currBPS      = reg_read(ADDR_STATS_RX_BPS_CURR   + coreId * 0x1000);
    coreParams[coreId].maxBPS       = reg_read(ADDR_STATS_RX_BPS_MAX    + coreId * 0x1000);
  }

  return 0;
}
//################################################

int printCurrTraffic(IfParams coreParams[NUM_OF_CORES]) {

  printf (prefixStrFormat,"Current PPS");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    printf (colformat,coreParams[coreId].currPPS);
  }
  printf("\n");

  printf (prefixStrFormat,"MAX     PPS");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    printf (colformat,coreParams[coreId].maxPPS);
  }
  printf("\n");

  printf (prefixStrFormat,"Current Bit Rate");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    printf (colformat,coreParams[coreId].currBPS * 8);
  }
  printf("\n");

  printf (prefixStrFormat,"Max     Bit Rate");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    printf (colformat,coreParams[coreId].maxBPS * 8);
  }
  printf("\n");

  printf (prefixStrFormat,"Total   RX  Packets");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    printf (colformat,coreParams[coreId].totalRxPkts);
  }
  printf("\n");

  printf (prefixStrFormat,"Total   RX  Bytes");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (! coreParams[coreId].valid) continue;
    printf (colformat,coreParams[coreId].totalRxBytes);
  }
  printf("\n");

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
  uint64_t cnt = 0;
  while (1) {
    printf("\e[1;1H\e[2J"); //	system("clear");
    /* ----------------------------------------- */
    printTime();
    /* ----------------------------------------- */
    printExceptions();
    /* ----------------------------------------- */
    getMcState(&mcState,coreParams);
    /* ----------------------------------------- */
    getSnIgmpCtx(&mcState,hwIgmp);
    /* ----------------------------------------- */
    getCurrTraffic(coreParams);
    /* ----------------------------------------- */
    printHeader(coreParams);
    /* ----------------------------------------- */
    printCurrTraffic(coreParams);
    /* ----------------------------------------- */
    printMcGroups(&mcState);
    /* ----------------------------------------- */
    if (++cnt % 5 == 0) {
      cleanMcState();
      sleep (1);
    }
    sleep(1);
  }
  SN_CloseDevice(devId);
  exit(0);


  free(hwIgmp);
  return 0;
}

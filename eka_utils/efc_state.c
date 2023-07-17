#include <inttypes.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "Efc.h"
#include "EfcMsgs.h"
#include "EkaHwCaps.h"
#include "EkaHwExceptionsDecode.h"
#include "EkaMcState.h"
#include "ctls.h"
#include "eka.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"
#include "eka_sn_addr_space.h"
#include "smartnic.h"

// #define NUM_OF_CORES EKA_MAX_CORES
#define NUM_OF_CORES 4
#define FREQUENCY EKA_FPGA_FREQUENCY

#define MASK32 0xffffffff

SN_DeviceId devId;

struct IfParams {
  char name[50] = {}; //{'N','O','_','N','A','M','E'};
  uint32_t ip = 0;
  uint8_t mac[6] = {};
  bool sfp;
  bool link;

  bool valid = false;

  int mcGrps = 0;
  //  uint32_t mcIp[EKA_MAX_UDP_SESSIONS_PER_CORE] = {};

  uint64_t totalRxBytes = 0;
  uint64_t totalRxPkts = 0;
  uint32_t currRxPPS = 0;
  uint32_t maxRxPPS = 0;
  uint64_t currRxBPS = 0;
  uint64_t maxRxBPS = 0;
  uint64_t droppedPkts = 0;

  uint64_t totalTxBytes = 0;
  uint64_t totalTxPkts = 0;
  uint32_t currTxPPS = 0;
  uint32_t maxTxPPS = 0;
  uint64_t currTxBPS = 0;
  uint64_t maxTxBPS = 0;

  bool hwParserEnable;
  bool hwSnifferEnable;
  uint8_t hwMACFilter[6] = {};
};

#define ADDR_P4_CONT_COUNTER1 0xf0340
#define ADDR_P4_CONT_COUNTER3 0xf0350

struct CommonState {
  bool armed = false;
  uint32_t arm_ver = 0;
  bool reportOnly = false;
  bool killSwitch = false;
  bool tcpFilterEn = false;
};

struct EfcState {
  uint64_t strategyRuns = 0;
  uint64_t strategyPassed = 0;
  uint64_t ordersSubscribed = 0;
  uint64_t ordersUnsubscribed = 0;
  uint32_t totalSecs = 0;
  uint32_t subscribedSecs = 0;
  bool forceFire = false;
  bool forceFireUnsubscr = false;
  bool fatalDebug = false;
  CommonState commonState;
};

struct FastCancelState {
  uint64_t strategyRuns = 0;
  uint64_t strategyPassed = 0;
  CommonState commonState;
};

struct FastSweepState {
  uint64_t strategyRuns = 0;
  uint64_t strategyPassed = 0;
  CommonState commonState;
};

struct QEDState {
  uint64_t strategyRuns = 0;
  uint64_t strategyPassed = 0;
  CommonState commonState;
};

struct NewsState {
  uint64_t strategyRuns = 0;
  uint64_t strategyPassed = 0;
  CommonState commonState;
};

const char *emptyPrefix = "                     ";
const char *prefixStrFormat = "%-20s ";
const char *colStringFormat = "| %20s ";
const char *colStringFormatGrn = GRN "| %20s " RESET;
const char *colStringFormatRed = RED "| %20s " RESET;
const char *colSmallNumFieldFormat = "| %17s%3d ";
const char *colformat = "|    %'-16ju  ";
const char *colformats = "|    %'-16s  ";
const int colLen = 22;

// ################################################

inline uint64_t reg_read(uint32_t addr) {
  uint64_t value = -1;
  if (SN_ERR_SUCCESS !=
      SN_ReadUserLogicRegister(devId, addr / 8, &value))
    on_error("SN_Read returned smartnic error code : %d",
             SN_GetLastErrorCode());
  return value;
}
// ################################################

inline void getExceptions(EfcExceptionsReport *excpt,
                          uint8_t coresBitmap) {
  excpt->exceptionStatus.globalVector =
      reg_read(ADDR_INTERRUPT_SHADOW_RO);
  for (int i = 0; i < EFC_MAX_CORES; i++) {
    if (((0x01 << i) & 0xFF) & coresBitmap) {
      excpt->exceptionStatus.portVector[i] = reg_read(
          EKA_ADDR_INTERRUPT_0_SHADOW_RO + i * 0x1000);
    } else {
      excpt->exceptionStatus.portVector[i] = 0;
    }
  }
}
// ################################################

inline void reg_write(uint64_t addr, uint64_t val) {
  if (SN_ERR_SUCCESS !=
      SN_WriteUserLogicRegister(devId, addr / 8, val))
    on_error("SN_Write returned smartnic error code : %d",
             SN_GetLastErrorCode());
}

// ################################################
void getIpMac_ioctl(uint8_t lane, IfParams *params) {
  char buf[1024];
  struct ifconf ifc;
  struct ifreq *ifr;
  int sck, nInterfaces;

  std::string portNameFETH =
      std::string("feth") + std::to_string(lane);
  std::string portNameFPGA0 =
      std::string("fpfa0_") + std::to_string(lane);

  /* Get a socket handle. */
  sck = socket(AF_INET, SOCK_DGRAM, 0);
  if (sck < 0)
    on_error(
        "%s: failed on socket(AF_INET, SOCK_DGRAM, 0) -> ",
        __func__);

  /* Query available interfaces. */
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;
  if (ioctl(sck, SIOCGIFCONF, &ifc) < 0)
    on_error(
        "%s: failed on ioctl(sck, SIOCGIFCONF, &ifc)  -> ",
        __func__);

  /* Iterate through the list of interfaces. */
  ifr = ifc.ifc_req;
  nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

  for (int i = 0; i < nInterfaces; i++) {
    struct ifreq *item = &ifr[i];
    if (strncmp(item->ifr_name, portNameFETH.c_str(),
                strlen(portNameFETH.c_str())) != 0 &&
        strncmp(item->ifr_name, portNameFPGA0.c_str(),
                strlen(portNameFPGA0.c_str())) != 0)
      continue;

    strcpy(params->name, item->ifr_name);

    params->ip = ((struct sockaddr_in *)&item->ifr_addr)
                     ->sin_addr.s_addr;
    /* extracting the pre-configured MACSA  */
    struct ifreq s;
    strcpy(s.ifr_name, item->ifr_name);

    if (ioctl(sck, SIOCGIFHWADDR, &s) != 0)
      on_error("%s: ioctl(fd, SIOCGIFHWADDR, &s) != 0) -> ",
               __func__);
    memcpy(params->mac, s.ifr_addr.sa_data, 6);
    break;
  }
  close(sck);

  SN_StatusInfo statusInfo;
  int rc = SN_GetStatusInfo(devId, &statusInfo);
  if (rc != SN_ERR_SUCCESS)
    on_error("Failed to get status info, error code %d",
             rc);
  SN_LinkStatus *s =
      (SN_LinkStatus *)&statusInfo.LinkStatus[lane];
  params->sfp = s->SFP_Present;
  params->link = s->Link;
  return;
}
// ################################################

int getNwParams(IfParams coreParams[NUM_OF_CORES]) {
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    char buf[1024];
    struct ifconf ifc;
    struct ifreq *ifr;
    int sck, nInterfaces;

    std::string portNameFETH =
        std::string("feth") + std::to_string(coreId);
    std::string portNameFPGA0 =
        std::string("fpga0_") + std::to_string(coreId);

    /* Get a socket handle. */
    sck = socket(AF_INET, SOCK_DGRAM, 0);
    if (sck < 0)
      on_error("%s: failed on socket(AF_INET, SOCK_DGRAM, "
               "0) -> ",
               __func__);

    /* Query available interfaces. */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sck, SIOCGIFCONF, &ifc) < 0)
      on_error("%s: failed on ioctl(sck, SIOCGIFCONF, "
               "&ifc)  -> ",
               __func__);

    /* Iterate through the list of interfaces. */
    ifr = ifc.ifc_req;
    nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

    for (int i = 0; i < nInterfaces; i++) {
      struct ifreq *item = &ifr[i];
      if (strncmp(item->ifr_name, portNameFETH.c_str(),
                  strlen(portNameFETH.c_str())) != 0 &&
          strncmp(item->ifr_name, portNameFPGA0.c_str(),
                  strlen(portNameFPGA0.c_str())) != 0)
        continue;

      strcpy(coreParams[coreId].name, item->ifr_name);

      coreParams[coreId].ip =
          ((struct sockaddr_in *)&item->ifr_addr)
              ->sin_addr.s_addr;
      /* extracting the pre-configured MACSA  */
      struct ifreq s;
      strcpy(s.ifr_name, item->ifr_name);

      if (ioctl(sck, SIOCGIFHWADDR, &s) != 0)
        on_error(
            "%s: ioctl(fd, SIOCGIFHWADDR, &s) != 0) -> ",
            __func__);
      memcpy(coreParams[coreId].mac, s.ifr_addr.sa_data, 6);
      coreParams[coreId].valid = true;
      break;
    }
    close(sck);

    SN_StatusInfo statusInfo;
    int rc = SN_GetStatusInfo(devId, &statusInfo);
    if (rc != SN_ERR_SUCCESS)
      on_error("Failed to get status info, error code %d",
               rc);
    SN_LinkStatus *s =
        (SN_LinkStatus *)&statusInfo.LinkStatus[coreId];
    coreParams[coreId].sfp = s->SFP_Present;
    coreParams[coreId].link = s->Link;
  }
  return 0;
}
// ################################################
void printTime() {
  uint64_t rt_counter = reg_read(ADDR_RT_COUNTER);
  uint64_t opendev_counter = reg_read(ADDR_OPENDEV_COUNTER);
  uint64_t epoch_counter = reg_read(FPGA_RT_CNTR);
  //  uint64_t var_nw_general_conf	 =
  //  reg_read(ADDR_NW_GENERAL_CONF);
  uint64_t var_sw_stats_zero = reg_read(ADDR_SW_STATS_ZERO);

  uint64_t seconds =
      rt_counter * (1000.0 / FREQUENCY) / 1000000000;
  uint64_t epcoh_seconds =
      epoch_counter * (1000.0 / FREQUENCY) / 1000000000;
  time_t raw_time = (time_t)epcoh_seconds;
  struct tm *timeinfo = localtime(&raw_time);
  char result[100];
  strftime(result, sizeof(result), "%a, %d %b %Y, %X",
           timeinfo);
  printf("EFH HW time: \t\t %s\n", result);
  printf("Last Reset Time: \t %dd:%dh:%dm:%ds\n",
         (int)seconds / 86400,
         (int)(seconds % 86400) / 3600,
         (int)(seconds % 3600) / 60, (int)seconds % 60);

  seconds =
      opendev_counter * (1000.0 / FREQUENCY) / 1000000000;
  printf("Open Dev Time: \t\t %dd:%dh:%dm:%ds (%s)\n",
         (int)seconds / 86400,
         (int)(seconds % 86400) / 3600,
         (int)(seconds % 3600) / 60, (int)seconds % 60,
         ((var_sw_stats_zero >> 63) & 0x1)
             ? GRN "EFC/EXC OPENED" RESET
             : RED "EFC/EXC CLOSED" RESET);
}
// ################################################
static void checkVer() {
  uint64_t swVer = reg_read(SCRPAD_SW_VER);
  if ((swVer & 0xFFFFFFFF00000000) != EKA_CORRECT_SW_VER) {
    fprintf(stderr,
            RED "%s: current SW Version 0x%jx is not "
                "supported by %s\n" RESET,
            __func__, swVer, __FILE__);
    //    exit(1);
  }
  //  TEST_LOG("swVer 0x%016jx is OK",swVer);
}
// ################################################

// ################################################
int printLineSeparator(IfParams coreParams[NUM_OF_CORES],
                       char sep, char s) {
  printf("%s", std::string(strlen(emptyPrefix), s).c_str());
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf("%c%s", sep, std::string(colLen, s).c_str());
  }
  printf("\n");
  return 0;
}
// ################################################

int printHeader(IfParams coreParams[NUM_OF_CORES],
                EfcState *pEfcState,
                EkaHwCaps *pEkaHwCaps) {
  printf("\n");
  printf("%s", emptyPrefix);
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;

    std::string nameStr =
        std::string(coreParams[coreId].name) + "       ";
    printf(colStringFormat, nameStr.c_str());
  }
  printf("\n");
  /* ----------------------------------------- */
  /* printf("%s",std::string(strlen(emptyPrefix),'-').c_str());
   */
  /* for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++)
   * { */
  /*   if (! coreParams[coreId].valid) continue; */
  /*   printf("+%s", std::string(colLen,'-').c_str()); */
  /* } */
  /* printf("\n"); */
  /* ----------------------------------------- */
  printLineSeparator(coreParams, '+', '-');
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "Link status");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;

    if (coreParams[coreId].sfp && coreParams[coreId].link) {
      printf(colStringFormat, "SFP: YES, LINK: YES");
    } else {
      std::string sfpStr = coreParams[coreId].sfp
                               ? std::string("SFP: YES, ")
                               : std::string("SFP: NO, ");
      std::string lnkStr = coreParams[coreId].link
                               ? std::string("LINK: YES ")
                               : std::string("LINK: NO ");
      printf(colStringFormatRed, (sfpStr + lnkStr).c_str());
    }
  }
  printf("\n");
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "IP");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    if (coreParams[coreId].ip == 0)
      printf(colStringFormatRed, "NO IP SET  ");
    else
      printf(colStringFormat,
             EKA_IP2STR(coreParams[coreId].ip));
  }
  printf("\n");
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "MAC");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;

    if (coreParams[coreId].mac == 0)
      printf(colStringFormatRed, "NO MAC SET  ");
    else
      printf(colStringFormat,
             EKA_MAC2STR(coreParams[coreId].mac));
  }
  printf("\n");
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "Joined MC groups");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;

    //    printf(colSmallNumFieldFormat,"",coreParams[coreId].mcGrps);
    printf(colformat, coreParams[coreId].mcGrps);
  }
  printf("\n");

  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "HW parser type");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    //    printf(colSmallNumFieldFormat,"",coreParams[coreId].mcGrps);
    //    printf
    //    (colformat,coreParams[coreId].hwParserEnable);
    if (coreId < 2) // TBD check hw md cores
      printf(colformats,
             EKA_FEED2STRING(
                 ((pEkaHwCaps->hwCaps.version.parser >>
                   coreId * 4) &
                  0xF)));
    else
      printf(colformats, "");
  }
  printf("\n");

  /* ----------------------------------------- */
  printf(prefixStrFormat, "HW parser enable");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    //    printf(colSmallNumFieldFormat,"",coreParams[coreId].mcGrps);
    printf(colformat, coreParams[coreId].hwParserEnable);
  }
  printf("\n");

  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "HW sniffer");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    //    printf(colSmallNumFieldFormat,"",coreParams[coreId].mcGrps);
    printf(colformat, coreParams[coreId].hwSnifferEnable);
  }
  printf("\n");

  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "HW TCP RX");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    bool isZeroMac =
        ((coreParams[coreId].hwMACFilter[0] == 0) &&
         (coreParams[coreId].hwMACFilter[1] == 0) &&
         (coreParams[coreId].hwMACFilter[2] == 0) &&
         (coreParams[coreId].hwMACFilter[3] == 0) &&
         (coreParams[coreId].hwMACFilter[4] == 0) &&
         (coreParams[coreId].hwMACFilter[5] == 0));

    if (isZeroMac && pEfcState->commonState.tcpFilterEn)
      printf(colStringFormat, "on, but zero mac");
    else if (!isZeroMac &&
             pEfcState->commonState.tcpFilterEn)
      printf(colStringFormat,
             EKA_MAC2STR(coreParams[coreId].hwMACFilter));
    else if (isZeroMac &&
             !pEfcState->commonState.tcpFilterEn)
      printf(colStringFormat, "off, and zero mac");
    else
      printf(colStringFormat, "off, non-zero mac");
  }

  printf("\n");
  /* ----------------------------------------- */
  /* printf ("======================"); */
  /* for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++)
   * { */
  /*   if (! coreParams[coreId].valid) continue; */
  /*   printf ("======================="); */
  /* } */
  /* printf("\n"); */
  /* ----------------------------------------- */
  return 0;
}

// ################################################

int getCurrTraffic(IfParams coreParams[NUM_OF_CORES]) {
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;

    // RX
    uint64_t pps =
        reg_read(ADDR_STATS_RX_PPS + coreId * 0x1000);
    coreParams[coreId].currRxPPS = pps & 0xFFFFFFFF;
    coreParams[coreId].maxRxPPS = (pps >> 32) & 0xFFFFFFFF;
    coreParams[coreId].totalRxBytes =
        reg_read(ADDR_STATS_RX_BYTES_TOT + coreId * 0x1000);
    coreParams[coreId].totalRxPkts =
        reg_read(ADDR_STATS_RX_PKTS_TOT + coreId * 0x1000);
    coreParams[coreId].currRxBPS =
        reg_read(ADDR_STATS_RX_BPS_CURR + coreId * 0x1000);
    coreParams[coreId].maxRxBPS =
        reg_read(ADDR_STATS_RX_BPS_MAX + coreId * 0x1000);
    coreParams[coreId].droppedPkts =
        reg_read(EFC_DROPPED_PKTS + coreId * 0x1000) &
        0xFFFFFFFF;
    // TX
    pps = reg_read(ADDR_STATS_TX_PPS + coreId * 0x1000);
    coreParams[coreId].currTxPPS = pps & 0xFFFFFFFF;
    coreParams[coreId].maxTxPPS = (pps >> 32) & 0xFFFFFFFF;
    coreParams[coreId].totalTxBytes =
        reg_read(ADDR_STATS_TX_BYTES_TOT + coreId * 0x1000);
    coreParams[coreId].totalTxPkts =
        reg_read(ADDR_STATS_TX_PKTS_TOT + coreId * 0x1000);
    coreParams[coreId].currTxBPS =
        reg_read(ADDR_STATS_TX_BPS_CURR + coreId * 0x1000);
    coreParams[coreId].maxTxBPS =
        reg_read(ADDR_STATS_TX_BPS_MAX + coreId * 0x1000);
  }

  return 0;
}
// ################################################

int getCurrHWEnables(IfParams coreParams[NUM_OF_CORES]) {

  uint64_t port_enable = reg_read(0xf0020);

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    coreParams[coreId].hwParserEnable =
        (port_enable >> coreId) & 0x1;
    coreParams[coreId].hwSnifferEnable =
        (reg_read(0xf0360) >> coreId) & 0x1;

    uint64_t hw_tcp_mac_filter =
        be64toh(reg_read(0xe0200 + coreId * 0x1000) << 16);
    memcpy(coreParams[coreId].hwMACFilter,
           (void *)&hw_tcp_mac_filter, 6);
  }

  return 0;
}
// ################################################

int printCurrRxTraffic(IfParams coreParams[NUM_OF_CORES]) {

  printf(prefixStrFormat, "Current RX PPS");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].currRxPPS);
  }
  printf("\n");

  printf(prefixStrFormat, "MAX     RX PPS");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].maxRxPPS);
  }
  printf("\n");

  printf(prefixStrFormat, "Current RX Bit Rate");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].currRxBPS * 8);
  }
  printf("\n");

  printf(prefixStrFormat, "MAX     RX Bit Rate");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].maxRxBPS * 8);
  }
  printf("\n");

  printf(prefixStrFormat, "Total   RX Packets");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].totalRxPkts);
  }
  printf("\n");

  printf(prefixStrFormat, "Total   RX Bytes");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].totalRxBytes);
  }
  printf("\n");

  printf(prefixStrFormat, "Ignored MD Packets");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].droppedPkts);
  }
  printf("\n");

  return 0;
}
// ################################################

int printCurrTxTraffic(IfParams coreParams[NUM_OF_CORES]) {

  printf(prefixStrFormat, "Current TX PPS");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].currTxPPS);
  }
  printf("\n");

  printf(prefixStrFormat, "MAX     TX PPS");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].maxTxPPS);
  }
  printf("\n");

  printf(prefixStrFormat, "Current TX Bit Rate");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].currTxBPS * 8);
  }
  printf("\n");

  printf(prefixStrFormat, "MAX     TX Bit Rate");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].maxTxBPS * 8);
  }
  printf("\n");

  printf(prefixStrFormat, "Total   TX Packets");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].totalTxPkts);
  }
  printf("\n");

  printf(prefixStrFormat, "Total   TX Bytes");
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].totalTxBytes);
  }
  printf("\n");

  return 0;
}
// ################################################
int getEfcState(EfcState *pEfcState) {

  uint64_t var_p4_cont_counter1 =
      reg_read(ADDR_P4_CONT_COUNTER1);
  uint64_t var_p4_cont_counter3 =
      reg_read(ADDR_P4_CONT_COUNTER3);
  uint64_t var_p4_general_conf =
      reg_read(ADDR_P4_GENERAL_CONF);
  uint64_t var_fatal_debug = reg_read(ADDR_FATAL_CONF);

  uint64_t subscrCnt = reg_read(SCRPAD_EFC_SUBSCR_CNT);
  pEfcState->totalSecs = (subscrCnt >> 32) & 0xFFFFFFFF;
  pEfcState->subscribedSecs = subscrCnt & 0xFFFFFFFF;

  pEfcState->strategyRuns =
      (var_p4_cont_counter1 >> 0) & MASK32;
  pEfcState->strategyPassed =
      (var_p4_cont_counter1 >> 32) & MASK32;
  pEfcState->ordersSubscribed =
      (var_p4_cont_counter3 >> 0) & MASK32;
  pEfcState->ordersUnsubscribed =
      (var_p4_cont_counter3 >> 32) & MASK32;

  uint64_t armReg = reg_read(P4_ARM_DISARM);
  pEfcState->commonState.armed = (armReg & 0x1) != 0;
  pEfcState->commonState.arm_ver =
      (armReg >> 32) & 0xFFFFFFFF;
  pEfcState->commonState.killSwitch =
      (reg_read(KILL_SWITCH) & 0x1) != 0;

  pEfcState->forceFire =
      (var_p4_general_conf & EKA_P4_ALWAYS_FIRE_BIT) != 0;
  pEfcState->forceFireUnsubscr =
      (var_p4_general_conf &
       EKA_P4_ALWAYS_FIRE_UNSUBS_BIT) != 0;
  pEfcState->commonState.reportOnly =
      (var_p4_general_conf & EKA_P4_REPORT_ONLY_BIT) != 0;
  pEfcState->fatalDebug = var_fatal_debug == 0xefa0beda;

  pEfcState->commonState.tcpFilterEn =
      (reg_read(0xf0020) >> 32 & 0x1) == 0;

  return 0;
}

// ################################################
int getFastCancelState(FastCancelState *pFastCancelState) {

  uint64_t var_fc_cont_counter1 = reg_read(0xf0800);

  uint64_t var_p4_general_conf =
      reg_read(ADDR_P4_GENERAL_CONF);

  pFastCancelState->strategyRuns =
      (var_fc_cont_counter1 >> 32) & MASK32;
  pFastCancelState->strategyPassed =
      (var_fc_cont_counter1 >> 0) & MASK32;

  pFastCancelState->commonState.reportOnly =
      (var_p4_general_conf & EKA_P4_REPORT_ONLY_BIT) != 0;

  uint64_t armReg = reg_read(NW_ARM_DISARM);
  pFastCancelState->commonState.armed = (armReg & 0x1) != 0;
  pFastCancelState->commonState.arm_ver =
      (armReg >> 32) & 0xFFFFFFFF;
  pFastCancelState->commonState.killSwitch =
      (reg_read(KILL_SWITCH) & 0x1) != 0;
  return 0;
}

// ################################################
int getFastSweepState(FastSweepState *pFastSweepState) {

  uint64_t var_fc_cont_counter1 = reg_read(0xf0810);

  uint64_t var_p4_general_conf =
      reg_read(ADDR_P4_GENERAL_CONF);

  pFastSweepState->strategyRuns =
      (var_fc_cont_counter1 >> 32) & MASK32;
  pFastSweepState->strategyPassed =
      (var_fc_cont_counter1 >> 0) & MASK32;

  pFastSweepState->commonState.reportOnly =
      (var_p4_general_conf & EKA_P4_REPORT_ONLY_BIT) != 0;

  uint64_t armReg = reg_read(NW_ARM_DISARM);
  pFastSweepState->commonState.armed = (armReg & 0x1) != 0;
  pFastSweepState->commonState.arm_ver =
      (armReg >> 32) & 0xFFFFFFFF;
  pFastSweepState->commonState.killSwitch =
      (reg_read(KILL_SWITCH) & 0x1) != 0;

  return 0;
}

// ################################################
int getQEDState(QEDState *pQEDState) {

  uint64_t var_fc_cont_counter1 = reg_read(0xf0818);

  uint64_t var_p4_general_conf =
      reg_read(ADDR_P4_GENERAL_CONF);

  pQEDState->strategyRuns =
      (var_fc_cont_counter1 >> 32) & MASK32;
  pQEDState->strategyPassed =
      (var_fc_cont_counter1 >> 0) & MASK32;

  pQEDState->commonState.reportOnly =
      (var_p4_general_conf & EKA_P4_REPORT_ONLY_BIT) != 0;

  uint64_t armReg = reg_read(NW_ARM_DISARM);
  pQEDState->commonState.armed = (armReg & 0x1) != 0;
  pQEDState->commonState.arm_ver =
      (armReg >> 32) & 0xFFFFFFFF;
  pQEDState->commonState.killSwitch =
      (reg_read(KILL_SWITCH) & 0x1) != 0;

  return 0;
}

// ################################################
int getNewsState(NewsState *pNewsState) {

  uint64_t var_news_cont_counter1 = reg_read(0xf0808);

  uint64_t var_p4_general_conf =
      reg_read(ADDR_P4_GENERAL_CONF);

  pNewsState->strategyRuns =
      (var_news_cont_counter1 >> 32) & MASK32;
  pNewsState->strategyPassed =
      (var_news_cont_counter1 >> 0) & MASK32;

  pNewsState->commonState.reportOnly =
      (var_p4_general_conf & EKA_P4_REPORT_ONLY_BIT) != 0;

  uint64_t armReg = reg_read(NW_ARM_DISARM);
  pNewsState->commonState.armed = (armReg & 0x1) != 0;
  pNewsState->commonState.arm_ver =
      (armReg >> 32) & 0xFFFFFFFF;
  pNewsState->commonState.killSwitch =
      (reg_read(KILL_SWITCH) & 0x1) != 0;

  return 0;
}

// ################################################
int printCommonState(CommonState *pCommonState) {

  if (pCommonState->killSwitch) {
    printf(RED "Fatal KILL SWITCH is turned ON!!! - reload "
               "driver is needed!!!\n\n" RESET);
  }

  if (pCommonState->arm_ver == EFC_HW_UNARMABLE) {
    printf(RED "\n\n!!! FPGA in fatal state. Can NOT be "
               "armed. Must reload driver !!!\n\n" RESET);
  } else {
    if (!pCommonState->armed) {
      printf(RED " UNARMED, (ver=%d)\n" RESET,
             pCommonState->arm_ver);
    } else {
      printf(GRN " ARMED, (ver=%d)\n" RESET,
             pCommonState->arm_ver);
    }
  }

  //  printf("\nReportOnly =
  //  %de-arming)\n\n",pCommonState->reportOnly);
  //  printf("ReportOnly
  //  :\t%ju\n",pCommonState->reportOnly);
  return 0;
}

// ################################################
int printEfcState(EfcState *pEfcState) {

  printf("\n-----------------------------------------------"
         "---------");
  printf("\nSecurity Hash Based Strategy (P4) : ");
  printCommonState(&pEfcState->commonState);
  printf("-------------------------------------------------"
         "-------\n");
  printf("ReportOnly            :\t%d\n",
         pEfcState->commonState.reportOnly);

  if (pEfcState->fatalDebug)
    printf(RED
           "WARNING: \'Fatal Debug\' is Active\n" RESET);

  if (pEfcState->forceFire) {
    printf(
        "Configurations: ForceFire               = %d "
        "(effective only if \'Fatal Debug\' is Active)\n",
        pEfcState->forceFire);
    printf(
        "\t\tForceFireOnUnsubscribed = %d (effective only "
        "if \'Fatal Debug\' and ForceFire are Active)\n",
        pEfcState->forceFireUnsubscr);
  }

  printf("\n-----------------------------------\n");
  printf("Subscription:\n");
  printf("Tried                 :\t%u\n",
         pEfcState->totalSecs);
  printf("Succeeded             :\t%u\n",
         pEfcState->subscribedSecs);
  printf("\n-----------------------------------\n");
  printf("Strategy:\n");
  printf("Subscribed   MD Orders:\t%ju\n",
         pEfcState->ordersSubscribed);
  printf("Unsubscribed MD Orders:\t%ju\n",
         pEfcState->ordersUnsubscribed);
  printf("Evaluated   strategies:\t%ju\n",
         pEfcState->strategyRuns);
  printf("Passed      strategies:\t%ju\n",
         pEfcState->strategyPassed);

  return 0;
}

// ################################################
int printFastCancelState(
    FastCancelState *pFastCancelState) {

  printf("\n-----------------------------------------------"
         "---------");
  printf("\nDirect Strategy : ");
  printCommonState(&pFastCancelState->commonState);
  printf("-------------------------------------------------"
         "-------\n");

  printf("Evaluated   strategies:\t%ju\n",
         pFastCancelState->strategyRuns);
  printf("Passed      strategies:\t%ju\n",
         pFastCancelState->strategyPassed);

  return 0;
}

// ################################################
int printFastSweepState(FastSweepState *pFastSweepState) {

  printf("\n-----------------------------------------------"
         "---------");
  printf("\nDirect Strategy : ");
  printCommonState(&pFastSweepState->commonState);
  printf("-------------------------------------------------"
         "-------\n");

  printf("Evaluated   strategies:\t%ju\n",
         pFastSweepState->strategyRuns);
  printf("Passed      strategies:\t%ju\n",
         pFastSweepState->strategyPassed);

  return 0;
}

// ################################################
int printQEDState(QEDState *pQEDState) {

  printf("\n-----------------------------------------------"
         "---------");
  printf("\nDirect Strategy : ");
  printCommonState(&pQEDState->commonState);
  printf("-------------------------------------------------"
         "-------\n");

  printf("Evaluated   strategies:\t%ju\n",
         pQEDState->strategyRuns);
  printf("Passed      strategies:\t%ju\n",
         pQEDState->strategyPassed);

  return 0;
}

// ################################################
int printNewsState(NewsState *pNewsState) {

  printf("\n-----------------------------------------------"
         "---------");
  printf("\nDirect Strategy : ");
  printCommonState(&pNewsState->commonState);
  printf("-------------------------------------------------"
         "-------\n");

  printf("Evaluated   strategies:\t%ju\n",
         pNewsState->strategyRuns);
  printf("Passed      strategies:\t%ju\n",
         pNewsState->strategyPassed);

  return 0;
}

// ################################################

int main(int argc, char *argv[]) {
  setlocale(
      LC_NUMERIC,
      "en_US"); // decimal point is represented by a period
                // (.), and the thousands separator is
                // represented by a comma (,)
  devId = SN_OpenDevice(NULL, NULL);
  if (devId == NULL)
    on_error(
        "Cannot open FiberBlaze device. Is driver loaded?");
  IfParams coreParams[NUM_OF_CORES] = {};
  EkaHwCaps *ekaHwCaps = new EkaHwCaps(devId);
  auto pEfcState = new EfcState;
  auto pFastCancelState = new FastCancelState;
  auto pFastSweepState = new FastSweepState;
  auto pQEDState = new QEDState;
  auto pNewsState = new NewsState;
  auto pEfcExceptionsReport = new EfcExceptionsReport;
  /* ----------------------------------------- */
  checkVer();
  /* ----------------------------------------- */
  getNwParams(coreParams);
  /* ----------------------------------------- */
  //  uint64_t cnt = 0;
  while (1) {
    getCurrTraffic(coreParams);
    getCurrHWEnables(coreParams);
    ekaHwCaps->refresh();
    getEfcState(pEfcState);
    getExceptions(
        pEfcExceptionsReport,
        ekaHwCaps->hwCaps.core.bitmap_tcp_cores |
            ekaHwCaps->hwCaps.core.bitmap_md_cores);
    /* ----------------------------------------- */

    for (auto coreId = 0; coreId < 2;
         coreId++) { // TBD md bitmap

      switch ((
          (ekaHwCaps->hwCaps.version.parser >> coreId * 4) &
          0xF)) {
      case 12:
        getQEDState(pQEDState);
        break;
      case 13:
        getFastSweepState(pFastSweepState);
        break;
      case 14:
        getNewsState(pNewsState);
        break;
      case 15:
        getFastCancelState(pFastCancelState);
        break;
      case 1:
      case 2:
        getEfcState(pEfcState);
        break;
      default:
        break;
      }
      /* ----------------------------------------- */
    }
    /* ----------------------------------------- */
    printf("\e[1;1H\e[2J"); //	system("clear");
    /* ----------------------------------------- */
    printTime();
    /* ----------------------------------------- */
    printHeader(coreParams, pEfcState, ekaHwCaps);
    /* ----------------------------------------- */
    printLineSeparator(coreParams, '+', '-');
    /* ----------------------------------------- */
    printCurrRxTraffic(coreParams);
    /* ----------------------------------------- */
    printLineSeparator(coreParams, '+', '-');
    /* ----------------------------------------- */
    printCurrTxTraffic(coreParams);
    /* ----------------------------------------- */
    printLineSeparator(coreParams, '+', '-');
    /* ----------------------------------------- */
    /* ----------------------------------------- */

    /* ----------------------------------------- */
    for (auto coreId = 0; coreId < 2;
         coreId++) { // TBD md bitmap

      switch ((
          (ekaHwCaps->hwCaps.version.parser >> coreId * 4) &
          0xF)) {
      case 12:
        printQEDState(pQEDState);
        break;
      case 13:
        printFastSweepState(pFastSweepState);
        break;
      case 14:
        printNewsState(pNewsState);
        break;
      case 15:
        printFastCancelState(pFastCancelState);
        break;
      case 1:
      case 2:
        printEfcState(pEfcState);
        break;
      default:
        break;
      }
      /* ----------------------------------------- */
    }

    /* ----------------------------------------- */
    char excptBuf[8192] = {};
    int decodeSize =
        ekaDecodeExceptions(excptBuf, pEfcExceptionsReport);
    if ((uint64_t)decodeSize > sizeof(excptBuf))
      on_error("decodeSize %d > sizeof(excptBuf) %jd",
               decodeSize, sizeof(excptBuf));
    printf("%s\n", excptBuf);
    /* ----------------------------------------- */
    sleep(1);
  }
  delete ekaHwCaps;
  delete pEfcState;
  delete pFastCancelState;
  delete pFastSweepState;
  delete pQEDState;
  delete pNewsState;
  delete pEfcExceptionsReport;
  SN_CloseDevice(devId);

  return 0;
}

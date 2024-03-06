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
#define NUM_OF_STRAT 16
#define FREQUENCY EKA_FPGA_FREQUENCY

#define MASK32 0xffffffff

#define S_P4 1
#define S_QED 12
#define S_SWEEP 13
#define S_NEWS 14
#define S_CANCEL 15

SN_DeviceId devId;

typedef struct __attribute__((packed)) {
  char     board_id[4];
  uint64_t ask_size;
  uint64_t bid_size;
  uint64_t ask_price;
  uint64_t bid_price;
} rf_bcs_single_prod_t;

typedef struct __attribute__((packed)) {
  uint64_t sell_order_update;  
  uint64_t buy_order_update;  
  uint64_t hedge_sell_price;  
  uint64_t hedge_buy_price;  
  uint64_t my_sell_price;  
  uint64_t my_buy_price;
  rf_bcs_single_prod_t quote;
  rf_bcs_single_prod_t base;
} rf_bcs_single_entry_t;

typedef struct __attribute__((packed)) {
  uint32_t replace_arm_thresh;
  uint32_t replace_arm_cnt;
  uint64_t clordid;
  rf_bcs_single_entry_t pair;
} rf_bcs_all_entry_t; 

typedef struct __attribute__((packed)) {
  uint64_t price;
  uint32_t size;
  uint32_t seq;
} rf_tob_entry_t;

typedef struct __attribute__((packed)) {
  rf_tob_entry_t bid;
  rf_tob_entry_t ask;
} rf_tob_total_t;


typedef struct __attribute__((packed)) {
  uint8_t buy:1;
  uint8_t sell:1;
  uint8_t pad8:6;
} arm_rep_per_side_t;

typedef struct __attribute__((packed)) {
	uint8_t            expected_version;
        arm_rep_per_side_t arm;
} arm_prod_unaligned_report_t; 

typedef struct __attribute__((packed)) {
        arm_prod_unaligned_report_t prod[16];
} arm_status_unaligned_report_t; 


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

  //  bool mirrorEnable;
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
  bool epmDump = false;
  CommonState commonState;
};

struct EurProdState {
  uint64_t totalSecs = 0;
  uint64_t bookSecs = 0;
  uint64_t secID[16];
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

struct StratState {
  EfcState p4;
  FastCancelState fastCancel;
  FastSweepState fastSweep;
  QEDState QED;
  NewsState news;
  EurProdState eur;
};

const char *emptyPrefix = "                     ";
const char *prefixStrFormat = "%-20s ";
const char *colStringFormat = "| %20s ";
const char *boardStringFormat = "| %-20.4s ";
const char *colStringFormatGrn = GRN "| %20s " RESET;
const char *colStringFormatRed = RED "| %20s " RESET;
const char *colSmallNumFieldFormat = "| %17s%3d ";
const char *colformat = "|    %'-16ju  ";
const char *colformats = "|    %'-16s  ";
const char *colformatsgrn = GRN "|    %'-16s  " RESET;
const char *colformatsred = RED "|    %'-16s  " RESET;
const char *bookStringFormat = "| %20s ";
const char *bookSideFormat = "| %7u@%-12ju ";
const char *prefixBookStrFormat = "%-12s %-8d";
const char *bookArmFormat = "| %9s %10s ";
const char *bookVersionFormat = "| %10u ";
const int colLen = 22;

bool active_strat[16] = {false};

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
  for (int i = 0; i < EKA_MAX_CORES; i++) {
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
int printStratLineSeparator(char sep, char s) {
  printf("%s", std::string(strlen(emptyPrefix), s).c_str());
  for (auto stratId = 0; stratId < NUM_OF_STRAT;
       stratId++) {
    if (!active_strat[stratId])
      continue;
    printf("%c%s", sep, std::string(colLen, s).c_str());
  }
  printf("\n");
  return 0;
}
// ################################################
int printBookLineSeparator(char sep, char s, int num) {
  printf("%s", std::string(strlen(emptyPrefix), s).c_str());
  for (auto i = 0; i < num;  i++) {
    printf("%c%s", sep, std::string(colLen, s).c_str());
  }
  printf("\n");
  return 0;
}

// ################################################
int printBookHeader() {
  printf("\n");
  printf("%s", emptyPrefix);
  printf(bookStringFormat, "SecurityID ");
  printf(bookStringFormat, "BoardID    ");
  printf(bookStringFormat, "Bid        ");
  printf(bookStringFormat, "Ask        ");
  //  printf(bookStringFormat, "Armed      ");
  //  printf(bookStringFormat, "Version    ");

  printf("\n");

  /* ----------------------------------------- */
  printBookLineSeparator( '+', '-', 4);
  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);

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

  auto parser_idx = 0;
  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    //    printf(colSmallNumFieldFormat,"",coreParams[coreId].mcGrps);
    //    printf
    //    (colformat,coreParams[coreId].hwParserEnable);
    if ((pEkaHwCaps->hwCaps.core.bitmap_md_cores >> coreId) & 0x1) {
      printf(colformats,
             EKA_FEED2STRING(
                 ((pEkaHwCaps->hwCaps.version.parser >> 
                   parser_idx*4) &
                  0xF)));
      parser_idx++;
    }
    else
      {
	printf(colformats, "-");
      }
  }
  printf("\n");

  /* ----------------------------------------- */
  printf(prefixStrFormat, "HW parser enable");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].hwParserEnable);
  }
  printf("\n");

  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  printf(prefixStrFormat, "HW sniffer");

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    if (!coreParams[coreId].valid)
      continue;
    printf(colformat, coreParams[coreId].hwSnifferEnable);
  }
  printf("\n");

  /* ----------------------------------------- */
  //  printf("%s",emptyPrefix);
  /* printf(prefixStrFormat, "HW mirror target"); */

  /* for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) { */
  /*   if (!coreParams[coreId].valid) */
  /*     continue; */
  /*   if (!((pEkaHwCaps->hwCaps.core.bitmap_mirror_cores >> */
  /*          coreId) & */
  /*         0x1)) { */
  /*     printf(colformats, "-"); */
  /*     continue; */
  /*   } */

  /*   if (!(coreParams[coreId].mirrorEnable)) */
  /*     printf(colformats, "off"); */
  /*   else { */
  /*     uint8_t mirrorTarget; */
  /*     if (coreId == 0) */
  /*       mirrorTarget = 3; */
  /*     else */
  /*       mirrorTarget = 0; */
  /*     std::string nameStr = */
  /*         std::string(coreParams[mirrorTarget].name) + */
  /*         "       "; */
  /*     printf(colformats, nameStr.c_str()); */
  /*   } */
  /* } */
  /* printf("\n"); */

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
int printStratHeader() {
  printf("\n");
  printf("%s", emptyPrefix);

  for (auto stratId = 0; stratId < NUM_OF_STRAT;
       stratId++) {
    if (!active_strat[stratId])
      continue;

    std::string nameStr =
        std::string(EKA_STRAT2STRING(stratId)) + " Strategy  ";
    //    printf(colStringFormat, nameStr.c_str());
    printf(colformats, nameStr.c_str());
  }
  printf("\n");
  printStratLineSeparator('+', '-');
  /* ----------------------------------------- */
  return 0;
}

// ################################################
int printProdHeader() {
  //  printf("%s", emptyPrefix);

  //  printf("\n");
  printStratLineSeparator('+', '-');
  /* ----------------------------------------- */
  return 0;
}

// ################################################
int printEurTOB(uint8_t prod_idx) {
  unsigned char tob_mem[64];
  unsigned char arm_mem[64];

  uint64_t *wrPtr;// = (uint64_t *)tob_mem;
  uint64_t *wrPtrArm;// = (uint64_t *)arm_mem;

  //hw lock
  uint64_t locked;
  do {
    locked = reg_read(0x72000); //1==locked
  }
  while (locked);

  wrPtrArm = (uint64_t *)arm_mem;

  for (auto j = 0; j < 4; j++)
    *wrPtrArm++ = reg_read(0x74000 + j * 8);
  
  wrPtr    = (uint64_t *)tob_mem;
    
  for (auto j = 0; j < 8; j++)
    *wrPtr++ = reg_read(0x73000 + prod_idx * 64 + j * 8);

  // hw unlock
  reg_write(0x72000, 0); 


  auto prodTOB{
      reinterpret_cast<const rf_tob_total_t *>(tob_mem)};

  auto armState{
      reinterpret_cast<const arm_status_unaligned_report_t *>(arm_mem)};

  printf(bookSideFormat, prodTOB->bid.size/10000, prodTOB->bid.price);
  printf(bookSideFormat, prodTOB->ask.size/10000, prodTOB->ask.price);

  printf("| ");
  if (armState->prod[prod_idx].arm.buy)
    printf(GRN "    BUY     " RESET);
  else
    printf(RED "    buy     " RESET);

  if (armState->prod[prod_idx].arm.sell)
    printf(GRN "    SELL " RESET);
  else
    printf(RED "    sell " RESET);

  printf(bookVersionFormat, armState->prod[prod_idx].expected_version);
  
  return 0;
}

// ################################################
int printTOB() {
  unsigned char tob_mem[4096];
  unsigned char arm_mem[64];

  uint64_t *wrPtr;// = (uint64_t *)tob_mem;
  uint64_t *wrPtrArm;// = (uint64_t *)arm_mem;

  wrPtrArm = (uint64_t *)arm_mem;

  for (auto j = 0; j < 4; j++)
    *wrPtrArm++ = reg_read(0x74000 + j * 8);
  
  wrPtr    = (uint64_t *)tob_mem;

  uint64_t *dstAddr = (uint64_t *)tob_mem;
  uint words2read = sizeof(rf_bcs_all_entry_t) / 8 + !!(sizeof(rf_bcs_all_entry_t) % 8);
  for (uint w = 0; w < words2read; w++)
    *(dstAddr + w) = reg_read(0x75000 +  w*8);
    
  auto allTOB{
      reinterpret_cast< rf_bcs_all_entry_t *>(tob_mem)};

  auto armState{
      reinterpret_cast<const arm_status_unaligned_report_t *>(arm_mem)};

  for (auto i = 0; i < 1; i++) {
    printf(prefixStrFormat,"");
    printf(colStringFormat,"TBDNAME");
    printf(boardStringFormat,allTOB->pair.base.board_id);
    printf(bookSideFormat, allTOB->pair.base.bid_size, allTOB->pair.base.bid_price);
    printf(bookSideFormat, allTOB->pair.base.ask_size, allTOB->pair.base.ask_price);
    printf("\n");
    printf(prefixStrFormat,"");
    printf(colStringFormat,"TBDNAME");
    printf(boardStringFormat,allTOB->pair.quote.board_id);
    printf(bookSideFormat, allTOB->pair.quote.bid_size, allTOB->pair.quote.bid_price);
    printf(bookSideFormat, allTOB->pair.quote.ask_size, allTOB->pair.quote.ask_price);
    printf("\n");
  }
  
  // printf("| ");
  // if (armState->prod[prod_idx].arm.buy)
  //   printf(GRN "    BUY     " RESET);
  // else
  //   printf(RED "    buy     " RESET);

  // if (armState->prod[prod_idx].arm.sell)
  //   printf(GRN "    SELL " RESET);
  // else
  //   printf(RED "    sell " RESET);

  // printf(bookVersionFormat, armState->prod[prod_idx].expected_version);
  
  return 0;
}

// ################################################
int printStratStatus(StratState *pStratState) {

  /* ----------------------------------------- */
  printf(prefixStrFormat, "Arm State");
  for (auto stratId = 0; stratId < NUM_OF_STRAT;
       stratId++) {
    if (!active_strat[stratId])
      continue;

    CommonState *pCommonState;

    switch (stratId) {
    case S_P4:
      pCommonState = &pStratState->p4.commonState;
      break;
    case S_QED:
      pCommonState = &pStratState->QED.commonState;
      break;
    case S_SWEEP:
      pCommonState = &pStratState->fastSweep.commonState;
      break;
    case S_NEWS:
      pCommonState = &pStratState->news.commonState;
      break;
    case S_CANCEL:
      pCommonState = &pStratState->fastCancel.commonState;
      break;
    }

    if (pCommonState->arm_ver == EFC_HW_UNARMABLE) {
      printf(colformatsred, "FATAL Reload DRV");
    } else {
      if (!pCommonState->armed) {
        printf(colformatsred, "Unarmed");
      } else {
        printf(colformatsgrn, "Armed");
      }
    }
  }
  printf("\n");

  /* ----------------------------------------- */
  printf(prefixStrFormat, "Arm Version");
  for (auto stratId = 0; stratId < NUM_OF_STRAT;
       stratId++) {
    if (!active_strat[stratId])
      continue;

    CommonState *pCommonState;

    switch (stratId) {
    case S_P4:
      pCommonState = &pStratState->p4.commonState;
      break;
    case S_QED:
      pCommonState = &pStratState->QED.commonState;
      break;
    case S_SWEEP:
      pCommonState = &pStratState->fastSweep.commonState;
      break;
    case S_NEWS:
      pCommonState = &pStratState->news.commonState;
      break;
    case S_CANCEL:
      pCommonState = &pStratState->fastCancel.commonState;
      break;
    }

    printf(colformat, pCommonState->arm_ver);
  }
  printf("\n");

  /* ----------------------------------------- */
  printf(prefixStrFormat, "Report Only");
  for (auto stratId = 0; stratId < NUM_OF_STRAT;
       stratId++) {
    if (!active_strat[stratId])
      continue;

    switch (stratId) {
    case S_P4:
      printf(colformat,pStratState->p4.commonState.reportOnly);
      break;
    case S_QED:
      printf(colformat,pStratState->p4.commonState.reportOnly);
      break;
    case S_SWEEP:
      printf(colformat,pStratState->p4.commonState.reportOnly);
      break;
    case S_NEWS:
      printf(colformat,pStratState->p4.commonState.reportOnly);
      break;
    case S_CANCEL:
      printf(colformat,pStratState->p4.commonState.reportOnly);
      break;
    }
  }
  printf("\n");
  
  /* ----------------------------------------- */
  printf(prefixStrFormat, "Strat Evaluated");
  for (auto stratId = 0; stratId < NUM_OF_STRAT;
       stratId++) {
    if (!active_strat[stratId])
      continue;

    switch (stratId) {
    case S_P4:
      printf(colformat, pStratState->p4.strategyRuns);
      break;
    case S_QED:
      printf(colformat, pStratState->QED.strategyRuns);
      break;
    case S_SWEEP:
      printf(colformat,
             pStratState->fastSweep.strategyRuns);
      break;
    case S_NEWS:
      printf(colformat, pStratState->news.strategyRuns);
      break;
    case S_CANCEL:
      printf(colformat,
             pStratState->fastCancel.strategyRuns);
      break;
    }
  }
  printf("\n");

  /* ----------------------------------------- */
  printf(prefixStrFormat, "Strat Passed");
  for (auto stratId = 0; stratId < NUM_OF_STRAT;
       stratId++) {
    if (!active_strat[stratId])
      continue;

    switch (stratId) {
    case S_P4:
      printf(colformat, pStratState->p4.strategyPassed);
      break;
    case S_QED:
      printf(colformat, pStratState->QED.strategyPassed);
      break;
    case S_SWEEP:
      printf(colformat,
             pStratState->fastSweep.strategyPassed);
      break;
    case S_NEWS:
      printf(colformat, pStratState->news.strategyPassed);
      break;
    case S_CANCEL:
      printf(colformat,
             pStratState->fastCancel.strategyPassed);
      break;
    }
  }
  printf("\n");

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
  //  uint64_t mirror_enable = reg_read(0xf0030);

  for (auto coreId = 0; coreId < NUM_OF_CORES; coreId++) {
    //    coreParams[coreId].mirrorEnable = false;

    if (!coreParams[coreId].valid)
      continue;
    coreParams[coreId].hwParserEnable =
        (port_enable >> coreId) & 0x1;
    //    coreParams[coreId].mirrorEnable =
    //        (mirror_enable >> coreId) & 0x1;

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
      reg_read(0xf0800);
  /* uint64_t var_p4_cont_counter3 = */
  /*     reg_read(ADDR_P4_CONT_COUNTER3); */
  uint64_t var_p4_general_conf =
      reg_read(ADDR_P4_GENERAL_CONF);
  uint64_t var_fatal_debug = reg_read(ADDR_FATAL_CONF);

  /* uint64_t subscrCnt = reg_read(SCRPAD_EFC_SUBSCR_CNT); */
  /* pEfcState->totalSecs = (subscrCnt >> 32) & 0xFFFFFFFF; */
  /* pEfcState->subscribedSecs = subscrCnt & 0xFFFFFFFF; */

  pEfcState->strategyPassed =
      (var_p4_cont_counter1 >> 0) & MASK32;
  pEfcState->strategyRuns =
      (var_p4_cont_counter1 >> 32) & MASK32;
  /* pEfcState->ordersSubscribed = */
  /*     (var_p4_cont_counter3 >> 0) & MASK32; */
  /* pEfcState->ordersUnsubscribed = */
  /*     (var_p4_cont_counter3 >> 32) & MASK32; */

  uint64_t armReg = reg_read(NW_ARM_DISARM);
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
  pEfcState->epmDump = var_fatal_debug == 0xefa1beda;

  pEfcState->commonState.tcpFilterEn =
      (reg_read(0xf0020) >> 32 & 0x1) == 0;

  return 0;
}

// ################################################
int getProdState(EurProdState *pProdState) {

  uint64_t prodBase = SW_SCRATCHPAD_BASE + 16*8; //from line 16
  
  pProdState->totalSecs = reg_read(prodBase + 16*8);
  pProdState->bookSecs  = reg_read(prodBase + 17*8);

  for (auto prodHande = 0; prodHande < 16; prodHande++) {
    pProdState->secID[prodHande] =
      reg_read(prodBase + prodHande*8);
  }

  return 0;
}

// ################################################
int printProdState(EurProdState *pProdState) {
  printf(prefixStrFormat, "Total Products");
  printf (colformat,pProdState->totalSecs);
  printf("\n");
  printf(prefixStrFormat, "Book Products");
  printf (colformat,pProdState->bookSecs);
  printf("\n");
  
  return 0;
}

// ################################################
int printEurBookState(EurProdState *pProdState) {

  for (auto prodHande = 0; prodHande < 2; prodHande++) {
    if (pProdState->secID[prodHande]) {
      printf(prefixBookStrFormat, "Handle" , prodHande);
      char buffer[12];
      snprintf(buffer, 16, "%ju", pProdState->secID[prodHande]);
      //      printf(colformat, pProdState->secID[prodHande]);
      printf(colformats, buffer);
      printEurTOB(prodHande);
      printf("\n");
    }
  }
  return 0;
}

// ################################################
int printBookState(EurProdState *pProdState) {
  printTOB();
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
int printEfcState(EfcState *pEfcState) {
  //// TBD NOT CURRENTLY DISPLAYED
  printf("\n-----------------------------------------------"
         "---------");

  printf("ReportOnly\t\t%d\n",
         pEfcState->commonState.reportOnly);

  if (pEfcState->fatalDebug)
    printf(RED
           "WARNING: \'Fatal Debug\' is Active\n" RESET);

  if (pEfcState->epmDump)
    printf(RED
           "WARNING: \'EPM Dump Mode\' is Active\n" RESET);

  if (pEfcState->commonState.armed)
    printf(GRN
           "ARMED\n" RESET);
  else
    printf(RED
           "disarmed\n" RESET);
    

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

  return 0;
}

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

  auto pStratState = new StratState;
  /* ----------------------------------------- */

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
    //    getEfcState(&pStratState->p4);
    getExceptions(
        pEfcExceptionsReport,
        ekaHwCaps->hwCaps.core.bitmap_tcp_cores |
            ekaHwCaps->hwCaps.core.bitmap_md_cores);
    /* ----------------------------------------- */

    for (auto stratId = 0; stratId < NUM_OF_STRAT;
         stratId++)
      active_strat[stratId] = false;

    // for (auto parser = 0; parser < 2;
    //      parser++) { 
    //   if (((ekaHwCaps->hwCaps.version.parser >> parser*4) &
    //        0xF) != 0) {
    //     active_strat[((ekaHwCaps->hwCaps.version.parser >>
    //                    (coreId-2) * 4) &
    //                   0xF)] = true;
    //   }
    // }

    active_strat[S_P4] = true; // single strategy
    
    //    if (active_strat[S_P4])
    //    otherwisse no report only disaply in unconfigured parser
    getEfcState(&pStratState->p4);
    
    /* ----------------------------------------- */
    printf("\e[1;1H\e[2J"); //	system("clear");
    /* ----------------------------------------- */
    
    printTime();
    /* ----------------------------------------- */
    printHeader(coreParams, &pStratState->p4, ekaHwCaps);
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
      printStratHeader();
      printStratStatus(pStratState); //TD update 

      // getProdState(&pStratState->eur);
      // printProdHeader();
      // printProdState(&pStratState->eur);

      printBookHeader();
      printBookState(&pStratState->eur);
    
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

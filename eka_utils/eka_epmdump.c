#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "EkaHwCaps.h"
#include "EkaHwExpectedVersion.h"
#include "eka_macros.h"
#include "smartnic.h"

#include "EkaEpmRegion.h"

static volatile bool keep_work = true;
static FILE *outFile = stdout;

static int singleAction = -1;
static int singleActionRegion = EkaEpmRegion::Regions::Efc;
static int plainMemAddr = -1;
static int plainMemLen = -1;

static bool checkAll = false;
static bool printAll = false;

static SC_DeviceId devId = nullptr;
/* --------------------------------------------- */

static void INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected\n", __func__);
  printf("%s: exitting...\n", __func__);
  fflush(stdout);
  return;
}

/* --------------------------------------------- */

static void printUsage(char *cmd) {
  printf(
      "USAGE: %s <Memory dump or Action dump options> \n",
      cmd);
  printf("\t-l - List regions config\n");
  printf("\t-b - Memory dump: "
         "EPM Start Address (32B aligned)\n");
  printf("\t-s - Memory dump: "
         "Bytes to read (32B aligned)\n");
  printf("\t-a - Action dump: Action Id to dump\n");
  printf("\t-r - Action dump: Epm Region Id, "
         "default Efc(= 0)\n");
  printf("\t-w - Output file path, default stdout\n");
  printf("\t-c - Check all: Checks heapOffs and heapLen of "
         "all valid actions\n");
  printf("\t-p - Print all: Prints all valid actions\n");
  printf("\t-h - Print this help\n");
  return;
}
/* --------------------------------------------- */

static int getAttr(int argc, char *argv[]) {
  if (argc == 1) {
    printf("%s: No dump option specified\n", argv[0]);
    printUsage(argv[0]);
    exit(1);
  }
  int opt;
  while ((opt = getopt(argc, argv, ":b:s:w:r:a:lcph")) !=
         -1) {
    switch (opt) {
    case 'b':
      plainMemAddr = std::stol(optarg, 0, 0);
      printf("memAddr = %u (0x%x)\n", plainMemAddr,
             plainMemAddr);
      if (plainMemAddr % 32)
        on_error("memAddr = %u (0x%x) is not 32B aligned",
                 plainMemAddr, plainMemAddr);
      break;
    case 's':
      plainMemLen = std::stol(optarg, 0, 0);
      printf("memLen = %u\n", plainMemLen);
      if (plainMemLen % 32)
        on_error("memLen = %u (0x%x) is not 32B aligned",
                 plainMemLen, plainMemLen);
      break;
    case 'r':
      singleActionRegion = std::stol(optarg, 0, 0);
      printf(
          "region = %u (\'%s\')\n", singleActionRegion,
          EkaEpmRegion::getRegionName(singleActionRegion));
      break;
    case 'a':
      singleAction = std::stol(optarg, 0, 0);
      printf("actionIdx = %u \n", singleAction);
      break;
    case 'w':
      printf("outFile = %s\n", optarg);
      outFile = fopen(optarg, "w+");
      if (!outFile)
        on_error(
            "Failed to open outFile \'%s\' for writing",
            optarg);
      break;
    case 'l': {
      char regionsConf[16 * 1024] = {};
      EkaEpmRegion::printConfig(regionsConf,
                                sizeof(regionsConf));
      printf("%s", regionsConf);
      exit(1);
    } break;
    case 'h':
      printUsage(argv[0]);
      exit(1);
      break;
    case 'c':
      checkAll = true;
      printf("checkAll = true\n");
      break;
    case 'p':
      printAll = true;
      printf("printAll = true\n");
      break;
    case '?':
      printf("unknown option: %c\n", optopt);
      break;
    }
  }

  return 0;
}
/* --------------------------------------------- */

static void snWrite(uint64_t addr, uint64_t val) {
  if (SC_ERR_SUCCESS !=
      SC_WriteUserLogicRegister(devId, addr / 8, val))
    on_error("SN_Write(0x%jx,0x%jx) returned smartnic "
             "error code : %d",
             addr, val, SC_GetLastErrorCode());
}
#if 1
static uint64_t snRead(uint64_t addr) {
  uint64_t res;
  if (SC_ERR_SUCCESS !=
      SN_ReadUserLogicRegister(devId, addr / 8, &res))
    on_error(
        "SN_Read(0x%jx) returned smartnic error code : %d",
        addr, SC_GetLastErrorCode());
  return res;
}
#endif
/* --------------------------------------------- */
static size_t dumpMem(void *dst, int startAddr,
                      size_t len) {
  const int BlockSize = 32;
  const int WordSize = 8;
  if (startAddr % BlockSize || len % BlockSize)
    on_error("startAddr %d (0x%x) or len %ju (0x%ju) "
             "are not aligned to block size %d",
             startAddr, startAddr, len, len, BlockSize);

  auto nBlocks = len / BlockSize;
  auto blockAddr = startAddr / BlockSize;
  uint64_t *wrPtr = (uint64_t *)dst;

  for (auto block = 0; block < nBlocks; block++) {
    snWrite(0xf0100, blockAddr++);
    for (auto j = 0; j < BlockSize / WordSize; j++)
      *wrPtr++ = snRead(0x80000 + j * 8);
  }

  return len;
}
/* --------------------------------------------- */
static size_t dumpAction(void *dst, int region,
                         int flatIdx) {
  const int BlockSize = 64;
  const int WordSize = 8;

  uint64_t *wrPtr = (uint64_t *)dst;

  snWrite(0xf0100, flatIdx);
  for (auto j = 0; j < BlockSize / WordSize; j++)
    *wrPtr++ = snRead(0x70000 + j * 8);

  return 64;
}
/* --------------------------------------------- */
static void printIgmpPkt(const void *buf, FILE *f) {
  auto igmpP = reinterpret_cast<const IgmpPkt *>(buf);
  fprintf(f, "%s %s on core %s\n",
          igmpP->igmpHdr.type == 0x16 ? "IGMP JOIN"
          : 0x17                      ? "IGMP LEAVE"
                                      : "IGMP INVALID",
          EKA_IP2STR(igmpP->igmpHdr.group),
          EKA_MAC2STR(igmpP->ethHdr.src));
}

/* --------------------------------------------- */
static void checkHwCompat(const char *utilityName) {
  EkaHwCaps *ekaHwCaps = new EkaHwCaps(devId);
  if (!ekaHwCaps)
    on_error("ekaHwCaps == NULL");

  if (ekaHwCaps->hwCaps.version.epm !=
      EKA_EXPECTED_EPM_VERSION)
    on_error("This FW version does not support %s",
             utilityName);

  delete ekaHwCaps;
}
/* --------------------------------------------- */
static uint64_t configEpmDump() {
  // Remember original port enable
  uint64_t portEnableOrig = snRead(0xf0020);
#if 0
  printf("Original portEnableOrig = 0x%jx\n",
         portEnableOrig);
#endif

  // Disable HW parser
  snWrite(0xf0020, (portEnableOrig & 0xffffffffffffff00));
  uint64_t portEnableNew = snRead(0xf0020);
  // printf("New portEnableNew = 0x%jx\n", portEnableNew);

  // Enable EPM dump mode
  snWrite(0xf0f00, 0xefa1beda);
  // printf("Dump EPM enabled\n");

  return portEnableOrig;
}
/* --------------------------------------------- */

static void restorePrevConf(uint64_t origConf) {
  // printf("Dump EPM finished\n");

  // Disable EPM dump mode
  snWrite(0xf0f00, 0x0);
// printf("Dump EPM disabled\n");

// Return original port enable
#if 0
  printf("Reconfiguring original portEnableOrig = 0x%jx\n",
         portEnableOrig);
#endif
  snWrite(0xf0020, origConf);
}
/* --------------------------------------------- */
bool checkActionParams(int region, int actionIdx,
                       const epm_action_t *hwAction) {
  bool isValid = hwAction->bit_params.bitmap.action_valid;

  bool passed = true;
  if (isValid && hwAction->data_db_ptr !=
                     EkaEpmRegion::getActionHeapOffs(
                         region, actionIdx)) {
    on_warning(
        "heapOffs %u != Expected %u", hwAction->data_db_ptr,
        EkaEpmRegion::getActionHeapOffs(region, actionIdx));
    passed = false;
  }
  if (isValid &&
      hwAction->payloadSize >
          EkaEpmRegion::getActionHeapBudget(region)) {
    on_warning("heapSize %u > Expected limit %u",
               hwAction->payloadSize,
               EkaEpmRegion::getActionHeapBudget(region));
    passed = false;
  }
  return passed;
}

/* --------------------------------------------- */
static std::pair<int, int>
processAction(int region, int actionIdx, char *hexDumpMsg) {
  auto aMem = new uint8_t[64];
  int flatIdx =
      actionIdx + EkaEpmRegion::getBaseActionIdx(region);

  dumpAction(aMem, region, flatIdx);

  auto a = reinterpret_cast<const epm_action_t *>(aMem);
  bool isValid = a->bit_params.bitmap.action_valid;

  if (!isValid)
    return {-1, 0};

  char actionHexDumpMsg[256] = {};

  auto actionType = getActionTypeFromUser(a->user);

  sprintf(actionHexDumpMsg,
          "%s region %d, \'%s\' "
          "action %d (%d): "
          "%s, "
          "heapOffs = %u, "
          "heapSize = %u,"
          "%s, " // PayloadLen/CSum origin
          "TCP conn = %d:%d, "
          "next = %d",
          EkaEpmRegion::getRegionName(region), region,
          printActionType(actionType), actionIdx, flatIdx,
          isValid ? "Valid" : "Not Valid", a->data_db_ptr,
          a->payloadSize,
          a->tcpCsSizeSource == TcpCsSizeSource::FROM_ACTION
              ? "FROM_ACTION"
              : "FROM_DESCR",
          a->target_core_id, a->target_session_id,
          (int16_t)a->next_action_index);

  fprintf(outFile, "%s\n", actionHexDumpMsg);
  // hexDump(actionHexDumpMsg, aMem, 64, outFile);

  if (flatIdx != getActionGlobalIdxFromUser(a->user))
    on_error(
        "flatIdx %u != getActionGlobalIdxFromUser() %u, "
        "user =0x%016jx",
        flatIdx, getActionGlobalIdxFromUser(a->user),
        a->user);

  int memAddr = a->data_db_ptr;
  int memLen = a->payloadSize;

#if 0
  sprintf(hexDumpMsg,
          "Heap of %s Action %d (%d) with "
          "heap offs = %d (0x%x), heap size %d (0x%x), "
          "at region %d \'%s\'",
          printActionType(actionType), actionIdx, flatIdx,
          memAddr, memAddr, memLen, memLen, region,
          EkaEpmRegion::getRegionName(region));
#endif
  sprintf(hexDumpMsg, "Heap of %s Action %d",
          printActionType(actionType), actionIdx);

  if (!checkActionParams(region, actionIdx, a))
    on_error("checkActionParams failed for action %d at "
             "region %d ",
             region, actionIdx);

  return {memAddr, memLen};
}
/* --------------------------------------------- */
static void printHeapMem(int region, int actionIdx,
                         int heapOffs, int heapLen,
                         const char *hexDumpMsg) {
  if (!heapLen)
    return;

  auto memLen2dump =
      roundUp<decltype(heapLen)>(heapLen, 32);

  auto mem = new uint8_t[memLen2dump];
  if (!mem)
    on_error("failed allocating mem[%d]", memLen2dump);

  dumpMem(mem, heapOffs, memLen2dump);

  hexDump(hexDumpMsg, mem, heapLen, outFile);

  if (region >= EkaEpmRegion::Regions::EfcMc)
    printIgmpPkt(mem, outFile);

  delete[] mem;
  return;
}
/* --------------------------------------------- */
static void dumpSingleAction(int singleActionRegion,
                             int singleAction) {
  char hexDumpMsg[256] = {};
  auto [heapOffs, heapLen] = processAction(
      singleActionRegion, singleAction, hexDumpMsg);

  if (heapOffs >= 0)
    printHeapMem(singleActionRegion, singleAction, heapOffs,
                 heapLen, hexDumpMsg);
  return;
}

/* --------------------------------------------- */
/* --------------------------------------------- */
/* --------------------------------------------- */

int main(int argc, char *argv[]) {
  getAttr(argc, argv);

  devId = SC_OpenDevice(NULL, NULL);
  if (!devId)
    on_error("Cannot open Smartnic Device");
  checkHwCompat(argv[0]);
  signal(SIGINT, INThandler);
  auto prevConf = configEpmDump();

  if (checkAll || printAll) {
    for (auto regionId = 0;
         regionId < EkaEpmRegion::Regions::Total;
         regionId++) {
      for (auto actionIdx = 0;
           actionIdx <
           EkaEpmRegion::getMaxActions(regionId);
           actionIdx++) {
        dumpSingleAction(regionId, actionIdx);
      }
    }
  } else {
    if (singleAction >= 0) {
      dumpSingleAction(singleActionRegion, singleAction);
    } else {
      printHeapMem(-1, -1, plainMemAddr, plainMemLen,
                   "Plain Memory Dump");
    }
  }

  restorePrevConf(prevConf);

  fclose(outFile);

  return 0;
}

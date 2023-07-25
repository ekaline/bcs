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

volatile bool keep_work = true;
FILE *outFile = stdout;

int actionIdx = -1;
int region = EkaEpmRegion::Regions::Efc;
int memAddr = -1;
int memLen = -1;

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
  printf("\t-b - Memory dump: "
         "EPM Start Address (32B aligned)\n");
  printf("\t-s - Memory dump: "
         "Bytes to read (32B aligned)\n");
  printf("\t-a - Action dump: Action Id to dump\n");
  printf("\t-r - Action dump: Epm Region Id, "
         "default Efc(= 0)\n");
  printf("\t-w - Output file path, default stdout\n");
  printf("\t-h - Print this help\n");
  return;
}
/* --------------------------------------------- */

static int getAttr(int argc, char *argv[]) {

  int opt;
  while ((opt = getopt(argc, argv, ":b:s:w:r:a:h")) != -1) {
    switch (opt) {
    case 'b':
      memAddr = std::stol(optarg, 0, 0);
      printf("memAddr = %u (0x%x)\n", memAddr, memAddr);
      if (memAddr % 32)
        on_error("memAddr = %u (0x%x) is not 32B aligned",
                 memAddr, memAddr);
      break;
    case 's':
      memLen = std::stol(optarg, 0, 0);
      printf("memLen = %u\n", memLen);
      if (memLen % 32)
        on_error("memLen = %u (0x%x) is not 32B aligned",
                 memLen, memLen);
      break;
    case 'r':
      region = std::stol(optarg, 0, 0);
      printf("region = %u (\'%s\')\n", region,
             EkaEpmRegion::getRegionName(region));
      break;
    case 'a':
      actionIdx = std::stol(optarg, 0, 0);
      printf("actionIdx = %u \n", actionIdx);
      break;
    case 'w':
      printf("outFile = %s\n", optarg);
      outFile = fopen(optarg, "w+");
      if (!outFile)
        on_error(
            "Failed to open outFile \'%s\' for writing",
            optarg);
      break;
    case 'h':
      printUsage(argv[0]);
      exit(1);
      break;
    case '?':
      printf("unknown option: %c\n", optopt);
      break;
    }
  }

  return 0;
}
/* --------------------------------------------- */

static void eka_write(SC_DeviceId devId, uint64_t addr,
                      uint64_t val) {
  if (SC_ERR_SUCCESS !=
      SC_WriteUserLogicRegister(devId, addr / 8, val))
    on_error("SN_Write(0x%jx,0x%jx) returned smartnic "
             "error code : %d",
             addr, val, SC_GetLastErrorCode());
}

uint64_t eka_read(SC_DeviceId devId, uint64_t addr) {
  uint64_t res;
  if (SC_ERR_SUCCESS !=
      SN_ReadUserLogicRegister(devId, addr / 8, &res))
    on_error(
        "SN_Read(0x%jx) returned smartnic error code : %d",
        addr, SC_GetLastErrorCode());
  return res;
}

/* --------------------------------------------- */
size_t dumpMem(SC_DeviceId devId, void *dst, int startAddr,
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
    eka_write(devId, 0xf0100, blockAddr++);
    for (auto j = 0; j < BlockSize / WordSize; j++)
      *wrPtr++ = eka_read(devId, 0x80000 + j * 8);
  }

  return len;
}

/* --------------------------------------------- */

int main(int argc, char *argv[]) {
  SC_DeviceId devId = SC_OpenDevice(NULL, NULL);
  if (!devId)
    on_error("Cannot open Smartnic Device");
  EkaHwCaps *ekaHwCaps = new EkaHwCaps(devId);
  if (!ekaHwCaps)
    on_error("ekaHwCaps == NULL");

  if (ekaHwCaps->hwCaps.version.epm !=
      EKA_EXPECTED_EPM_VERSION)
    on_error("This FW version does not support %s",
             argv[0]);

  signal(SIGINT, INThandler);

  getAttr(argc, argv);

  // Remember original port enable
  uint64_t portEnableOrig = eka_read(devId, 0xf0020);
  printf("Original portEnableOrig = 0x%jx\n",
         portEnableOrig);

  // Disable HW parser
  eka_write(devId, 0xf0020,
            (portEnableOrig & 0xffffffffffffff00));
  uint64_t portEnableNew = eka_read(devId, 0xf0020);
  printf("New portEnableNew = 0x%jx\n", portEnableNew);

  // Enable EPM dump mode
  eka_write(devId, 0xf0f00, 0xefa1beda);
  printf("Dump EPM enabled\n");

  if (actionIdx == -1) {
    printf("Dumping plain memory: memAddr = %d (0x%x), "
           "memLen = %d (0x%x)\n",
           memAddr, memAddr, memLen, memLen);
  } else {

    memAddr =
        EkaEpmRegion::getActionHeapOffs(region, actionIdx);
    memLen = EkaEpmRegion::getActionHeapBudget(region);

    printf("Dumping Action %d with "
           "heap offs = %d (0x%x), heap size %d (0x%x)"
           "at region %d \'%s\'\n",
           actionIdx, memAddr, memAddr, memLen, memLen,
           region, EkaEpmRegion::getRegionName(region));
  }

  auto mem = new uint8_t[memLen];
  if (!mem)
    on_error("failed allocating mem[%d]", memLen);

  dumpMem(devId, mem, memAddr, memLen);
  hexDump("Memory Dump", mem, memLen, outFile);

  printf("Dump EPM finished\n");

  // Disable EPM dump mode
  eka_write(devId, 0xf0f00, 0x0);
  printf("Dump EPM disabled\n");

  // Return original port enable
  printf("Reconfiguring original portEnableOrig = 0x%jx\n",
         portEnableOrig);
  eka_write(devId, 0xf0020, portEnableOrig);

  fclose(outFile);
  delete[] mem;

  return 0;
}

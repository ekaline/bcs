#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "Efc.h"
#include "EkaHwCaps.h"
#include "EkaHwInternalStructs.h"
#include "eka_macros.h"
#include "smartnic.h"

SC_DeviceId devId = nullptr;

struct hw_ctxdump_t {
  uint32_t Handle;
  uint8_t HashStatus;
  uint8_t pad3[3];
  EkaHwSecCtx SecCTX;
} __attribute__((packed))
__attribute__((aligned(sizeof(uint64_t))));

/* --------------------------------------------- */
static void checkHwCompat(const char *utilityName) {
  EkaHwCaps *ekaHwCaps = new EkaHwCaps(devId);
  if (!ekaHwCaps)
    on_error("ekaHwCaps == NULL");

  if (ekaHwCaps->hwCaps.version.ctxbyhash != 4)
    on_error("This FW version (hwCaps.version.ctxbyhash = "
             "%d) does not support %s",
             ekaHwCaps->hwCaps.version.ctxbyhash,
             utilityName);
}
/* --------------------------------------------- */

static void snWrite(uint64_t addr, uint64_t val) {
  if (SC_ERR_SUCCESS !=
      SC_WriteUserLogicRegister(devId, addr / 8, val))
    on_error("SN_Write(0x%jx,0x%jx) returned smartnic "
             "error code : %d",
             addr, val, SC_GetLastErrorCode());
}
/* --------------------------------------------- */

static uint64_t snRead(uint64_t addr) {
  uint64_t res;
  if (SC_ERR_SUCCESS !=
      SN_ReadUserLogicRegister(devId, addr / 8, &res))
    on_error(
        "SN_Read(0x%jx) returned smartnic error code : %d",
        addr, SC_GetLastErrorCode());
  return res;
}

/* --------------------------------------------- */

static void printUsage(const char *cmd) {
  printf("USAGE: %s [opt] SecurityID "
         "-- dump HW Security Context\n"
         "\t-n numeric representaion of SecurityID"
         "\t-a ASCII representaion of SecurityID "
         "\t-h print this help"
         "(Pitch Only!)",
         cmd);
}
/* --------------------------------------------- */

static uint64_t getBinSecId(const char *secChar) {
  if (strlen(secChar) != 6)
    on_error("unexpected security \'%s\' len %ju != 6",
             secChar, strlen(secChar));
  char shiftedStr[8] = {};
  for (auto i = 0; i < 6; i++)
    shiftedStr[i + 2] = secChar[i];

  return be64toh(*(uint64_t *)shiftedStr);
}
/* --------------------------------------------- */
static bool isAscii(char letter) {
  //  EKA_LOG("testing %d", letter);
  if ((letter >= '0' && letter <= '9') ||
      (letter >= 'a' && letter <= 'z') ||
      (letter >= 'A' && letter <= 'Z'))
    return true;
  return false;
}
/* --------------------------------------------- */

inline std::string cboeSecIdString(uint64_t n) {
  auto revN = be64toh(n);
  auto p = reinterpret_cast<const char *>(&revN);
  std::string res = {};
  for (auto i = 0; i < sizeof(n); i++) {
    if (isAscii(*(p + i)))
      res += *(p + i);
  }
  return res;
}
/* --------------------------------------------- */

static uint64_t secIdBin = 0;

static void getAttr(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, ":n:a:h")) != -1) {
    switch (opt) {
    case 'n':
      secIdBin = std::stoul(optarg, 0, 0);
      printf("secIdBin = 0x%016jx (\'%s\')\n", secIdBin,
             cboeSecIdString(secIdBin).c_str());
      break;
    case 'a':
      secIdBin = getBinSecId(optarg);
      printf("secIdBin = 0x%016jx (\'%s\')\n", secIdBin,
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
}

/* --------------------------------------------- */

int main(int argc, char *argv[]) {
  getAttr(argc, argv);

  devId = SC_OpenDevice(NULL, NULL);
  if (devId == NULL) {
    fprintf(stderr,
            "Cannot open XYU device. Is driver loaded?\n");
    exit(-1);
  }

  checkHwCompat(argv[0]);

  uint64_t protocol_id = 2; // pitch

  uint64_t hw_cmd =
      (secIdBin & 0x00ffffffffffffff) | (protocol_id << 56);
  printf("checking secIdBin 0x%016jx, protocolid %u, hwcmd "
         "0x%016jx\n",
         secIdBin, protocol_id, hw_cmd);

  snWrite(0xf0038, hw_cmd);

  hw_ctxdump_t hwCtxDump = {};

  uint words2read = roundUp8(sizeof(hw_ctxdump_t)) / 8;
  uint64_t srcAddr = 0x71000 / 8;
  uint64_t *dstAddr = (uint64_t *)&hwCtxDump;

  for (uint w = 0; w < words2read; w++)
    SN_ReadUserLogicRegister(devId, srcAddr++, dstAddr++);

  if (!(hwCtxDump.HashStatus)) {
    printf("secIdBin 0x%016jx was not found in hash, using "
           "protocolid %u\n",
           secIdBin, protocol_id);
    return 0;
  }

  printf("secIdBin 0x%016jx found, ctx in handle %u\n",
         secIdBin, hwCtxDump.Handle);
  printf(
      "bidMinPrice=%d,askMaxPrice=%d, bidSize=%d, "
      "askSize=%d, versionKey=%d, lowerBytesOfSecId=0x%x\n",
      hwCtxDump.SecCTX.bidMinPrice,
      hwCtxDump.SecCTX.askMaxPrice,
      hwCtxDump.SecCTX.bidSize, hwCtxDump.SecCTX.askSize,
      hwCtxDump.SecCTX.versionKey,
      hwCtxDump.SecCTX.lowerBytesOfSecId);

  SN_CloseDevice(devId);

  return 0;
}

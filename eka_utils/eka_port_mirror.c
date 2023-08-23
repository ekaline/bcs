#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "eka_macros.h"
#include "smartnic.h"

#include "EkaHwCaps.h"
#include "EkaHwExpectedVersion.h"

static SC_DeviceId devId = nullptr;

static const uint MirrorConfigAddr = 0xf0030;
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
  uint64_t res = 0;
  if (!devId)
    on_error("!devId");

  if (SC_ERR_SUCCESS !=
      SN_ReadUserLogicRegister(devId, addr / 8, &res))
    on_error(
        "SN_Read(0x%jx) returned smartnic error code : %d",
        addr, SC_GetLastErrorCode());
  return res;
}

/* --------------------------------------------- */

static void printUsage(char *cmd) {
  printf("USAGE: %s [option] -- Enable/Disable physical "
         "lanes mirroring:\n",
         cmd);
  printf("\t-A - ALL: Mirroring lanes: "
         "#0 -> #3 and #3 -> #0\n");
  printf("\t-U - UP: Mirroring only lanes: "
         "#3 -> #0\n");
  printf("\t-D - Disable: Disable mirroring \n");
  printf("\t-P - Print current state \n");
  printf("\t-h - Print this help\n");
  return;
}

/* --------------------------------------------- */

static const char *decodeMirrorState(uint64_t c) {
  switch (c) {
  case 0:
    return "Disable";
  case 0x8:
    return "Uplink mirroring: #3 -> #0";
  case 0x9:
    return "Duplex mirroring: #3 <--> #0";
  default:
    return "Unexpected conf";
  }
}
/* --------------------------------------------- */
static void checkHwCompat(const char *utilityName) {
  EkaHwCaps *ekaHwCaps = new EkaHwCaps(devId);
  if (!ekaHwCaps)
    on_error("ekaHwCaps == NULL");

  if (ekaHwCaps->hwCaps.version.mirror !=
      EKA_EXPECTED_PORT_MIRRORING)
    on_error("This FW version does not support %s",
             utilityName);

  delete ekaHwCaps;
}
/* --------------------------------------------- */

int main(int argc, char *argv[]) {
  devId = SN_OpenDevice(NULL, NULL);
  if (!devId)
    on_error("Cannot open Ekaline Smartnic device. Is "
             "driver loaded?");

  checkHwCompat(argv[0]);

  int opt;
  while ((opt = getopt(argc, argv, ":aAuUdDpPh")) != -1) {
    switch (opt) {
    case 'A':
    case 'a':
      printf("Mirroring ALL: #0 <--> #3\n");
      snWrite(MirrorConfigAddr, 0x9);
      break;
    case 'U':
    case 'u':
      printf("Mirroring UP: #3 --> #0\n");
      snWrite(MirrorConfigAddr, 0x8);
      break;
    case 'D':
    case 'd':
      printf("Disable Mirroring UP\n");
      snWrite(MirrorConfigAddr, 0x0);
      break;

    case 'P':
    case 'p': {
      auto res = snRead(MirrorConfigAddr);
      printf("Current Mirroring state 0x%jx: %s\n", res,
             decodeMirrorState(res));
    } break;

    case 'h':
      printUsage(argv[0]);
      exit(1);
      break;

    case '?':
      printf("unknown option: %c\n", optopt);
      break;
    }
  }

  SN_CloseDevice(devId);

  return 0;
}

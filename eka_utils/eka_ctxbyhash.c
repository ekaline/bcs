#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

#include "EkaHwCaps.h"
#include "Efc.h"
#include "EkaHwInternalStructs.h"
#include "smartnic.h"
#include "eka_macros.h"

SC_DeviceId devId = nullptr;

struct hw_ctxdump_t {
  uint32_t    Handle;
  uint8_t     HashStatus;
  uint8_t     pad3[3];
  EkaHwSecCtx SecCTX;
} __attribute__((packed)) __attribute__ ((aligned(sizeof(uint64_t))));

/* --------------------------------------------- */
static void checkHwCompat(const char *utilityName) {
  EkaHwCaps *ekaHwCaps = new EkaHwCaps(devId);
  if (!ekaHwCaps)
    on_error("ekaHwCaps == NULL");

  if (ekaHwCaps->hwCaps.version.ctxbyhash !=
      4)
    on_error("This FW version (hwCaps.version.ctxbyhash = %d) does not support %s",
	       ekaHwCaps->hwCaps.version.ctxbyhash,utilityName);
}

static void snWrite(uint64_t addr, uint64_t val) {
  if (SC_ERR_SUCCESS !=
      SC_WriteUserLogicRegister(devId, addr / 8, val))
    on_error("SN_Write(0x%jx,0x%jx) returned smartnic "
             "error code : %d",
             addr, val, SC_GetLastErrorCode());
}

static uint64_t snRead(uint64_t addr) {
  uint64_t res;
  if (SC_ERR_SUCCESS !=
      SN_ReadUserLogicRegister(devId, addr / 8, &res))
    on_error(
        "SN_Read(0x%jx) returned smartnic error code : %d",
        addr, SC_GetLastErrorCode());
  return res;
}


int main(int argc, char *argv[]){
  devId = SC_OpenDevice(NULL, NULL);
  if (devId == NULL) {
    fprintf( stderr, "Cannot open XYU device. Is driver loaded?\n" );
    exit( -1 );
  }

  checkHwCompat(argv[0]);

  uint64_t secid = (uint64_t)strtoull(argv[1], NULL, 16) ;
  uint64_t  protocol_id = 2; //pitch

  uint64_t hw_cmd = (secid & 0x00ffffffffffffff) | (protocol_id<<56);
  printf ("checking secid 0x%016jx, protocolid %u, hwcmd 0x%016jx\n",secid,protocol_id,hw_cmd);

  snWrite(0xf0038, hw_cmd);


  hw_ctxdump_t hwCtxDump = {};

  uint words2read = roundUp8(sizeof(hw_ctxdump_t)) / 8;
  uint64_t srcAddr = 0x71000 / 8;
  uint64_t *dstAddr = (uint64_t *)&hwCtxDump;

  for (uint w = 0; w < words2read; w++)
    SN_ReadUserLogicRegister(devId, srcAddr++,
                             dstAddr++);


  if (!(hwCtxDump.HashStatus)) {
    printf("secid 0x%016jx was not found in hash, using protocolid %u\n",secid,protocol_id);
    return 0;
  }

  printf ("secid 0x%016jx found, ctx in handle %u\n",secid,hwCtxDump.Handle);
  printf ("bidMinPrice=%d,askMaxPrice=%d, bidSize=%d, askSize=%d, versionKey=%d, lowerBytesOfSecId=0x%x\n",
	  hwCtxDump.SecCTX.bidMinPrice,
	  hwCtxDump.SecCTX.askMaxPrice,
	  hwCtxDump.SecCTX.bidSize,
	  hwCtxDump.SecCTX.askSize,
	  hwCtxDump.SecCTX.versionKey,
	  hwCtxDump.SecCTX.lowerBytesOfSecId);

  SN_CloseDevice(devId);

  return 0;
}

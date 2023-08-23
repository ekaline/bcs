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

/* --------------------------------------------- */
static void checkHwCompat(const char *utilityName) {
  EkaHwCaps *ekaHwCaps = new EkaHwCaps(devId);
  if (!ekaHwCaps)
    on_error("ekaHwCaps == NULL");

  if (ekaHwCaps->hwCaps.version.ctxbyhash !=
      3)
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
    uint64_t res;


uint64_t hw_cmd = (secid & 0x00ffffffffffffff) | (protocol_id<<56);
printf ("checking secid 0x%016jx, protocolid %u, hwcmd 0x%016jx\n",secid,protocol_id,hw_cmd);

    snWrite(0xf0038, hw_cmd);
res = snRead(0x71000);

EkaHwSecCtx *hwSecCtx = (EkaHwSecCtx*)&res;
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t bidSize;
  uint8_t askSize;
  uint8_t versionKey;
  uint8_t lowerBytesOfSecId;


    printf("result = %ju,(0x%jx)\n",res,res);
if (res==0xbad00000bad) {
  printf("SECID not found!!!\n");
return 0 ;
}

printf ("bidMinPrice=%d,askMaxPrice=%d, bidSize=%d, askSize=%d, versionKey=%d, lowerBytesOfSecId=0x%x\n",
	  hwSecCtx->bidMinPrice,
	  hwSecCtx->askMaxPrice,
	  hwSecCtx->bidSize,
	  hwSecCtx->askSize,
	  hwSecCtx->versionKey,
	  hwSecCtx->lowerBytesOfSecId);

    SN_CloseDevice(devId);

    return 0;
}

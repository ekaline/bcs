#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

#include "smartnic.h"

int main(int argc, char *argv[]){
    SN_DeviceId DeviceId = SN_OpenDevice(NULL, NULL);
    if (DeviceId == NULL) {
        fprintf( stderr, "Cannot open XYU device. Is driver loaded?\n" );
        exit( -1 );
    }
    uint32_t addr = (int)strtoul(argv[1], NULL, 16) / 8;
    uint64_t res;

    SN_ReadUserLogicRegister (DeviceId, addr, &res);
    //    printf ("reading from address %s (=%" PRIu32 "):\n",argv[1],addr);
    printf ("reading from address %s (=%u):\n",argv[1],addr);
    //    printf("%" PRIu64 ",(0x%lx)\n",res,res);
    printf("%ju,(0x%jx)\n",res,res);
    SN_CloseDevice(DeviceId);

    return 0;
}

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
    uint32_t addr = (uint32_t)strtoul(argv[1], NULL, 16) / 8;
    uint64_t data = (uint64_t)strtoull(argv[2], NULL, 16);

    //    printf ("writing %s(%" PRIu64 ") to address %s (=%" PRIu32 "):\n",argv[2],data,argv[1],addr);
    printf ("writing %s(%ju) to address %s (=%u):\n",argv[2],data,argv[1],addr);
    SN_WriteUserLogicRegister(DeviceId, addr, data);

    SN_CloseDevice(DeviceId);

    return 0;
}

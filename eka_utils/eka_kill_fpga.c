#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

#include "smartnic.h"

int main(int argc, char *argv[]){
    SN_DeviceId DeviceId = SN_OpenDevice(NULL, NULL);
    char ch;

    if (DeviceId == NULL) {
        fprintf( stderr, "Cannot open XYU device. Is driver loaded?\n" );
        exit( -1 );
    }

    printf ("\nDo you confirm to kill FGPA by completely disable all outgoing traffic? (type Y to confirm)\n > ");
    scanf (" %c", &ch);
    
    if (ch != 'Y') {
      printf ("\n ...cancelled, FPGA keeps working\n");
      return 0;
    }

    uint32_t addr = 0xf0fe0 / 8;
    uint64_t data = 0xefa0dead1;

    SN_WriteUserLogicRegister(DeviceId, addr, data);

    printf ("\n ...confirmed, FPGA killed, reload driver to return to a normal state\n");

    SN_CloseDevice(DeviceId);

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/time.h>

#include "eka_macros.h"
#include "smartnic.h"
#include "EkaHwCaps.h"
#include "EkaHwExpectedVersion.h"

volatile bool keep_work = true;
/* --------------------------------------------- */

static void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected\n",__func__);
  printf ("%s: exitting...\n",__func__);
  fflush(stdout);
  return;
}
/* --------------------------------------------- */

static void printUsage(char* cmd) {
    printf("USAGE: %s <options> \n",cmd); 
    printf("\t-b - EPM Start Address (32B aligned)\n"); 
    printf("\t-s - Bytes to read (32B aligned)\n"); 
    printf("\t-w - Output file path\n"); 
    printf("\t-h - Print this help\n"); 
    return;
}

static int getAttr(int argc, char *argv[],
		   uint32_t* startAddr, uint32_t* bytesRead,
		   char* fileName) {
  
    bool ifSet = false;
    bool fileSet = false;
    int opt; 
    while((opt = getopt(argc, argv, ":b:s:w:h")) != -1) {  
	switch(opt) {  
	case 'b':  
	    *startAddr = atoi(optarg);
	    printf("startAddr = %u\n", *startAddr);
	    ifSet = true;
	    break;
	case 's':  
	    *bytesRead = atoi(optarg);
	    printf("bytesRead = %u\n", *bytesRead);
	    ifSet = true;
	    break;  	
	case 'w':  
	    strcpy(fileName,optarg);
	    printf("fileName = %s\n", fileName);
	    fileSet = true;
	    break;
	case 'h':  
	    printUsage(argv[0]);
	    exit (1);
	    break;  
	case '?':  
	    printf("unknown option: %c\n", optopt); 
	    break;  
	}  
    }  

    if (! fileSet) on_error("Output file name is not set. Use \'-w <file name>\' mandatory option");
    return 0;
}
/* --------------------------------------------- */

static void eka_write(SC_DeviceId devId, uint64_t addr, uint64_t val) {
    if (SC_ERR_SUCCESS != SC_WriteUserLogicRegister(devId, addr/8, val))
	on_error("SN_Write(0x%jx,0x%jx) returned smartnic error code : %d",addr,val,SC_GetLastErrorCode());
}

uint64_t eka_read(SC_DeviceId devId, uint64_t addr) {
    uint64_t res;

    if (SC_ERR_SUCCESS != SN_ReadUserLogicRegister (devId, addr/8, &res))
      on_error("SN_Read(0x%jx) returned smartnic error code : %d",addr,SC_GetLastErrorCode());

    return res;
}

/* --------------------------------------------- */

int main(int argc, char *argv[]) {
    SC_DeviceId devId = SC_OpenDevice(NULL, NULL);
    if (devId == NULL) on_error("Cannot open Smartnic Device");
    EkaHwCaps* ekaHwCaps = new EkaHwCaps(devId);
    if (ekaHwCaps == NULL) on_error("ekaHwCaps == NULL");
    
    if (ekaHwCaps->hwCaps.version.sniffer != EKA_EXPECTED_SNIFFER_VERSION)
	on_error("This FW version does not support %s",argv[0]);

    uint32_t startAddr = -1;
    uint32_t bytesRead = -1;

    char fileName[256] = {};

    signal(SIGINT, INThandler);

    getAttr(argc,argv,&startAddr,&bytesRead,fileName);

    FILE* out_file = fopen(fileName, "w+");
    if (out_file == NULL) on_error("Failed opening pcapFile file \'%s\'\n",fileName);

    // Remember original port enable
    uint64_t portEnableOrig = eka_read(devId,0xf0020);
    printf ("Original portEnableOrig = 0x%jx\n",portEnableOrig);

    // Disable HW parser
    eka_write(devId,0xf0020,(portEnableOrig&0xffffffffffffff00));
    uint64_t portEnableNew = eka_read(devId,0xf0020);
    printf ("New portEnableNew = 0x%jx\n",portEnableNew);
    
    // Wait for clearing the pipe
    sleep (2);

    // Enable EPM dump mode
    eka_write(devId,0xf0f00,0xefa1beda);
    printf ("Dump EPM enabled\n");

    // Start Dump Loop
    uint64_t rd_val;

    for (auto i = 0; i < bytesRead; i=i+32) {
      // Configuring indirect address
      eka_write(devId,0xf0100,(i+startAddr)/32);
      for (auto j = 0; j < 4; j++) {
	rd_val = eka_read(devId,0x80000+j*8);
	printf ("0x%jx = 0x%016jx\t",startAddr+i+j*8, rd_val);
	for (int k = 0; k < 8; k++) {
	  char c = (rd_val >> (k * 8)) & 0xFF;
	  printf("%c", c);
	}
	printf ("\n");
      }
    }

    // Wait for clearing the pipe
    sleep (2);
    printf ("Dump EPM finished\n");

    // Disable EPM dump mode
    eka_write(devId,0xf0f00,0x0);
    printf ("Dump EPM disabled\n");

    // Return original port enable
    printf ("Reconfiguring original portEnableOrig = 0x%jx\n",portEnableOrig);
    eka_write(devId,0xf0020,portEnableOrig);
    

    fclose(out_file);

    return 0;
}

//#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "smartnic.h"

#define NUM_OF_CORES 6

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

void die(const char *msg) {
    perror(msg);
    exit(1);
}

#define BITS_PER_LONG 64
#define BIT(x) (1UL << (x))
#define MMAP_FLAG_BAR_SHIFT (BITS_PER_LONG - 1)
#define MMAP_FLAG_DMA_SHIFT (BITS_PER_LONG - 2)
#define MMAP_BAR_NUM_SHIFT  (BITS_PER_LONG - 5)
#define MMAP_FLAG_BAR       BIT(MMAP_FLAG_BAR_SHIFT)

#define MAX_BAR_SIZE (1<<25)
#define FREQUENCY 166

// global configurations
#define ADDR_INTERRUPT_SHADOW_RO 0xf0790
#define ADDR_INTERRUPT_SHADOW_RC 0xf0798
#define ADDR_VERSION_ID 0xffff0
// per core base
#define ADDR_INTERRUPT_0_SHADOW_RO     0xe0150
#define ADDR_INTERRUPT_0_SHADOW_RC     0xe0158

#define MASK64 0xffffffffffffffff
#define MASK32 0xffffffff
#define MASK8  0xff
#define MASK4  0xf
#define MASK1  0x1

SN_DeviceId DeviceId;

uint64_t reg_read (uint32_t addr)
{
    uint64_t value = -1;

    SN_ReadUserLogicRegister (DeviceId, addr/8, &value);

    return value;

}

int main(int argc, char *argv[])
{

  setlocale(LC_NUMERIC, "");

  printf(RESET);

  DeviceId = SN_OpenDevice(NULL, NULL);
  if (DeviceId == NULL) {
    fprintf( stderr, "Cannot open FiberBlaze device. Is driver loaded?\n" );
    exit( -1 );
  }

  int curr_core;
  int sleep_seconds;
  int stop_loop = 0;
  char *endptr;

  //  printf ("%d\n",argc);

  if ( argc == 2 )
  {
    //sleep_seconds = atoi(argv[1]);
    sleep_seconds = strtod(argv[1], &endptr);
    if (1 > sleep_seconds)
    {
        stop_loop = 1;
    }
  }
  else
  {
    sleep_seconds = 1;
  }

  //  const char * format="%'-16llu\t";

  // per core variable definition

  uint64_t var_version_id	 = reg_read(ADDR_VERSION_ID);
  if ( ((var_version_id>>60)&MASK4) != 8)
  {
    printf("ERROR: Current HW version is %ju, sn_exceptions may be used with Ekaline Multi Core design using FiberBlaze card only (HW version 8)\n",(var_version_id>>60)&MASK4);
    return 0;
  }

  if (system("clear")) die("\'clear\' failed");

  int animation = 0;

  while (1)
    {
     // Reading All central regs
      uint64_t var_global_shadow         = reg_read(ADDR_INTERRUPT_SHADOW_RO);
      uint64_t var_core_shadow;

      if (system("clear")) die("\'clear\' failed");;

      if (var_global_shadow)
	{
	  printf ("--- Global Exceptions --\n\n");
	  for(curr_core = 0; curr_core < NUM_OF_CORES; curr_core++)
	    if ((var_global_shadow>>curr_core)&0x1) printf("Bit %d: Core%d exception, will be resolved below\n",curr_core,curr_core);
          if ((var_global_shadow>>6)&0x1)  printf("Bit 6: Register access interface became full\n" );
          if ((var_global_shadow>>9)&0x1)  printf("Bit 9: CTX read reply FIFO overrun, FATAL\n" );
          if ((var_global_shadow>>10)&0x1) printf("Bit 10: HASH read reply FIFO overrun, FATAL\n" );
          if ((var_global_shadow>>12)&0x1) printf("Bit 12: Lookup counter read reply FIFO overrun, FATAL\n" );
          if ((var_global_shadow>>13)&0x1) printf("Bit 13: Hash table update overrun, happens if is updated too fast\n" );
          if ((var_global_shadow>>14)&0x1) printf("Bit 14: Context table update overrun, happens if the table is updated too fast\n" );

	  if ((var_global_shadow>>0)&0x3f) printf ("\n--- Core Exceptions --\n");
	  for(curr_core = 0; curr_core < NUM_OF_CORES; curr_core++){
	    if ((var_global_shadow>>curr_core)&0x1) {
	      printf("\nResolving exception for Core%d\n",curr_core);
	      var_core_shadow = reg_read(ADDR_INTERRUPT_0_SHADOW_RO+curr_core*0x1000);
	      
	      if ((var_core_shadow>>0)&0x1)  printf("Bit 0: RX port overrun, at least one packet was dropped\n");
	      if ((var_core_shadow>>1)&0x1)  printf("Bit 1: MD parser error, happens if the MD parser reached unknown state while parsing MD\n");
	      if ((var_core_shadow>>2)&0x1)  printf("Bit 2: Sequence number overflow - sequence number crossed 64bit value\n");
	      if ((var_core_shadow>>3)&0x1)  printf("Bit 3: Overrun in MD fifo, FATAL\n");
	      if ((var_core_shadow>>4)&0x1)  printf("Bit 4: RX CRC error was detected\n");
	      if ((var_core_shadow>>7)&0x1)  printf("Bit 7: Software DirectTCP remote ACK table table update overrun, happens if table is updated too fast\n");
	      if ((var_core_shadow>>8)&0x1)  printf("Bit 8: Software DirectTCP local SEQ table table update overrun, happens if table is updated too fast\n");
	      if ((var_core_shadow>>9)&0x1)  printf("Bit 9: Controller watchdog expired when the controller is armed, but data structures were not updated in time by the host SW\n");
	      if ((var_core_shadow>>10)&0x1)  printf("Bit 10: Session table update overrun, happens if  table is updated too fast\n");
	      if ((var_core_shadow>>11)&0x1)  printf("Bit 11: Ultrafast fire TCP table update overrun, happens if table is updated too fast\n");
	      if ((var_core_shadow>>12)&0x1)  printf("Bit 12: Mismatch in security ID in controller, compared between MD and Context\n");
	      if ((var_core_shadow>>13)&0x1)  printf("Bit 13: Mismatch between MD serial number and subscription lookup serial number\n");
	      if ((var_core_shadow>>14)&0x1)  printf("Bit 14: Controller overfire, reached 16 fires when running on session list\n");
	      if ((var_core_shadow>>15)&0x1)  printf("Bit 15: Null session pointer specified in multicast group to session table, there must be at least one session\n");
	      if ((var_core_shadow>>16)&0x1)  printf("Bit 16: Ultrafast fire tried to fire, but waited too long for available line\n");
	      
	    }
	  }
	  
	}

      if (animation % 2)
	printf("\rPolling exceptions \\");
      else
	printf("\rPolling exceptions /");

      animation++;

      fflush(stdout); 
      
      for( int a = 0; a < sleep_seconds; a=a+1){
	
	sleep (1);
      }

    if (stop_loop)
        break;
    }

  printf(RESET);
  return 0;
}

#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>

#include "ekaline.h"
#include "smartnic.h"

#include "eka_dev.h"

bool eka_get_exceptions (eka_dev_t* dev) {
  return eka_read(dev,EKA_ADDR_INTERRUPT_SHADOW_RO) == 0 ? false : true;
}

void eka_print_exceptions (eka_dev_t* dev) {
  uint64_t var_global_shadow = eka_read(dev,EKA_ADDR_INTERRUPT_SHADOW_RO);
  uint64_t var_core_shadow;
  int curr_core;

  printf("--- Global Exceptions --\n\n");
  for(curr_core = 0; curr_core < dev->hw.enabled_cores; curr_core++)
    if ((var_global_shadow>>curr_core)&0x1) printf("Bit %d: Core%d exception, will be resolved below\n",curr_core,curr_core);
  if ((var_global_shadow>>6)&0x1)  printf("Bit 6: Register access interface became full\n" );
  if ((var_global_shadow>>9)&0x1)  printf("Bit 9: CTX read reply FIFO overrun, FATAL\n" );
  if ((var_global_shadow>>10)&0x1) printf("Bit 10: HASH read reply FIFO overrun, FATAL\n" );
  if ((var_global_shadow>>12)&0x1) printf("Bit 12: Lookup counter read reply FIFO overrun, FATAL\n" );
  if ((var_global_shadow>>13)&0x1) printf("Bit 13: Hash table update overrun, happens if is updated too fast\n" );
  if ((var_global_shadow>>14)&0x1) printf("Bit 14: Context table update overrun, happens if the table is updated too fast\n" );

  if ((var_global_shadow>>0)&0x3f) printf("\n--- Core Exceptions --\n");
  for(curr_core = 0; curr_core < dev->hw.enabled_cores; curr_core++){
    if ((var_global_shadow>>curr_core)&0x1) {
      printf("\nResolving exception for Core%d\n",curr_core);
      var_core_shadow = eka_read(dev,EKA_ADDR_INTERRUPT_0_SHADOW_RO+curr_core*0x1000);
	      
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

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <sys/ioctl.h>

#include "smartnic.h"
#include "eka.h"
#include "ctls.h"

#define NUM_OF_CORES 6
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"
#define FREQUENCY 161

// global configurations
#define ADDR_VERSION_ID         0xf0ff0
#define ADDR_VERSION_HIGH_ID    0xf0ff8
#define ADDR_VERSION_HIGHTWO_ID 0xf0fe0

#define MASK64 0xffffffffffffffff
#define MASK32 0xffffffff
#define MASK24 0xffffff
#define MASK16 0xffff
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

int main(int argc, char *argv[]){
  DeviceId = SN_OpenDevice(NULL, NULL);
  if (DeviceId == NULL) {
    fprintf( stderr, "Cannot open XYU device. Is driver loaded?\n" );
    exit( -1 );
  }

  uint64_t var_version_id	 = reg_read(ADDR_VERSION_ID);
  uint64_t var_version_id_high = reg_read(ADDR_VERSION_HIGH_ID);
  uint64_t var_version_id_hightwo = reg_read(ADDR_VERSION_HIGHTWO_ID);


  uint64_t ekaline_git_id = ((var_version_id>>0)&MASK32);
  int day_id         = ((var_version_id>>32)&MASK8);
  int month_id       = ((var_version_id>>40)&MASK8);
  int year_id        = ((var_version_id>>48)&MASK8);
  int feed_id        = ((var_version_id>>56)&MASK4);
  int arch_id        = ((var_version_id>>60)&MASK4);

  uint64_t fb_git_id      = ((var_version_id_high>>0)&MASK32);
  int timem_id        = ((var_version_id_high>>32)&MASK8);
  int timeh_id        = ((var_version_id_high>>40)&MASK8);
  int feature_id     = ((var_version_id_high>>48)&MASK8);
  int number_of_cores = ((var_version_id_high>>56)&MASK8);
  int number_of_ctx_threads = ((var_version_id_hightwo>>0)&MASK8);

  char arch_string[256], feed_string[256], feature_string[256];

  switch(arch_id) {
  case 5 :
    strcpy(arch_string, "Silicom 2.4.2"); 
    break;
  case 6 :
    strcpy(arch_string, "Silicom 2.5.0"); 
    break;
  case 7 :
    strcpy(arch_string, "Silicom 2.5.2"); 
    break;
  case 8 :
    strcpy(arch_string, "Silicom 3.0.0"); 
    break;
  default :
    strcpy(arch_string, "Unknown"); 
  }

  switch(feed_id) {
  case 0 :
    strcpy(feed_string, "NASDAQ"); 
    break;
  case 1 :
    strcpy(feed_string, "MIAX"); 
    break;
  case 2 :
    strcpy(feed_string, "PHLX"); 
    break;
  case 3 :
    strcpy(feed_string, "ISE/GEMX/MRX"); 
    break;
  case 4 :
    strcpy(feed_string, "BZX/C2/EDGEX"); 
    break;
  default :
    strcpy(feed_string, "Unknown"); 
  }

  switch(feature_id) {
  case 0 :
    strcpy(feature_string, "GEM Top Quote Feed only"); 
    break;
  case 1 :
    strcpy(feature_string, "GEM Top Quote Feed, GEM Order Feed"); 
    break;
  case 2 :
    strcpy(feature_string, "GEM Top Quote Feed, GEM Order Feed, GEM Price RoundDown"); 
    break;
  case 3 :
    strcpy(feature_string, "GEM Top Quote Feed, GEM Order Feed, GEM Price RoundDown, 768k Securities"); 
    break;
  case 4 :
    strcpy(feature_string, "... no multifire on orders > 10"); 
    break;
  case 5 :
    strcpy(feature_string, "... no multifire on orders > 10, secctx one size, AON support"); 
    break;
  case 6 :
    strcpy(feature_string, "Multifire on orders < 10, secctx one size, AON support, P4 price and size logic"); 
    break;
  case 7 :
    strcpy(feature_string, "size > 10: single fire from core0"); 
    break;
  case 8 :
    strcpy(feature_string, "SQF: order followed by delete"); 
    break;
  case 9 :
    strcpy(feature_string, "ACL"); 
    break;
  case 16 :
    strcpy(feature_string, "1ms"); 
    break;
  case 17 :
    strcpy(feature_string, "1ms, new channels"); 
    break;
  case 18 :
    strcpy(feature_string, "fastpath ip header"); 
    break;
  case 20 :
    strcpy(feature_string, "WC chipscope"); 
    break;
  default :
    strcpy(feature_string, "Unknown"); 
  }

  printf("\nVersion registers:\t0x%jx 0x%jx\n", var_version_id, var_version_id_high);
  printf("Build Date:\t\t%x/%x/20%x %x:%x\n\n",day_id, month_id, year_id, timeh_id, timem_id);

  printf("Ekaline GIT ID:\t\t0x%jx\n",ekaline_git_id);
  printf("SmartNIC GIT ID:\t0x%jx\n\n",fb_git_id);

  printf("Architecture ID:\t%d (%s)\n",arch_id, arch_string);
  printf("Physical Cores:\t\t%d\n",number_of_cores);
  printf("Supported CTX Threads:\t%d\n",number_of_ctx_threads);

  printf("Feed ID:\t\t%d (%s)\n",feed_id, feed_string);
  printf("Feature ID:\t\t%d (%s)\n",feature_id, feature_string);

  
  int fd = SN_GetFileDescriptor(DeviceId);
  eka_ioctl_t state;
  state.cmd = EKA_VERSION;
  ioctl(fd,SC_IOCTL_EKALINE_DATA,&state);
  printf ("%s\n",state.eka_version);
  printf ("%s\n",state.eka_release);

  printf(RESET);
  SN_CloseDevice(DeviceId);
      
  return 0;
}

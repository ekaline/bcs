#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <string.h>

#include "eka.h"
#include "smartnic.h"
#include "ctls.h"

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stdout); fflush(stderr); exit(1); }

int main(int argc, char **argv){
  if (argc != 2) {
    printf ("USAGE: %s: 0 or 1\n",argv[0]);
    return 0;
  }
  uint8_t on_off = atoi(argv[1]);
  if (on_off == 0) printf ("Setting drivers debug prints to OFF\n");
  else if (on_off == 1) printf ("Setting drivers debug prints to ON\n");
  else on_error ("%s: Invalid parameter %s\n",argv[0],argv[1]);

  SN_DeviceId DeviceId = SN_OpenDevice(NULL, NULL);
  if (DeviceId == NULL) on_error ("Cannot open Smartnic device\n");
  int fd = SN_GetFileDescriptor(DeviceId);

  eka_ioctl_t state = {};
  memset(&state,0,sizeof(eka_ioctl_t));
  for (int i=0; i<2;i++) {
    state.paramA = i;
    state.cmd = on_off ? EKA_DEBUG_ON : EKA_DEBUG_OFF;
    int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state);
    if (rc < 0) on_error("%s: error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_DEBUG_ON/OFF (%d) on feth%u\n",argv[0],state.cmd,i);
  }

  SN_CloseDevice(DeviceId);
  return 0;
}

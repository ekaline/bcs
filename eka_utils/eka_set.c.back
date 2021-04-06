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
  if (argc != 7) {
    printf ("USAGE: %s: NIF SESSION_ID IPv4_SADDR IPv4_DADDR TCP_SPORT TCP_DPORT\n",argv[0]);
    return 0;
  }

  struct in_addr sa;
  struct in_addr da;
  printf ("setting session for: nif=%s, eks=%s, sa=%s, da=%s, sp=%s, dp=%s\n",argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);

  if (inet_aton(argv[3], &sa) == 0) {
    fprintf(stderr, "Invalid source address\n");
    exit(-1);
  }
  if (inet_aton(argv[4], &da) == 0) {
    fprintf(stderr, "Invalid dest address\n");
    exit(-1);
  }
  eka_ioctl_t state;
  memset(&state,0,sizeof(eka_ioctl_t));
  state.cmd = EKA_SET;
  state.nif_num = atoi(argv[1]);
  state.session_num = atoi(argv[2]);
  state.eka_session.active = 1;
  state.eka_session.ip_saddr = sa.s_addr;
  state.eka_session.ip_daddr = da.s_addr;
  state.eka_session.tcp_sport = htons(atoi(argv[5]));
  state.eka_session.tcp_dport = htons(atoi(argv[6]));


  SN_DeviceId DeviceId = SN_OpenDevice(NULL, NULL);
  if (DeviceId == NULL) {
    fprintf( stderr, "Cannot open XYU device. Is driver loaded?\n" );
    exit( -1 );
  }

  int fd = SN_GetFileDescriptor(DeviceId);

  int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state);

  if (rc < 0) on_error("%s: error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_SET\n",__func__);

  SN_CloseDevice(DeviceId);
  return 0;
}

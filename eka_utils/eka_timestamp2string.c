#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>

#include "eka_nw_headers.h"
#include "eka_fh_nom_messages.h"

#define on_error(...) { fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }
#define EFH_PRICE_SCALE 1


uint64_t get_ts(uint8_t* m) {
  uint64_t ts_tmp = 0;
  memcpy((uint8_t*)&ts_tmp+2,m+3,6);
  return be64toh(ts_tmp);
}

static int ts_ns2str(char* dst, uint64_t ts) {
  uint ns = ts % 1000;
  uint64_t res = (ts - ns) / 1000;
  uint us = res % 1000;
  res = (res - us) / 1000;
  uint ms = res % 1000;
  res = (res - ms) / 1000;
  uint s = res % 60;
  res = (res - s) / 60;
  uint m = res % 60;
  res = (res - m) / 60;
  uint h = res % 24;
  sprintf (dst,"%02d:%02d:%02d.%03d.%03d.%03d",h,m,s,ms,us,ns);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) on_error ("timestamp agrument is not specified");

  uint64_t timestamp = strtoul(argv[1],NULL,10);
  
  char ts_str[32] = {};
  ts_ns2str(ts_str,timestamp);
  printf ("%s = %ju = %s\n",argv[1],timestamp,ts_str);
  return 0;
}


#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include "smartnic.h"
//#include "EkaHwInternalStructs.h"
#include <string.h>
#include "ctls.h"
#include "eka.h"
#include <sys/ioctl.h>

#include "EkaHwCaps.h"
#include "eka_macros.h"
#include "EkaHwExpectedVersion.h"

#ifndef SMARTNIC_EKALINE_DATA
#define SMARTNIC_EKALINE_DATA SC_IOCTL_EKALINE_DATA
#endif

#undef EKA_LOG
#define EKA_LOG(...) { printf(__VA_ARGS__); printf("\n"); }

int main(int argc, char *argv[]){
  EkaHwCaps* ekaHwCaps = new EkaHwCaps(NULL);
  if (ekaHwCaps == NULL) on_error("ekaHwCaps == NULL");

  ekaHwCaps->printStdout();
  ekaHwCaps->printDriverVer();

  return 0;
}

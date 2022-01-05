#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <thread>
#include <assert.h>
#include <chrono>
#include <sys/ioctl.h>
#include <string>

//#include <asm/i387.h>
#include <x86intrin.h>

#include "smartnic.h"
#include "ctls.h"
#include "eka.h"
#include "eka_data_structs.h"

volatile uint64_t * EkalineGetWcBase(SC_DeviceId deviceId);

/* *************************************************************** */
int main(int argc, char *argv[]) {
  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");

  int fd = SC_GetFileDescriptor(dev_id);
  eka_ioctl_t state = {};
  state.cmd = EKA_IOREMAP_WC;
  state.paramA = (uint64_t)(new uint64_t);
  if (state.paramA == 0) on_error("state.paramA == 0");
  
  int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state);
  if (rc < 0) on_error("error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_IOREMAP_WC");
  TEST_LOG("EKA_IOREMAP_WC IOCTL succeeded");


  volatile uint64_t* a2wr_fromEkalineGetWcBase = EkalineGetWcBase(dev_id);
  volatile uint64_t* a2wr_fromIOCTL = (volatile uint64_t*)(*(uint64_t*)state.paramA);
  volatile uint64_t* a2wr = a2wr_fromEkalineGetWcBase;

  TEST_LOG("EkalineGetWcBase = %p, state.wcattr.bar0_wc_va = %jx",
	   a2wr_fromEkalineGetWcBase,(uint64_t)a2wr_fromIOCTL);

  if (a2wr_fromEkalineGetWcBase != a2wr_fromIOCTL) {
    on_error("a2wr_fromEkalineGetWcBase %jx != a2wr_fromIOCTL %jx",
	     (uint64_t)a2wr_fromEkalineGetWcBase,(uint64_t)a2wr_fromIOCTL);
  } else {
    TEST_LOG(GRN "EKALINE WC mmap succeeded: WC base addr = %p" RESET,a2wr);
  }

  /* ------------------------------------------------------------------ */
  uint64_t __attribute__ ((aligned(0x100))) data[] = {
    0x1111111111111111,
    0x2222222222222222,
    0x3333333333333333,
    0x4444444444444444,
    0x5555555555555555,
    0x6666666666666666,
    0x7777777777777777,
    0x8888888888888888,

    0x9999999999999999,
    0xaaaaaaaaaaaaaaaa,
    0xbbbbbbbbbbbbbbbb,
    0xcccccccccccccccc,
    0xdddddddddddddddd,
    0xeeeeeeeeeeeeeeee,
    0xffffffffffffffff,
    0x1000100010001001,

    0x1111111111111111,
    0x2222222222222222,
    0x3333333333333333,
    0x4444444444444444,
    0x5555555555555555,
    0x6666666666666666,
    0x7777777777777777,
    0x8888888888888888,

    0x9999999999999999,
    0xaaaaaaaaaaaaaaaa,
    0xbbbbbbbbbbbbbbbb,
    0xcccccccccccccccc,
    0xdddddddddddddddd,
    0xeeeeeeeeeeeeeeee,
    0xffffffffffffffff,
    0x1000100010001001
  };


  TEST_LOG ("memcpy from %p to %p size = %ju Bytes",data,a2wr,sizeof(data));
  memcpy((void*)a2wr,data,sizeof(data));

  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  return 0;
}



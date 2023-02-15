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
#include "eka_macros.h"

volatile uint64_t * EkalineGetWcBase(SC_DeviceId deviceId);

#define TCP_FAST_SEND_SP_BASE (0x8000 / 8)

inline void copy_to_atomic(volatile uint64_t * __restrict dst, 
			   const void *__restrict srcBuf, const size_t len) {

  auto atomicDst = (std::atomic<uint64_t> *__restrict) dst;
  auto src = (const uint64_t *__restrict) srcBuf;
  const uint64_t *endsrc = src+len;
  while (src < endsrc) {
    //    dst_a->store( *src, std::memory_order_seq_cst );
    atomicDst->store( *src, std::memory_order_release );
    atomicDst++; src++;
  }
}
/* *************************************************************** */
int main(int argc, char *argv[]) {
  const uint64_t HwWcTestAddr = 0x20000;

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  volatile uint64_t* a2wr = EkalineGetWcBase(dev_id);

  /* ------------------------------------------------------------------ */
  const size_t RegionSize = 2 * 1024; 
  
  uint8_t __attribute__ ((aligned(0x100))) data[RegionSize] = {};
  uint8_t __attribute__ ((aligned(0x100))) test_data[RegionSize] = {};

  for (size_t i = 0; i < RegionSize; i ++) {
    data[i] = 'a' + rand() % ('z' -'a' + 1);
  }

  hexDump("Data",data,sizeof(data));
  
  copy_to_atomic(a2wr,data,sizeof(data)/8);

  uint64_t srcAddr = HwWcTestAddr / 8;
  uint64_t* dstAddr = (uint64_t*)&test_data;
  for (uint w = 0; w < roundUp64(sizeof(test_data)) / 8; w++)
    SN_ReadUserLogicRegister(dev_id, srcAddr++, dstAddr++);

  hexDump("test_data",test_data,sizeof(data));

  if (memcmp(data,test_data,RegionSize)) {
    TEST_LOG("Failed");
  } else {
    TEST_LOG("Succeeded");
  }
  
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  return 0;
}



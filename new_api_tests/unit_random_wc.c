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
    atomicDst->store( *src, std::memory_order_seq_cst );
    //atomicDst->store( *src, std::memory_order_release );
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
  const size_t BlockSize  = 64; // 1 Burst

  uint8_t __attribute__ ((aligned(0x100))) data[RegionSize] = {};
  uint8_t __attribute__ ((aligned(0x100))) test_data[RegionSize] = {};

  for (auto j = 0; j < 1; j++) {
    /* for (size_t i = 0; i < RegionSize; i ++) { */
    /*   data[i] = 'a' + rand() % ('z' -'a' + 1); */
    /* } */

    for (size_t b = 0; b < RegionSize/BlockSize; b ++) {
      for (size_t w = 0; w < BlockSize/sizeof(uint64_t); w ++) {
	size_t flatWordIdx = b * BlockSize + w * sizeof(uint64_t);
	sprintf((char*)&data[flatWordIdx],"b%2ju,w%ju;",b,w);
      }
    }
    
    copy_to_atomic(a2wr,data,sizeof(data)/8);

    uint64_t srcAddr = HwWcTestAddr / 8;
    uint64_t* dstAddr = (uint64_t*)&test_data;
    for (uint w = 0; w < roundUp64(sizeof(test_data)) / 8; w++)
      SN_ReadUserLogicRegister(dev_id, srcAddr++, dstAddr++);


    int r = memcmp(data,test_data,RegionSize);
    if (r != 0) {
      TEST_LOG("Failed: r = %d",r);
      hexDump("Data",data,sizeof(data));
      hexDump("test_data",test_data,sizeof(data));
      break;
      
    } else {
      //      TEST_LOG("%d: Succeeded",j);
    }
    sleep(0);
  }
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  return 0;
}



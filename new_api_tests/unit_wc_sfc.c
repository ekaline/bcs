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
  for (size_t i = 0; i < len/8; i ++) {
    if (i % 64 == 0) _mm_sfence();
    //atomicDst->store( *src, std::memory_order_seq_cst );
    atomicDst->store( *src, std::memory_order_release );
    atomicDst++; src++;
  }
}

#define _RESTRICT 1

#if _RESTRICT
inline void copy__m256i(volatile uint64_t * __restrict dst, 
			      const void *__restrict srcBuf,
			      const size_t len) {
  auto dst256 = (__m256i* __restrict) dst;
  auto src256 = (__m256i* __restrict) srcBuf;
  for (uint i = 0; i < len/32; i++) {
    _mm256_store_si256(dst256++, *src256++);
    __asm__ __volatile__ ("":::"memory");
    if (i % 2)
      _mm_mfence();

  }
}
#else
inline void copy__m256i(volatile uint64_t* dst, 
			      const void *srcBuf,
			      const size_t len) {
  auto dst256 = (__m256i* ) dst;
  auto src256 = (const __m256i* ) srcBuf;
  for (uint i = 0; i < len/32; i++) {
    _mm256_store_si256(dst256++, *src256++);
    __asm__ __volatile__ ("":::"memory");
  }
}
#endif

inline void copy__sameAddr(volatile uint64_t* dst, 
			   const void *srcBuf,
			   const size_t len) {
  auto atomicDst = (std::atomic<uint64_t> *__restrict) dst;
  auto src = (const uint64_t *__restrict) srcBuf;
  for (size_t i = 0; i < len/8; i ++) {
    if (i > 0 && i % 8 == 0) {
       atomicDst = (std::atomic<uint64_t> *__restrict) dst + 1;
    }
    atomicDst->store( *src, std::memory_order_release );
    atomicDst++; src++;

  }
}

inline void copyBuf(volatile uint64_t* dst, 
			      const void *srcBuf,
			      const size_t len) {
  
  copy_to_atomic(dst,srcBuf,len);
  //  copy__m256i(dst,srcBuf,len);
  //copy__sameAddr(dst,srcBuf,len);

}

/* *************************************************************** */
int main(int argc, char *argv[]) {
  const uint64_t HwWcTestAddr = 0x20000;

  struct WcDesc {
    uint64_t pad18 : 18;
    uint64_t descH : 46;
    uint64_t descL : 18;
    uint64_t nBytes: 12;
    uint64_t addr  : 32;
    uint64_t opc   :  2;
  } __attribute__ ((aligned(sizeof(uint64_t)))) __attribute__((packed));


  /* printf ("sizeof(WcDesc) = %ju\n",sizeof(WcDesc)); */
  /* return(0); */
  
  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  volatile uint64_t* a2wr = EkalineGetWcBase(dev_id);

  /* ------------------------------------------------------------------ */
  const size_t RegionSize = 1 * 1024; 

  uint8_t __attribute__ ((aligned(0x100))) data[RegionSize] = {};
  uint8_t __attribute__ ((aligned(0x100))) test_data[RegionSize] = {};

#define _RANDOM_DATA 0
  
  for (auto j = 0; j < 1000; j++) {
#if _RANDOM_DATA
    for (size_t i = 0; i < RegionSize; i ++) {
      data[i] = 'a' + rand() % ('z' -'a' + 1);
    }
#else
    const size_t BlockSize  = 64; // 1 Burst

    for (size_t b = 0; b < RegionSize/BlockSize; b ++) {
      for (size_t w = 0; w < BlockSize/sizeof(uint64_t); w ++) {
    	size_t flatWordIdx = b * BlockSize + w * sizeof(uint64_t);
    	sprintf((char*)&data[flatWordIdx],"b%2ju,w%ju;",b,w);
      }
    }
#endif    
    //    copyBuf(a2wr,data,sizeof(data));

    WcDesc desc = {
		   .nBytes = sizeof(data) & 0xFFF,
		   .addr = (uint64_t)a2wr,
		   .opc = 2,
    };
    
    copyBuf(a2wr,&desc,sizeof(desc));
    __sync_synchronize(); 
    copyBuf((volatile uint64_t*)(a2wr + 8),data,sizeof(data));
    
    uint64_t srcAddr = HwWcTestAddr / 8;
    uint64_t* dstAddr = (uint64_t*)&test_data;
    for (uint w = 0; w < roundUp64(sizeof(test_data)) / 8; w++)
      SN_ReadUserLogicRegister(dev_id, srcAddr++, dstAddr++);


    int r = memcmp(data,test_data,RegionSize);
    if (r) {
      hexDump("Data",data,sizeof(data));
      hexDump("test_data",test_data,sizeof(data));
      TEST_LOG("Iteration %d: Failed: r = %d",j,r);
      break;
      
    } else {
      //      TEST_LOG("Iteration %d: Succeeded",j);
    }
    usleep(100);
  }
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  return 0;
}



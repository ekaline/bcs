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
#include <vector>

//#include <asm/i387.h>
#include <x86intrin.h>
#include <emmintrin.h>  // Needed for _mm_clflush()

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
    atomicDst->store( *src, std::memory_order_release );
    atomicDst++; src++;
  }
}

inline void copyBuf(volatile uint64_t* dst, 
			      const void *srcBuf,
			      const size_t len) {
  
  copy_to_atomic(dst,srcBuf,len);
  //  __sync_synchronize();
  _mm_clflush(srcBuf);
      
}

struct WcDesc {
  uint64_t desc : 64;
  uint64_t nBytes: 12;
  uint64_t addr  : 32;
  uint64_t opc   :  2;
  uint64_t pad18 : 18;
} __attribute__((packed));

static const uint64_t Iterations = 100 * 1024 * 1024; 
static const int DataMaxSize = 2048 - 64; 
static const uint64_t HwWcTestAddr = 0x20000;

static volatile bool keep_work = true;

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
  return;
}

void testThread(int thrId, SC_DeviceId dev_id,
	      volatile uint64_t* wcBaseAddr) {

  volatile uint64_t* a2wr = wcBaseAddr + thrId * 0x800;
  uint64_t testMemOffs = thrId * 2048;
  uint64_t i;
  for (i = 0; keep_work && i < Iterations; i++) {
    uint8_t data[DataMaxSize];
    size_t dataLen = roundUp64(rand() % DataMaxSize);
    for (size_t c = 0; c < dataLen; c ++) {
      data[c] = 'a' + rand() % ('z' -'a' + 1);
    }

    WcDesc __attribute__ ((aligned(0x100)))
      desc = {
	      .nBytes = dataLen & 0xFFF,
	      .addr = testMemOffs,
	      .opc = 2,
    };

    copyBuf(a2wr,&desc,sizeof(desc));
    copyBuf((volatile uint64_t*)(a2wr + 8),data,dataLen);

    uint8_t rdData[DataMaxSize] = {};

    uint64_t srcAddr = (HwWcTestAddr + testMemOffs) / 8;
    uint64_t* dstAddr = (uint64_t*)rdData;
    for (uint w = 0; w < dataLen / 8; w++)
      SN_ReadUserLogicRegister(dev_id, srcAddr++, dstAddr++);


    int r = memcmp(data,rdData,dataLen);
    if (r) {
      hexDump("Data",data,dataLen);
      hexDump("rdData",rdData,dataLen);
      TEST_LOG("Thread %d, Iteration %ju: Failed: dataLen = %ju, &desc = %p, sizeof(desc)=%ju",
	       thrId,i,dataLen,&desc,sizeof(desc));
      keep_work = false;
      return;
    } 

    if ( i % 10000 == 0) printf("%2d: %ju\n",thrId,i);
  }
  printf("Thread %2d: passed %ju iterations\n",thrId,i);
  return;
}

/* *************************************************************** */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  time_t t;
  srand((unsigned) time(&t));


  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  volatile uint64_t* wcBaseAddr = EkalineGetWcBase(dev_id);

  const int NumThreads = 1;

  std::thread thr[NumThreads];
  /* ------------------------------------------------------------------ */
  for (int i = 0; i < NumThreads; i++) {
    thr[i] = std::thread(testThread,i,dev_id,wcBaseAddr);    
  }

  for (int i = 0; i < NumThreads; i++) {
    thr[i].join();
  }
  
  if (keep_work) 
    TEST_LOG("%d Threads * %ju Iterations Passed\n",
	     NumThreads, Iterations);

  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS)
    on_error("Error on SC_CloseDevice");
  return 0;
}



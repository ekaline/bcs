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
//#include "eka_data_structs.h"

volatile uint64_t * EkalineGetWcBase(SC_DeviceId deviceId);

static const uint64_t HwWcTestAddr = 0x20000;

#define TCP_FAST_SEND_SP_BASE (0x8000 / 8)
#define TEST_LOG(...) { printf("%s@%s:%d: ",__func__,__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); }

#define on_error(...) { fprintf(stderr, "FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }
/* #define TEST_LOG(...) { fprintf(stderr, "%s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n"); } */

void hexDump (const char* desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    if (desc != NULL) printf ("%s:\n", desc);
    if (len == 0) { printf("  ZERO LENGTH\n"); return; }
    if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) printf ("  %s\n", buff);
            printf ("  %04x ", i);
        }
        printf (" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
        else buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) { printf ("   "); i++; }
    printf ("  %s\n", buff);
}

void print_usage(char* cmd) {
  printf ("USAGE: %s -t <TEST_TYPE>\n",cmd);
  printf("\t\t\t\t TEST_TYPE options:\n");
  printf("\t\t\t\t\t<memcpy>\n");
  printf("\t\t\t\t\t<_mm_stream_si32>\n");
  printf("\t\t\t\t\t<_mm256_store_si256>\n");
  printf("\t\t\t\t\t<loop> - for loop of _mm256_store_si256\n");
  exit (1);    
}

void copy_to_atomic(std::atomic<uint64_t> *__restrict dst_a, 
                      const uint64_t *__restrict src, size_t len) {
  const uint64_t *endsrc = src+len;
    while (src < endsrc) {
        dst_a->store( *src, std::memory_order_release );
        dst_a++; src++;
    }
}
/* *************************************************************** */
int main(int argc, char *argv[]) {

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  volatile uint64_t* a2wr = EkalineGetWcBase(dev_id);

  TEST_LOG("a2wr = %p",a2wr);

  /* ------------------------------------------------------------------ */
  const size_t RegionSize = 64*13;
  const size_t BlockSize  = 64; // 1 Burst
  const size_t WordSize   = 8;  // 1 Memory word

  
  uint64_t __attribute__ ((aligned(0x100))) data[RegionSize/WordSize] = {};
  uint64_t __attribute__ ((aligned(0x100))) test_data[RegionSize/WordSize] = {};

  for (size_t b = 0; b < RegionSize/BlockSize; b ++) {
    for (size_t w = 0; w < BlockSize/WordSize; w ++) {
      size_t flatWordIdx = b * (BlockSize/WordSize) + w;
      sprintf((char*)&data[flatWordIdx],"B%2ju,B%ju;",b,w);
    }
  }
    
  //  hexDump("Data",data,sizeof(data));
  //  memcpy((void*)a2wr,data,sizeof(data));

  //  volatile uint64_t* b2wr = a2wr;
  std::atomic<long unsigned int>* b2wr = (std::atomic<long unsigned int>*)a2wr;
  copy_to_atomic(b2wr,data,sizeof(data)/8);
  /*
    for (size_t b = 0; b < RegionSize/BlockSize; b ++) {
    //    memcpy((void*)b2wr,&data[b * (BlockSize/WordSize)],BlockSize);
          copy_to_atomic(b2wr,&data[b * (BlockSize/WordSize)],BlockSize);
    //    printf ("data add = %ju\n",b * (BlockSize/WordSize));
    //        hexDump("Block",&data[b * (BlockSize/WordSize)],BlockSize);
    //    _mm_sfence();
    //        _mm_mfence();

    b2wr += (BlockSize/WordSize);
  }
  */

  //dumper
  
  uint words2read = sizeof(test_data) / 8 + !!(sizeof(test_data) % 8);
  uint64_t srcAddr = HwWcTestAddr / 8;
  uint64_t* dstAddr = (uint64_t*)&test_data;
  for (uint w = 0; w < words2read; w++)
    SN_ReadUserLogicRegister(dev_id, srcAddr++, dstAddr++);

  hexDump("Test Data",test_data,sizeof(test_data));

  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  return 0;
}



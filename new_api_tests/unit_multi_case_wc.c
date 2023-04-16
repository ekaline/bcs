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

#define TCP_FAST_SEND_SP_BASE (0x8000 / 8)
#define TEST_LOG(...) { printf("%s@%s:%d: ",__func__,__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); }

#define on_error(...) { fprintf(stderr, "FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }
/* #define TEST_LOG(...) { fprintf(stderr, "%s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n"); } */

/* void hexDump (const char* desc, void *addr, int len) { */
/*     int i; */
/*     unsigned char buff[17]; */
/*     unsigned char *pc = (unsigned char*)addr; */
/*     if (desc != NULL) printf ("%s:\n", desc); */
/*     if (len == 0) { printf("  ZERO LENGTH\n"); return; } */
/*     if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; } */
/*     for (i = 0; i < len; i++) { */
/*         if ((i % 16) == 0) { */
/*             if (i != 0) printf ("  %s\n", buff); */
/*             printf ("  %04x ", i); */
/*         } */
/*         printf (" %02x", pc[i]); */
/*         if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.'; */
/*         else buff[i % 16] = pc[i]; */
/*         buff[(i % 16) + 1] = '\0'; */
/*     } */
/*     while ((i % 16) != 0) { printf ("   "); i++; } */
/*     printf ("  %s\n", buff); */
/* } */

/* void print__m256i(__m256i* addr, __m256i in) { */
/*   uint64_t *v64val = (uint64_t*) &in; */
/*   TEST_LOG("%p : %016jx %016jx %016jx %016jx\n", addr, v64val[0], v64val[1], v64val[2], v64val[3]); */
/* } */

void print_usage(char* cmd) {
  printf ("USAGE: %s -t <TEST_TYPE>\n",cmd);
  printf("\t\t\t\t TEST_TYPE options:\n");
  printf("\t\t\t\t\t<memcpy>\n");
  printf("\t\t\t\t\t<_mm_stream_si32>\n");
  printf("\t\t\t\t\t<_mm256_store_si256>\n");
  printf("\t\t\t\t\t<loop> - for loop of _mm256_store_si256\n");
  exit (1);    
}

/* *************************************************************** */
int main(int argc, char *argv[]) {
  if (argc < 2) print_usage(argv[0]);

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");
  volatile uint64_t* a2wr = EkalineGetWcBase(dev_id);

  TEST_LOG("a2wr = %p",a2wr);

  /* ------------------------------------------------------------------ */
  int opt; 
  std::string test = std::string("NO TEST");
  while((opt = getopt(argc, argv, ":t:h")) != -1) {  
    switch(opt) {
    case 't':  
      test = std::string(optarg);
      printf("running test: %s\n", test.c_str());  
      break;
    case 'h':  

    default: 
      print_usage(argv[0]);
      break;
    }
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

  if (test == std::string("memcpy")) {
    printf ("Running \"memcpy\" test\n");
    TEST_LOG ("memcpy from %p to %p size = %ju Bytes",data,a2wr,sizeof(data));
    //    memcpy((uint8_t*)a2wr,data,sizeof(data));
    memcpy((void*)a2wr,data,sizeof(data));
    /* memcpy(a2wr,data,sizeof(data) / 2); */
    /* memcpy(a2wr + (sizeof(data) / 2),data + (sizeof(data) / 2),sizeof(data) / 2); */


  } else if (test == std::string("_mm_stream_si32")) {
    printf ("Running \"_mm_stream_si32\" test\n");

    int32_t* rd_ptr = (int32_t*)data;
    int32_t* wr_ptr = (int32_t*)a2wr;

    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);
    _mm_stream_si32(wr_ptr++, *rd_ptr++);

  } else if (test == std::string("_mm256_store_si256")) {
    printf ("Running \"_mm256_store_si256\" test\n");
#ifdef __AVX2__

    __m256i* dst256 = (__m256i*) a2wr;
    __m256i* src256 = (__m256i*) data;
    _mm256_store_si256(dst256++, *src256++);
    _mm256_store_si256(dst256++, *src256++);
    _mm256_store_si256(dst256++, *src256++);
    _mm256_store_si256(dst256++, *src256++);
    _mm256_store_si256(dst256++, *src256++);
    _mm256_store_si256(dst256++, *src256++);
    _mm256_store_si256(dst256++, *src256++);
    _mm256_store_si256(dst256++, *src256++);
#else
    on_error("AVX2 not supported");
#endif
  } else if (test == std::string("loop")) {
    printf ("Running \"loop of _mm256_store_si256\" test\n");
#ifdef __AVX2__

    __m256i* dst256 = (__m256i*) a2wr;
    __m256i* src256 = (__m256i*) data;
    for (uint i = 0; i < 8; i++) {
      _mm256_store_si256(dst256++, *src256++);
    }
#else
    on_error("AVX2 not supported");
#endif
  } else  {
    on_error ("%s test options not supported",test.c_str());
  }


  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");
  return 0;
}



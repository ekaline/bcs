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

#include "smartnic.h"

#define on_error(...) { fprintf(stderr, "FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }

#define ADDR2WRITE (0xf0790 / 8)
#define MAX_ITERATIONS 1000000
uint64_t rtt[MAX_ITERATIONS] = {};
uint64_t rtv[MAX_ITERATIONS] = {};

int main(int argc, char *argv[]) {
  uint64_t iterations = (uint64_t)strtoull(argv[1],NULL,10);
  if (iterations > static_cast<uint64_t>(MAX_ITERATIONS)) on_error("%ju iterations is greater than %ju MAX_ITERATIONS",iterations,static_cast<uint64_t>(MAX_ITERATIONS));

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");

  for (uint64_t i = 0; i < iterations; i ++) {
    auto start = std::chrono::high_resolution_clock::now();
    if (SC_ERR_SUCCESS != SC_WriteUserLogicRegister(dev_id, ADDR2WRITE, 0x1122334455667788)) on_error("SC_Read returned smartnic error code : %d",SC_GetLastErrorCode());
    auto finish = std::chrono::high_resolution_clock::now();
    rtt[i] = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
  }
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS) on_error("Error on SC_CloseDevice");

  uint64_t rtt_max = static_cast<uint64_t>(0);
  uint64_t rtt_min = static_cast<uint64_t>(999999);

  for (uint64_t i = 0; i < iterations; i++) {
    printf ("%4ju,%09ju\n",i,rtt[i]);
    if (rtt[i] < rtt_min) rtt_min = rtt[i];
    if (rtt[i] > rtt_max) rtt_max = rtt[i];
  }

  fprintf (stderr,"after %ju iterations: min = %09ju ns, max = %09ju ns\n",iterations,rtt_min,rtt_max);
  if (argc == 5) { // dummy check, in reality always false
    for (uint64_t i = 0; i < iterations; i++) { // to prevent from the compiler to optimize the stuff out (in fact not needed)
      printf ("%08ju\n",rtv[i]); 
    }
  }
  return 0;

}

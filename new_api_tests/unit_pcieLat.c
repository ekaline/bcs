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


/* ############################################# */

static void eka_write_devid(SN_DeviceId dev_id, uint64_t addr, uint64_t val) {
    if (SN_ERR_SUCCESS != SN_WriteUserLogicRegister(dev_id, addr/8, val))
      on_error("SN_Write returned smartnic error code : %d",SN_GetLastErrorCode());
}

/* ############################################# */

uint64_t eka_read_devid(SN_DeviceId dev_id, uint64_t addr) {
  uint64_t ret;
  if (SN_ERR_SUCCESS != SN_ReadUserLogicRegister(dev_id, addr/8, &ret))
    on_error("SN_Read returned smartnic error code : %d",SN_GetLastErrorCode());
  return ret;
}
/* ############################################# */

/* *************************************************************** */
int main(int argc, char *argv[]) {

  SC_DeviceId dev_id = SC_OpenDevice(NULL, NULL);
  if (dev_id == NULL) on_error("SC_OpenDevice == NULL: cannot open Smartnic device");

  /* ------------------------------------------------------------------ */
  const size_t Iterations = 1024 * 1024; 
  const volatile uint64_t FpgaRtCntr = 0xf0e08;
  const volatile uint64_t ScrPadAddr = 0x20000 + 64 * 1024 - 16;

  auto rdData     = new uint64_t[Iterations];
  auto rttRdLatency = new uint64_t[Iterations];
  auto rttWrLatency = new uint64_t[Iterations];

  for (size_t i = 0; i < Iterations; i++) {
    auto one = std::chrono::high_resolution_clock::now();    
    rdData[i] = eka_read_devid(dev_id,FpgaRtCntr);
    auto two = std::chrono::high_resolution_clock::now();
    eka_write_devid(dev_id,ScrPadAddr,rdData[i]);
    auto three = std::chrono::high_resolution_clock::now();
    
    rttRdLatency[i] = (uint64_t)(std::chrono::duration_cast<std::chrono::nanoseconds>(two-one).count());
    rttWrLatency[i] = (uint64_t)(std::chrono::duration_cast<std::chrono::nanoseconds>(three-two).count());

    if ( i % 100000 == 0) printf("%ju\n",i);    
  }

  uint64_t minRdLat = -1;
  uint64_t minWrLat = -1;
  for (size_t i = 0; i < Iterations; i++) {
    if (rttRdLatency[i] < minRdLat)
      minRdLat = rttRdLatency[i];    
    if (rttWrLatency[i] < minWrLat)
      minWrLat = rttWrLatency[i];    
  }
  
  TEST_LOG("%ju Iterations: minRdLat=%ju ns, minWrLat=%ju ns",
	   Iterations,minRdLat,minWrLat);

  delete[] rdData;
  delete[] rttRdLatency;
  delete[] rttWrLatency;
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS)
    on_error("Error on SC_CloseDevice");
  return 0;
}



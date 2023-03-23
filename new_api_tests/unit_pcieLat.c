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
  const size_t Iterations = 10 * 1024 * 1024; 
  const uint64_t FpgaRtCntr = 0xf0e08;

  auto rdData     = new uint64_t[Iterations];
  auto rttLatency = new uint64_t[Iterations];

  for (size_t i = 0; i < Iterations; i++) {
    auto before = std::chrono::high_resolution_clock::now();    
    rdData[i] = eka_read_devid(dev_id,FpgaRtCntr);
    auto after = std::chrono::high_resolution_clock::now();
    
    rttLatency[i] = (uint64_t)(std::chrono::duration_cast<std::chrono::nanoseconds>(after-before).count());

    if ( i % 100000 == 0) printf("%ju\n",i);    
  }

  uint64_t minLat = -1;
  for (size_t i = 0; i < Iterations; i++) {
    if (i != 0) {
      if (rdData[i] <= rdData[i-1])
	on_error("rdData[i] %ju <= rdData[i-1] %ju",
		 rdData[i],rdData[i-1]);
    }

    if (rttLatency[i] < minLat)
      minLat = rttLatency[i];    
  }
  
  TEST_LOG("%ju Iterations: minLat=%ju ns",
	   Iterations,minLat);

  delete[] rdData;
  delete[] rttLatency;
  if (SC_CloseDevice(dev_id) != SC_ERR_SUCCESS)
    on_error("Error on SC_CloseDevice");
  return 0;
}



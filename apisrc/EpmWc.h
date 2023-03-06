#ifndef _EPM_WC_H_
#define _EPM_WC_H_

#include <x86intrin.h>
#include <emmintrin.h>  // Needed for _mm_clflush()
#include <atomic>

#include "EkaHwInternalStructs.h"

#include "eka_macros.h"

/* ----------------------------------------- */

static inline
void atomicCopy(volatile uint64_t * __restrict dst, 
		    const void *__restrict srcBuf,
		    const size_t len) {
  
  auto atomicDst = (std::atomic<uint64_t> *__restrict) dst;
  auto src = (const uint64_t *__restrict) srcBuf;
  for (size_t i = 0; i < len/8; i ++) {
    atomicDst->store( *src, std::memory_order_release );
    atomicDst++; src++;
  }
  __sync_synchronize();
  _mm_clflush(srcBuf);
  return;
}
/* ----------------------------------------- */

static inline
void epmCopyWcDesc(volatile uint64_t* descAddr, 
		   const size_t pktSize,
		   uint64_t hwHeapAddr,
		   uint16_t actionLocalIdx,
		   uint16_t epmRegion,
		   uint32_t tcpPseudoCsum,
		   uint64_t send
		   ) {
  struct WcDesc {
    epm_trig_desc_t trigDesc;
    uint64_t nBytes     : 12;
    uint64_t hwHeapAddr : 32;
    uint64_t opc        :  2;
    uint64_t pad18      : 18;
  } __attribute__((packed));
  
  WcDesc __attribute__ ((aligned(0x100)))
    desc = {
	    .trigDesc = {
			 .str = {
				 .tcp_cs       = tcpPseudoCsum,
				 .action_index = actionLocalIdx,
				 .size         = (uint16_t)(pktSize & 0xFFFF),
				 .region       = epmRegion
				 }
			 },
	    .nBytes     = roundUp64(pktSize) & 0xFFF,
	    .hwHeapAddr = hwHeapAddr,
	    .opc        = send
  };

  TEST_LOG("desc: action_index=%u, hwHeapAddr=%x, size=%u, nBytes=%u, opc=%u",
	   desc.trigDesc.str.action_index,
	   desc.hwHeapAddr,
	   desc.trigDesc.str.size,
	   desc.nBytes,
	   desc.opc);

  atomicCopy(descAddr,&desc,sizeof(desc));
  return;
}

/* ----------------------------------------- */

static inline
void epmCopyWcPayload(volatile uint64_t* dst, 
		      const void *srcBuf,
		      const size_t len) {
  atomicCopy(dst,srcBuf,len);
  return;

}

/* ----------------------------------------- */
static inline
void epmCopyWcBuf(volatile uint64_t* snDevWCPtr,
		  uint64_t hwHeapAddr,
		  const void* swHeapAddr,
		  const size_t pktSize,
		  uint thrId,
		  uint16_t actionLocalIdx,
		  uint16_t epmRegion,
		  uint32_t tcpPseudoCsum,
		  bool send
		  ) {

  
  auto wcRegionBase = (volatile uint64_t*)((uint64_t)snDevWCPtr + thrId * 0x800);
  epmCopyWcDesc(wcRegionBase,
		pktSize,
		hwHeapAddr,
		actionLocalIdx,
		epmRegion,
		tcpPseudoCsum,
		send ? 1 : 2
		);
  TEST_LOG("\nthrId=%u, snDevWCPtr=%p, wcRegionBase=%p, offs=0x%jx, thrId * 0x800 = 0x%jx, arith = 0x%jx)",
	   thrId, snDevWCPtr, wcRegionBase,
	   wcRegionBase - snDevWCPtr,
	   (uint64_t)(thrId * 0x800),
	   ((uint64_t)snDevWCPtr + thrId * 0x800)
	   );
  
  TEST_LOG("thrId=%u, snDevWCPtr=%p, thrIdDesc addr = %p, data addr = %p, hwHeapAddr=0x%jx, offs=0x%jx)",
	   thrId, snDevWCPtr, wcRegionBase,wcRegionBase + 8,hwHeapAddr,
	   (uint64_t)wcRegionBase - (uint64_t)snDevWCPtr);

  hexDump("WC Copied Buf",swHeapAddr,pktSize);
  epmCopyWcPayload(wcRegionBase + 8,
		   swHeapAddr,
		   roundUp64(pktSize)
		   );

  return;
}


#endif

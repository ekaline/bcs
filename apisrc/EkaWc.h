#ifndef _EKA_WC_H_
#define _EKA_WC_H_

#include <x86intrin.h>
#include <emmintrin.h>  // Needed for _mm_clflush()
#include <atomic>

#include "EkaHwInternalStructs.h"

#include "eka_macros.h"

/* ----------------------------------------- */
class EkaWc {
public:
  enum class SendOp : int { Send = 1, DontSend = 2 };

  EkaWc(volatile uint64_t *snDevWCPtr) {
    snDevWCPtr_ = snDevWCPtr;
    if (!snDevWCPtr_)
      on_error("!snDevWCPtr_");

    txBuf_ = 0;
    busy_.clear();
  }

  inline void epmCopyWcBuf(uint64_t hwHeapAddr,
			   const void* swHeapAddr,
			   const size_t pktSize,
			   uint16_t actionLocalIdx,
			   uint16_t epmRegion
			   ) {
    const int wcChId = 2;
    auto wcRegionBase = (volatile uint64_t*)((uint64_t)snDevWCPtr_ + wcChId * 0x800);


    if ((uint64_t)wcRegionBase - (uint64_t)snDevWCPtr_ != wcChId * 0x800)
      on_error("wcRegionBase %p - snDevWCPtr_ %p != wcChId * 0x800 0x%x",
	       wcRegionBase,snDevWCPtr_,wcChId * 0x800);
    // TEST_LOG("hwHeapAddr=0x%jx, actionLocalIdx=%u, pktSize=%jd, wcRegionOffs=0x%x",
    // 	     hwHeapAddr,actionLocalIdx,pktSize,wcChId * 0x800);
    // hexDump("epmCopyWcBuf",swHeapAddr,roundUp64(pktSize));
    
    epmCopyWcDesc(wcRegionBase,
		  pktSize,
		  hwHeapAddr,
		  actionLocalIdx,
		  epmRegion,
		  0, //tcpPseudoCsum,
		  SendOp::DontSend
		  );
    epmCopyWcPayload(wcRegionBase + 8,
		     swHeapAddr,
		     roundUp64(pktSize)
		     );
    return;
  }
  
  inline void epmCopyAndSendWcBuf(const void* swHeapAddr,
				  const size_t pktSize,
				  uint16_t actionLocalIdx,
				  uint16_t epmRegion,
				  uint32_t tcpPseudoCsum
				  ) {
    const uint64_t TxBufAddr_0 = 8 * 1024 * 1024 - 2048;
    const uint64_t TxBufAddr_1 = 8 * 1024 * 1024 - 4096;
    const int wcChId = 0;
    
    while(busy_.test_and_set(std::memory_order_acquire)) {
	busy_.test(std::memory_order_relaxed);
    }


    auto hwHeapAddr = txBuf_ ? TxBufAddr_1 : TxBufAddr_0;
    auto wcRegionBase = (volatile uint64_t*)((uint64_t)snDevWCPtr_ + wcChId * 0x800);

    epmCopyWcDesc(wcRegionBase,
		  pktSize,
		  hwHeapAddr,
		  actionLocalIdx,
		  epmRegion,
		  tcpPseudoCsum,
		  SendOp::Send
		  );
    epmCopyWcPayload(wcRegionBase + 8,
		     swHeapAddr,
		     roundUp64(pktSize)
		     );


    txBuf_ = ! txBuf_;
    
    busy_.clear(std::memory_order_release);

    return;
  }
  
private:
  inline void atomicCopy(volatile uint64_t * __restrict dst, 
			 const void *__restrict srcBuf,
			 const size_t len) {
    
    auto atomicDst = (std::atomic<uint64_t> *__restrict) dst;
    auto src = (const uint64_t *__restrict) srcBuf;
    for (size_t i = 0; i < len/8; i ++) {
      atomicDst->store( *src, std::memory_order_release );
      atomicDst++; src++;
    }
    /* __sync_synchronize(); */
    _mm_clflush(srcBuf);
    return;
  }
  /* ----------------------------------------- */

  volatile uint64_t *snDevWCPtr_;

  volatile bool txBuf_ = 0;
  std::atomic_flag  busy_ = ATOMIC_FLAG_INIT;

  /* ----------------------------------------- */

  inline void epmCopyWcDesc(volatile uint64_t* descAddr, 
			    const size_t pktSize,
			    uint64_t hwHeapAddr,
			    uint16_t actionLocalIdx,
			    uint16_t epmRegion,
			    uint32_t tcpPseudoCsum,
			    SendOp send
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
	      .opc        = (uint64_t)send
    };
#if 0
    TEST_LOG("desc: action_index=%u, hwHeapAddr=%x, size=%u, nBytes=%u, opc=%u",
	     desc.trigDesc.str.action_index,
	     desc.hwHeapAddr,
	     desc.trigDesc.str.size,
	     desc.nBytes,
	     desc.opc);
#endif
    atomicCopy((volatile uint64_t *)descAddr,&desc,sizeof(desc));
    return;
  }

  /* ----------------------------------------- */

  inline void epmCopyWcPayload(volatile uint64_t* dst, 
			       const void *srcBuf,
			       const size_t len) {
    atomicCopy((volatile uint64_t*)dst,srcBuf,len);
    return;
  }
  /* ----------------------------------------- */

};

#endif

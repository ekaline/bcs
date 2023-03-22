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

  enum class AccessType : int {
    Invalid = 0,
      EtherFrame,
      HeapInit,
      HeapPreload,
      TcpSend,
      SwFire
  };
  
  EkaWc(volatile uint64_t *snDevWCPtr) {
    snDevWCPtr_ = snDevWCPtr;
    if (!snDevWCPtr_)
      on_error("!snDevWCPtr_");
    for (auto i = 0; i < MaxChannels_; i++)
      ch_[i].setId(i);
  }

  inline void epmCopyWcBuf(uint64_t hwHeapAddr,
			   const void* swHeapAddr,
			   const size_t pktSize,
			   AccessType type,
			   uint16_t actionLocalIdx,
			   uint16_t epmRegion,
			   uint32_t tcpPseudoCsum,
			   SendOp send
			   ) {
    auto wcChId = getChannelId(type);

    ch_[wcChId].acquire();
    auto wcRegionBase = (volatile uint64_t*)((uint64_t)snDevWCPtr_ + wcChId * 0x800);
    epmCopyWcDesc(wcRegionBase,
		  pktSize,
		  hwHeapAddr,
		  actionLocalIdx,
		  epmRegion,
		  tcpPseudoCsum,
		  send
		  );
    epmCopyWcPayload(wcRegionBase + 8,
		     swHeapAddr,
		     roundUp64(pktSize)
		     );
    ch_[wcChId].release();

    return;
  }
  
private:
  static const int MaxChannels_ = 16;
  static const uint32_t WndSize_ = 4096;

  static const int EtherFrameChId_  = MaxChannels_ - 1;    // 15
  static const int HeapInitChId_    = EtherFrameChId_ - 1; // 14
  static const int HeapPreloadChId_ = HeapInitChId_ - 1;   // 13
  static const int TcpSendChId_ = 0;                       // 0
  static const int SwFireChId_ = 1;                        // 1

  /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  class WrChannel_ {
  public:
    void setId(int i) {
      id_= i;
    }
    void acquire() {
      while(isFree_.test_and_set(std::memory_order_acquire)) {
	isFree_.test(std::memory_order_relaxed);
      }
    }
    void release() {
      isFree_.clear();
    }
    
  private:
    int id_ = -1;
    std::atomic<uint32_t> baseAddr_ = -1;
    std::atomic_flag      isFree_ = ATOMIC_FLAG_INIT;
  };
  /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

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
  WrChannel_ ch_[MaxChannels_] = {};
  volatile uint64_t *snDevWCPtr_;
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
    atomicCopy(descAddr,&desc,sizeof(desc));
    return;
  }

  /* ----------------------------------------- */

  inline void epmCopyWcPayload(volatile uint64_t* dst, 
			       const void *srcBuf,
			       const size_t len) {
    atomicCopy(dst,srcBuf,len);
    return;
  }
  /* ----------------------------------------- */
  int getChannelId(AccessType type) {
    switch (type) {
    case AccessType::EtherFrame :
      return EtherFrameChId_;
    case AccessType::HeapPreload :
      return HeapPreloadChId_;
    case AccessType::HeapInit :
      return HeapInitChId_;
    case AccessType::TcpSend :
      return TcpSendChId_;
    case AccessType::SwFire :
      return SwFireChId_;
    default :
      on_error("Unexpected AccessType %d",(int)type);
    }
  }
  
};

#endif

#ifndef _EKA_HEAP_WR_CHANNELS_H_
#define _EKA_HEAP_WR_CHANNELS_H_

#include "eka_macros.h"

class EkaHeapWrChannels {
 public:

  enum class AccessType : int {
    Invalid = 0,
      EtherFrame,
      HeapInit,
      HeapPreload,
      TcpSend,
      SwFire
  };

  
  EkaHeapWrChannels() {

  }

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

  void getChannel(int chId) {
    ch_[chId].get();

  }
  
  void releaseChannel(int chId) {
    ch_[chId].release();
  }
  
 private:
  static const int MaxChannels_ = 16;
  static const uint32_t WndSize_ = 4096;

  static const int EtherFrameChId_  = MaxChannels_ - 1;    // 15
  static const int HeapInitChId_    = EtherFrameChId_ - 1; // 14
  static const int HeapPreloadChId_ = HeapInitChId_ - 1;   // 13
  static const int TcpSendChId_ = 0;                       // 0
  static const int SwFireChId_ = 1;                        // 1

    
  class WrChannel_ {
  public:
    void get(uint32_t baseAddr, uint32_t len) {
      while(isFree_.test_and_set(std::memory_order_acquire)) {
      }
      if (baseAddr < baseAddr_ ||
	  baseAddr + len > baseAddr_ + WndSize_)
	baseAddr_.store(baseAddr);
    }

    void get() {
      while(isFree_.test_and_set(std::memory_order_acquire)) {
	// TEST_LOG("Spinlock on get");
	isFree_.test(std::memory_order_relaxed);
      }
      // TEST_LOG("Acquired!");

    }

    void release() {
      isFree_.clear();
      // TEST_LOG("Released!");
    }
    
  private:
    std::atomic<uint32_t> baseAddr_ = -1;
    std::atomic_flag      isFree_ = ATOMIC_FLAG_INIT;
  };

  WrChannel_ ch_[MaxChannels_];

};
#endif

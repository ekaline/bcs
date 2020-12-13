#ifndef _EKA_FH_XDP_GR_H_
#define _EKA_FH_XDP_GR_H_

#include "EkaFhGroup.h"

class EkaFhXdpGr : public EkaFhGroup{
 public:
  FhXdpGr();
  virtual               ~FhXdpGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);
  int                   bookInit(EfhCtx* pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);

  inline uint     findAndInstallStream(uint streamId, uint32_t curSeq) {
    for (uint i = 0; i < numStreams; i ++) if (stream[i]->getId() == streamId) return i;
    if (numStreams == MAX_STREAMS) on_error("numStreams == MAX_STREAMS (%u), cant add stream %u",numStreams,streamId);
    stream[numStreams] = new Stream(streamId,curSeq);
    return numStreams++;
  }
  inline uint32_t getExpectedSeq(uint streamIdx) {
    return stream[streamIdx]->getExpectedSeq();
  }
  inline uint32_t setExpectedSeq(uint streamIdx,uint32_t seq) {
    return stream[streamIdx]->setExpectedSeq(seq);
  }
  inline void     setGapStart() {
    gapStart = std::chrono::high_resolution_clock::now();
    return;
  }
  inline bool     isGapOver() {
    auto finish = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(finish-gapStart).count() > 120) return true;
    return false;
  }
  inline uint     getId(uint streamIdx) {
    return stream[streamIdx]->getId();
  }
  inline uint32_t resetExpectedSeq(uint streamIdx) {
    return stream[streamIdx]->resetExpectedSeq();
  }
  bool     inGap;

  class Stream {
  public:
    Stream(uint strId,uint32_t seq) {
      id = strId;
      expectedSeq = seq;
    }
    inline uint     getId() {
      return id;
    }
    inline uint32_t getExpectedSeq() {
      return expectedSeq;
    }
    inline uint32_t setExpectedSeq(uint32_t seq) {
      return (expectedSeq = seq);
    }
    inline uint32_t resetExpectedSeq() {
      return (expectedSeq = 2);
    }

  private:
    uint     id;
    uint32_t expectedSeq;
  };

 private:
  Stream*  stream[MAX_STREAMS];
  uint     numStreams;

  std::chrono::high_resolution_clock::time_point gapStart;

  uint32_t underlyingIdx[MAX_UNDERLYINGS];
  uint     numUnderlyings;
  char     symbolStatus[MAX_UNDERLYINGS];
  char     seriesStatus[MAX_SERIES];
};
#endif

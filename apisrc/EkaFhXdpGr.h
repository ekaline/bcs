#ifndef _EKA_FH_XDP_GR_H_
#define _EKA_FH_XDP_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhTobBook.h"

class EkaFhXdpGr : public EkaFhGroup{
 public:
  virtual               ~EkaFhXdpGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);


  bool                  processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				      uint             pktSize, 
				      uint             streamIdx, 
				      const uint8_t*   pktPtr, 
				      uint             msgInPkt, 
				      uint64_t         seq);

  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);

  int                  subscribeStaticSecurity(uint64_t        securityId, 
					       EfhSecurityType efhSecurityType,
					       EfhSecUserData  efhSecUserData,
					       uint64_t        opaqueAttrA,
					       uint64_t        opaqueAttrB) {
    if (book == NULL) on_error("%s:%u book == NULL",EKA_EXCH_DECODE(exch),id);
    book->subscribeSecurity(securityId, 
			    efhSecurityType,
			    efhSecUserData,
			    opaqueAttrA,
			    opaqueAttrB);
    return 0;
  }


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

/* -------------------------------------- */
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
/* -------------------------------------- */
 public:
  bool     inGap = false;

 private:
  static const uint MAX_STREAMS = 16;
  static const uint MAX_UNDERLYINGS = 512;

  Stream*  stream[MAX_STREAMS] = {};
  uint     numStreams = 0;

  std::chrono::high_resolution_clock::time_point gapStart;

  uint32_t underlyingIdx[MAX_UNDERLYINGS] = {};
  uint     numUnderlyings                 = 0;
  char     symbolStatus[MAX_UNDERLYINGS]  = {};
  //  char     seriesStatus[MAX_SERIES]       = {};

  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurity  = EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>;
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,FhSecurity,SecurityIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

};
#endif

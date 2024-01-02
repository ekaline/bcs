#ifndef _EKA_PARSER_H_
#define _EKA_PARSER_H_

#include "EkaExch.h"

static inline std::string ns2str(uint64_t ts) {
  char dst[64] = {};
  uint ns = ts % 1000;
  uint64_t res = (ts - ns) / 1000;
  uint us = res % 1000;
  res = (res - us) / 1000;
  uint ms = res % 1000;
  res = (res - ms) / 1000;
  uint s = res % 60;
  res = (res - s) / 60;
  uint m = res % 60;
  res = (res - m) / 60;
  uint h = res % 24;
  sprintf (dst,"%02d:%02d:%02d.%03d.%03d.%03d",h,m,s,ms,us,ns);
  return std::string(dst);
}

class EkaParser {
 public:
  EkaParser(EkaExch* _exch,EkaStrat* _strat) {
    exch  = _exch;
    strat = _strat;
  }

  virtual uint processPkt(MdOut* mdOut, 
			  ProcessAction processAction, 
			  uint8_t* pkt, 
			  uint pktPayloadSize) = 0;

  virtual uint printPkt(uint8_t* pkt, 
			  uint pktPayloadSize){
    return 0;
  }

  virtual uint printMsg(uint8_t* msgStart){
    return 0;
  }



  virtual uint createAddOrder(uint8_t* dst,
			      uint64_t secId,
			      SIDE     s,
			      uint     priceLevel,
			      uint64_t basePrice, 
			      uint64_t priceMultiplier, 
			      uint64_t priceStep, 
			      uint32_t size, 
			      uint64_t sequence,
			      uint64_t ts) { return 0;}
 
  virtual uint createTrade(uint8_t* dst,
			   uint64_t secId,
			   SIDE     s,
			   uint64_t basePrice, 
			   uint64_t priceMultiplier, 
			   uint32_t size, 
			   uint64_t sequence,
			   uint64_t ts) { return 0;}
   
  EkaExch*  exch;
  EkaStrat* strat;
};

#endif

#ifndef _EKA_EOBI_BOOK_SIDE_H_
#define _EKA_EOBI_BOOK_SIDE_H_

#include "eka_hw_conf.h"
#include "EkaEobiBook.h"

class EkaEobiBookSide {
 public:
  static const uint ENTRIES = EkaEobiBook::ENTRIES;

  using ProductId      = EkaEobi::ProductId;
  using ExchSecurityId = EkaEobi::ExchSecurityId;
  using Price          = EkaEobi::Price;
  using Size           = EkaEobi::Size;
  using NormPrice      = EkaEobi::NormPrice;

  /* ----------------------------------------------------- */

  EkaEobiBookSide(SIDE _side, EkaEobiBook*_parent) {
    parent = _parent;
    side   = _side;

    dev    = parent->dev;
    step   = parent->step;
    bottom = parent->bottom;
    top    = parent->top;

    tobIdx = side == SIDE::BID ? 0 : ENTRIES - 1;

    EKA_LOG("Creating %s side: bottom=%jd, top=%jd, step=%jd, tobIdx=%u",
	    side == BID ? "BID" : "ASK",
	    bottom,top,step,tobIdx
	    );

    for (uint i = 0; i < ENTRIES; i++) {
      size[i] = 0;
    }
  }
  /* ----------------------------------------------------- */
 
  int invalidate() {
    //    EKA_LOG("Invalidating %s",side == SIDE::BID ? "BID" : "ASK");
    tobIdx = side == SIDE::BID ? 0 : ENTRIES - 1;

    for (uint i = 0; i < ENTRIES; i++) {
      size[i] = 0;
    }
    memset(tobStateEntry,0,sizeof(tobStateEntry));
    return 0;
  }

  int addSize(Price price, Size _size) {
    uint idx = price2idx(price);
    size[idx] += _size;

    tobIdx = newTobIdx(idx);
    //    EKA_LOG("%c: idx = %u, tobIdx = %u",side == BID ? 'B' : 'A', idx,tobIdx);
    return 0;
  }
  /* ----------------------------------------------------- */

  int substrSize(Price price, Size _size) {
    uint idx = price2idx(price);
    if (size[idx] < _size) {
      size[idx] = 0;
      return 0;
    } 

    size[idx] -= _size;

    tobIdx = newTobIdx(idx);
    //    EKA_LOG("%c: idx = %u, tobIdx = %u",side == BID ? 'B' : 'A', idx,tobIdx);
    return 0;
  }
  /* ----------------------------------------------------- */
  uint64_t getEntryPrice(uint bookEntry) {
    if (tobStateEntry[bookEntry].size==0) return (uint64_t)0;
    return (uint64_t) idx2price(tobStateEntry[bookEntry].idx);
  }
  /* ----------------------------------------------------- */

  inline Price getValidEntryPrice(uint bookEntry) {
    uint idx = bookEntry2idx(bookEntry);
    return  idx * step + bottom;
  }
  /* ----------------------------------------------------- */

  uint64_t getEntrySize(uint bookEntry) {
    return (uint) tobStateEntry[bookEntry].size;
  }

  /* ----------------------------------------------------- */

  inline Size getValidEntrySize(uint bookEntry) {
    uint idx = bookEntry2idx(bookEntry);
    return size[idx];
  }
  /* ----------------------------------------------------- */
  void printSide() {
    if (side == SIDE::ASK) {
      printf("ASK: tobIdx = %u\n", tobIdx);
      for (int idx = ENTRIES - 1; idx >= 0; idx --) {
	if (size[idx] == 0) continue;
	printf("%4u: %4u @ %8ju \n",idx, size[idx], idx2price(idx));
      }
    } else {
      printf("\t\t\t\tBID: tobIdx = %u\n", tobIdx);
      for (int idx = ENTRIES - 1; idx >= 0; idx --) {
	if (size[idx] == 0) continue;
	printf("\t\t\t\t%4u: %4u @ %8ju \n",idx, size[idx], idx2price(idx));
      }
    }
  }
  /* ----------------------------------------------------- */
  void printTob() {
    printf("%c: %4u: %4u @ %8ju",
	   side == SIDE::ASK ? 'A' : 'B',
	   tobIdx, size[tobIdx], idx2price(tobIdx));
  }

  /* ----------------------------------------------------- */
  bool testTobChange(uint64_t ts) {
    TobStateEntry newTobStateEntry[HW_DEPTH_PRICES] = {};
    for (uint i = 0; i < HW_DEPTH_PRICES; i++) {
      uint idx = bookEntry2idx(i);
      newTobStateEntry[i].idx  = idx;
      newTobStateEntry[i].size = size[idx];
    }
    
    lastTobTs = 0;

#ifdef _EKA_PARSER_PRINT_ALL_ 
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("%s: OLD TobState   -- NEW TobState\n",
	   side == BID ? "BID" : "ASK");
    for (uint i = 0; i < HW_DEPTH_PRICES; i++) {
      printf("%8u : %8u -- %8u : %8u\n",
	     tobStateEntry[i].idx,   tobStateEntry[i].size,
	     newTobStateEntry[i].idx,newTobStateEntry[i].size
	     );
    }
#endif

    if (memcmp(&newTobStateEntry[0],&tobStateEntry[0],sizeof(TobStateEntry) * HW_DEPTH_PRICES) == 0)
      return false;


    if ((side == BID && newTobStateEntry[0].idx >= tobStateEntry[0].idx) ||
	(side == ASK && newTobStateEntry[0].idx <= tobStateEntry[0].idx))
      lastTobTs = ts;

    memcpy(&tobStateEntry[0],&newTobStateEntry[0],sizeof(TobStateEntry) * HW_DEPTH_PRICES);
    return true;
  }

 private:
  /* ----------------------------------------------------- */

  inline Price idx2price(uint idx) {
    if (idx  >= ENTRIES) on_error("Invalid idx %u",idx);
    return bottom + idx * step;
  }

  /* ----------------------------------------------------- */

  inline uint price2idx(Price _price) {
    if (_price < bottom || _price >= bottom + ENTRIES * step)
      on_error("%s: Invalid price %jd, bottom = %jd, top = %jd",
	       EKA_RESOLVE_PROD(dev->hwFeedVer,parent->prod->product_id),_price,bottom,top);
    uint idx = (_price - bottom) / step;
    if (idx >= ENTRIES) 
      on_error("%s: Invalid price %jd, bottom = %jd, top = %jd",
	       EKA_RESOLVE_PROD(dev->hwFeedVer,parent->prod->product_id),_price,bottom,top);
    return idx;
  }
  /* ----------------------------------------------------- */
  inline uint newTobIdx(uint idx) {
    //    EKA_LOG("side = %s, idx = %u, tobIdx = %d",side == BID ? "BID" : "ASK",idx,tobIdx);

    if (side == BID) {
      if (idx < tobIdx) return tobIdx;
      for (int i = idx; i >= 0; i--) {
	if (size[i] != 0) return (uint)i;
      }
      return 0;
    } else if (side == ASK) { 
      if (idx > tobIdx) return tobIdx;
      for (int i = idx; i < (int)ENTRIES; i++) {
	if (size[i] != 0) return (uint)i;
      }
      return ENTRIES - 1;
    } else {
      on_error("unexpected side = %d",side);
    }
  }
  /* ----------------------------------------------------- */
  inline uint bookEntry2idx(uint bookEntry) {
    uint checkedEntries = 0;
    //    EKA_LOG("side = %s, bookEntry = %d, tobIdx = %d",side == BID ? "BID" : "ASK",bookEntry,tobIdx);
    if (side == BID) {
      for (int idx = tobIdx; idx > 0; idx --) {
	if (size[idx] != 0) {
	  if (checkedEntries == bookEntry) return idx;
	  checkedEntries++;	  
	}
      }
      return 0;
    } else if (side == ASK) { // 
      for (int idx = tobIdx; idx < (int)(ENTRIES - 1); idx ++) {
	if (size[idx] != 0) {
	  if (checkedEntries == bookEntry) return idx;
	  checkedEntries++;	  
	}
      }
      return ENTRIES - 1;
    } else {
      on_error("unexpected side = %d",side);
    }
  }
  /* ----------------------------------------------------- */

 public:
  SIDE          side;
  EkaEobiBook*  parent;

  uint          tobIdx;   // index of the best price entry

  Price         bottom; // lowest price
  Price         top;    // highest price
  Price         step;

  Size          size[ENTRIES];

  uint64_t      lastTobTs;

 private:
  class TobStateEntry {
  public:
    uint idx;
    Size size;
  };

  TobStateEntry tobStateEntry[HW_DEPTH_PRICES];

  EkaDev* dev;
};

#endif

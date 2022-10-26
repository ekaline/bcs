#ifndef _EKA_FH_CME_BOOK_SIDE_H_
#define _EKA_FH_CME_BOOK_SIDE_H_

#include "EkaFhCmeBookEntry.h"
#include "EkaFhTypes.h"

template <
  class PriceLevetT,
  class PriceT, 
  class SizeT>
  class EkaFhCmeBookSide {
 public:
  static const PriceLevetT ENTRIES = 10; 
  
  EkaFhCmeBookSide(SideT _side, PriceLevetT _ActiveEntries) {
    side       = _side;
    //    tob        = 0;
    ActiveEntries = _ActiveEntries;
    if (ActiveEntries <= 0 || ActiveEntries > ENTRIES)
      on_error("ActiveEntries %jd is out of range",ActiveEntries);
    //    TEST_LOG("Creating %s side",side == SideT::BID ? "BID" : "ASK");
    for (PriceLevetT i = 0; i < ENTRIES; i++) {
      entry[i].valid   = 0;
      entry[i].price   = 0;
      entry[i].size    = 0;
      entry[i].accSize = 0;
    }
  }

  inline void resetSide() {
    for (PriceLevetT i = 0; i < ENTRIES; i++) {
      entry[i].valid   = 0;
      entry[i].price   = 0;
      entry[i].size    = 0;
      entry[i].accSize = 0;
    }
  }
  
  inline bool isValid(PriceLevetT idx) const{
    return entry[idx].valid;
  }

  inline SideT getSide() const{
    return side;
  }
  
  inline PriceT getEntryPrice (PriceLevetT idx) const{
    if (! entry[idx].valid) return 0;
    return entry[idx].price;
  }

  inline SizeT getEntrySize (PriceLevetT idx) const{
    if (! entry[idx].valid) return 0;
    return entry[idx].size;
  }

  inline bool isPriceWorseThan (PriceT priceA, PriceT priceB) const{
    if (side == SideT::BID) return (priceA <= priceB);
    return (priceA >= priceB);
  }

  /* ----------------------------------------------------- */

  inline bool checkIntegrity()  const { 
    bool res = true;
    for (PriceLevetT idx = 0; idx < ENTRIES - 1; idx ++) {
#ifndef FH_LAB      
      // This check doesnt work without SNAPSHOT!!!
      if (! isValid(idx) && isValid(idx + 1)) {
        TEST_LOG("%s entry %jd not valid, but entry %jd is valid",
      		 side == SideT::BID ? "BID" : "ASK",
      		 idx, idx + 1);
        res = false;
      }
#endif
      
      if (isValid(idx) && idx >= ActiveEntries) {
	TEST_LOG("idx %jd >= ActiveEntries %jd",idx,ActiveEntries);
	res = false;
      }
      if (isValid(idx) && isValid(idx + 1) &&
	  isPriceWorseThan(getEntryPrice(idx),getEntryPrice(idx + 1))) {
	// TEST_LOG("%s: price %8ju at level %2jd "
	// 	 "is worse than price %8ju at level %2jd",
	// 	 side == SideT::BID ? "BID" : "ASK",
	// 	 getEntryPrice(idx), idx,
	// 	 getEntryPrice(idx + 1), idx + 1);
	res = false;
      }
    }
    return res;
  }

  void printSide() const {
    if (side == SideT::ASK) {
      printf("ASK:\n");
      for (PriceLevetT idx = ENTRIES - 1; idx >= 0; idx --) {
	if (! entry[idx].valid) continue;
	printf("%4u@%4jd (%ju)\n",entry[idx].size,idx,entry[idx].price);
      }
    } else {
      printf("\t\t\t\tBID:\n");
      for (PriceLevetT idx = 0; idx < ENTRIES; idx ++) {
	if (! entry[idx].valid) continue;
	printf("\t\t\t\t%4u@%4jd (%ju)\n", entry[idx].size,idx,entry[idx].price);
      }
    }
  }
  /* ----------------------------------------------------- */

  inline PriceLevetT priceLevel2idx (uint8_t priceLevel) const{
    if (priceLevel == 0 || priceLevel > ActiveEntries )
      on_error("Invalid priceLevel %u (ActiveEntries = %jd)",
	       priceLevel,ActiveEntries);
    return (priceLevel - 1);
  }

  /* ----------------------------------------------------- */

  inline bool newPlvl(uint8_t pLevelIdx,
		      PriceT  price,
		      SizeT   size) {
    PriceLevetT idx = priceLevel2idx(pLevelIdx);
    for (PriceLevetT i = ActiveEntries - 1; i > idx; i--)
      memcpy(&entry[i],&entry[i-1],sizeof(entry[0]));
    entry[idx].size  = size;
    entry[idx].price = price;
    entry[idx].valid = true;

    return idx == 0;
  }

  /* ----------------------------------------------------- */

  inline bool changePlvl(uint8_t pLevelIdx,
			 PriceT  price,
			 SizeT   size) {
    PriceLevetT idx = priceLevel2idx(pLevelIdx);

    entry[idx].size  = size;
    entry[idx].price = price;
    entry[idx].valid = true;

    return idx == 0;
  }

  /* ----------------------------------------------------- */

  inline bool deletePlvl(uint8_t pLevelIdx) {
    PriceLevetT idx = priceLevel2idx(pLevelIdx);
    
    for (auto i = idx; i < ActiveEntries - 1; i ++)
      memcpy(&entry[i],&entry[i+1],sizeof(entry[0]));
    entry[ActiveEntries - 1].valid = false;
    entry[ActiveEntries - 1].size  = 0; 
    entry[ActiveEntries - 1].price = 0;

    return idx == 0;// && entry[0].valid;
  }

  /* ----------------------------------------------------- */

 private:
  SideT       side = SideT::UNINIT;
  EkaFhCmeBookEntry <PriceT,SizeT> entry[ENTRIES] = {};
  PriceLevetT ActiveEntries = 0; 

  //  PriceLevetT      tob = 0; // pointer to best price entry

  };

#endif


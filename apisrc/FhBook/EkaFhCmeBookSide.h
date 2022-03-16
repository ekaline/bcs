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
    tob        = 0;
    ActiveEntries = _ActiveEntries;
    if (ActiveEntries <= 0 || ActiveEntries > ENTRIES)
      on_error("ActiveEntries %d is out of range",ActiveEntries);
    //    TEST_LOG("Creating %s side",side == SideT::BID ? "BID" : "ASK");
    for (auto i = 0; i < ENTRIES; i++) {
      entry[i].valid   = 0;
      entry[i].price   = 0;
      entry[i].size    = 0;
      entry[i].accSize = 0;
    }
  }

  inline bool isValid(PriceLevetT idx) {
    return entry[idx].valid;
  }

  inline PriceT getEntryPrice(PriceLevetT idx) {
    if (! entry[idx].valid) return 0;
    return entry[idx].price;
  }

  inline SizeT getEntrySize(PriceLevetT idx) {
    if (! entry[idx].valid) return 0;
    return entry[idx].size;
  }

  inline bool isPriceWorseThan(PriceT priceA, PriceT priceB) {
    if (side == SideT::BID) return (priceA <= priceB);
    return (priceA >= priceB);
  }

  /* ----------------------------------------------------- */

  inline bool checkIntegrity() { 
    bool res = true;
    for (auto idx = 0; idx < ActiveEntries - 1; idx ++) {
      /* if (! isValid(idx) && isValid(idx + 1)) { */
      /*   EKA_WARN("%s entry %u not valid, but entry %u is valid", */
      /* 	   side == SideT::BID ? "BID" : "ASK", idx, idx + 1); */
      /*   res = false; */
      /* } */
      if (isValid(idx) && isValid(idx + 1) &&
	  isPriceWorseThan(getEntryPrice(idx),getEntryPrice(idx + 1))) {
	//	  EKA_WARN("%s: price %8ju at level %2u is worse than price %8ju at level %2u",
	on_error("%s: price %8ju at level %2u is worse than price %8ju at level %2u",
		 side == SideT::BID ? "BID" : "ASK",
		 getEntryPrice(idx), idx,
		 getEntryPrice(idx + 1), idx + 1);
	res = false;
      }
    }
    return res;
  }

  void printSide() {
    if (side == SideT::ASK) {
      printf("ASK:\n");
      for (int idx = ActiveEntries - 1; idx >= 0; idx --) {
	if (! entry[idx].valid) continue;
	printf("%4u@%4u (%ju)\n",entry[idx].size,idx,entry[idx].price);
      }
    } else {
      printf("\t\t\t\tBID:\n");
      for (auto idx = 0; idx < ActiveEntries; idx ++) {
	if (! entry[idx].valid) continue;
	printf("\t\t\t\t%4u@%4u (%ju)\n", entry[idx].size,idx,entry[idx].price);
      }
    }
  }
  /* ----------------------------------------------------- */

  inline PriceLevetT priceLevel2idx(PriceLevetT priceLevel) {
    if (priceLevel == 0 || priceLevel > ActiveEntries )
      on_error("Invalid priceLevel %u (ActiveEntries = %u)",
	       priceLevel,ActiveEntries);
    return (priceLevel - 1);
  }

  /* ----------------------------------------------------- */

  inline bool newPlevel(PriceLevetT   pLevelIdx,
		       PriceT price,
		       SizeT  size) {
    PriceLevetT idx = priceLevel2idx(pLevelIdx);
    for (auto i = ActiveEntries - 1; i > idx; i--)
      memcpy(&entry[i],&entry[i-1],sizeof(entry[0]));
    entry[idx].size  = size;
    entry[idx].price = price;
    entry[idx].valid = true;

    return idx == 0;
  }

  /* ----------------------------------------------------- */

  inline bool changePlevel(PriceLevetT   pLevelIdx,
			  PriceT price,
			  SizeT  size) {
    PriceLevetT idx = priceLevel2idx(pLevelIdx);

    entry[idx].size  = size;
    entry[idx].price = price;
    entry[idx].valid = true;

    return idx == 0;
  }

  /* ----------------------------------------------------- */

  inline bool deletePlevel(PriceLevetT pLevelIdx) {
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

  PriceLevetT      tob = 0; // pointer to best price entry

  };

#endif


#ifndef _EKA_CME_BOOK_H
#define _EKA_CME_BOOK_H

#include <string.h>

#include "eka_macros.h"
#include "eka_hw_conf.h"

#include "EkaCme.h"
#include "EkaBook.h"

#include "EkaBookTypes.h"

//##############################################################################

class EkaCmeBook : public EkaBook {
 public:
  static const uint ENTRIES4SANITY = 10*1024;

  using ProductId      = EkaCme::ProductId;
  using ExchSecurityId = EkaCme::ExchSecurityId;
  using Price          = EkaCme::Price;
  using Size           = EkaCme::Size;
  using NormPrice      = EkaCme::NormPrice;

  using PriceLevelIdx  = EkaCme::PriceLevelIdx;


  //##############################################################################

  class EkaCmeBookSide {
  public:
    static const uint ENTRIES = 10; //10*1024;
    static const uint ENTRIES4SANITY = 10*1024;

    EkaCmeBookSide(SIDE _side, EkaCmeBook*_parent) {
      /* dev    = _dev; */
      /* side   = _side; */
      /* step   = _step; */
      /* bottom = _bottom; */
      /* top    = _top; */
      side = _side;
      parent = _parent;
      pendingTOB = false;
      tob = 0;
      EKA_LOG("Creating %s side",side == BID ? "BID" : "ASK");
      for (uint i = 0; i < ENTRIES; i++) {
	entry[i].valid   = 0;
	entry[i].price   = 0;
	entry[i].size    = 0;
	entry[i].accSize = 0;
      }
    }

    inline void clearPendingTOB() {
      pendingTOB = false;
    }
    
    inline bool hasPendingTOB() {
      return pendingTOB;
    }
    
    /* inline NormPrice getTobNormPrice() { */
    /*   if (! entry[0].valid) return 0; */
    /*   return (entry[0].price - bottom) / step; */
    /* } */

    inline bool isValid(uint idx) {
      return entry[idx].valid;
    }


    inline Price getEntryPrice(uint idx) {
      if (! entry[idx].valid) return 0;
      return entry[idx].price;
    }

    inline uint getEntrySize(uint idx) {
      if (! entry[idx].valid) return 0;
      return entry[idx].size;
    }

    inline bool isPriceWorseThan(Price priceA, Price priceB) {
      if (side == BID) return (priceA <= priceB);
      return (priceA >= priceB);
    }

    /* ----------------------------------------------------- */

    /* inline int getFirePrice(void* dst, uint8_t priceFlavor) { */
    /*   Price tobPrice = getEntryPrice(0)  / static_cast<uint64_t>(1e9); */
    /*   uint642ascii10(reinterpret_cast<char*>(dst), static_cast<uint64_t>(tobPrice), priceFlavor); */
    /*   return 0; */
    /* } */
    /* /\* ----------------------------------------------------- *\/ */

    /* inline int getBetterPrice(void* dst, Price step, uint8_t priceFlavor) { */
    /*   if (getEntryPrice(0) == 0) return 0; */

    /*   Price betterPrice = side == BID ?  */
    /* 	getEntryPrice(0)  / static_cast<uint64_t>(1e9) + step : */
    /* 	getEntryPrice(0)  / static_cast<uint64_t>(1e9) - step; */

    /*   uint642ascii10(reinterpret_cast<char*>(dst), static_cast<uint64_t>(betterPrice), priceFlavor); */
    /*   return 0; */
    /* } */
    /* ----------------------------------------------------- */

    inline bool checkIntegrity() { 
      bool res = true;
      for (uint idx = 0; idx < ENTRIES - 1; idx ++) {
	/* if (! isValid(idx) && isValid(idx + 1)) { */
	/*   EKA_WARN("%s entry %u not valid, but entry %u is valid", */
	/* 	   side == BID ? "BID" : "ASK", idx, idx + 1); */
	/*   res = false; */
	/* } */
	if (isValid(idx) && isValid(idx + 1) &&
	    isPriceWorseThan(getEntryPrice(idx),getEntryPrice(idx + 1))) {
	  //	  EKA_WARN("%s: price %8ju at level %2u is worse than price %8ju at level %2u",
	  on_error("%s: price %8ju at level %2u is worse than price %8ju at level %2u",
		   side == BID ? "BID" : "ASK",
		   getEntryPrice(idx), idx,
		   getEntryPrice(idx + 1), idx + 1);
	  res = false;
	}
      }
      return res;
    }

    void printSide() {
      if (side == SIDE::ASK) {
	printf("ASK:\n");
	for (int idx = ENTRIES - 1; idx >= 0; idx --) {
	  if (! entry[idx].valid) continue;
	  printf("%4u@%4u (%ju)\n",entry[idx].size,idx,entry[idx].price);
	}
      } else {
	printf("\t\t\t\tBID:\n");
	for (uint idx = 0; idx < ENTRIES; idx ++) {
	  if (! entry[idx].valid) continue;
	  printf("\t\t\t\t%4u@%4u (%ju)\n", entry[idx].size,idx,entry[idx].price);
	}
      }
    }
    /* ----------------------------------------------------- */

    inline uint priceLevel2idx(uint8_t priceLevel) {
      if (priceLevel == 0 || priceLevel > ENTRIES )
	on_error("Invalid priceLevel %u",priceLevel);
      return (priceLevel - 1);
    }

    /* ----------------------------------------------------- */

    inline int newPlevel(MdOut*           mdOut,
			 PriceLevelIdx    pLevelIdx,
			 Price            price,
			 Size             size) {
      uint idx = priceLevel2idx(pLevelIdx);
      for (uint i = ENTRIES - 1; i > idx; i--)
	memcpy(&entry[i],&entry[i-1],sizeof(entry[0]));
      entry[idx].size  = size;
      entry[idx].price = price;
      entry[idx].valid = true;
      if (idx < HW_DEPTH_PRICES) {
	//	EKA_LOG("%s pendingTOB",side == BID ? "BID" : "ASK");
	pendingTOB = true;
      }

      return 0;
    }

    /* ----------------------------------------------------- */

    inline int changePlevel(MdOut*           mdOut,
			    PriceLevelIdx pLevelIdx,
			    Price         price,
			    Size          size) {
      uint idx = priceLevel2idx(pLevelIdx);

      entry[idx].size  = size;
      entry[idx].price = price;
      entry[idx].valid = true;

      if (idx < HW_DEPTH_PRICES) {
	//	EKA_LOG("%s pendingTOB",side == BID ? "BID" : "ASK");
	pendingTOB = true;
      }

      return 0;
    }

    /* ----------------------------------------------------- */

    inline int deletePlevel(MdOut*           mdOut,
			    PriceLevelIdx pLevelIdx) {
      uint idx = priceLevel2idx(pLevelIdx);
    
      for (uint i = idx; i < ENTRIES - 1; i ++)
	memcpy(&entry[i],&entry[i+1],sizeof(entry[0]));
      entry[ENTRIES - 1].valid = false;
      entry[ENTRIES - 1].size  = 0; 
      entry[ENTRIES - 1].price = 0; 
    
      if (idx < HW_DEPTH_PRICES) {
	//	EKA_LOG("%s pendingTOB",side == BID ? "BID" : "ASK");
	pendingTOB = true;
      }

      return 0;
    }

    /* ----------------------------------------------------- */

  private:

    class BookEntry {
    public:
      Size     size;
      Size     accSize;
      Price    price;
      bool     valid;
      char     pad[256 - sizeof(size) - sizeof(accSize) - sizeof(price) - sizeof(valid)];
    };

    bool       betterThanTob(uint idx);
    uint       findNextTob(uint idx);

    SIDE       side;
    EkaCmeBook*  parent;

    bool     pendingTOB;
    BookEntry __attribute__ ((aligned(0x100))) entry[ENTRIES];

    uint tob; // pointer to best price entry

    /* Price bottom; // lowest price */
    /* Price top; // highest price */
    /* Price step; */

    /* EkaDev* dev; */
  };

 EkaCmeBook(EkaDev* _dev, EkaStrat* _strat, uint8_t _coreId, EkaProd* _prod) : EkaBook(_dev, _strat,_coreId,_prod){
    bid = new EkaCmeBookSide(BID, this);
    ask = new EkaCmeBookSide(ASK, this);
  }

  /* inline int getFirePrice(void* dst,SIDE side) { */
  /*   return side == BID ? bid->getFirePrice(dst,ei_price_flavor) :  */
  /*     ask->getFirePrice(dst,ei_price_flavor); */
  /* } */


  /* inline int getBetterPrice(void* dst,SIDE side) { */
  /*   return side == BID ? bid->getBetterPrice(dst,getStep(),ei_price_flavor) :  */
  /*     ask->getBetterPrice(dst,getStep(),ei_price_flavor); */
  /* } */

  inline void clearPendingTOB() {
    bid->clearPendingTOB();
    ask->clearPendingTOB();
  }
    
  inline bool hasPendingTOB(SIDE side) {
    return side == SIDE::BID ? bid->hasPendingTOB() : ask->hasPendingTOB();
  }


  inline bool validPrice(Price price) {
    /* if (price < bottom || price >= top) { */
    /*   on_error("price %ju is out of range: %ju .. %ju",price,bottom,top); */
    /*   return false; */
    /* } */
    return true;
  }

  inline bool isValid(SIDE side, uint idx) {
    return side == SIDE::BID ? bid->isValid(idx) : ask->isValid(idx);
  }

  /* inline NormPrice getTobNormPrice(SIDE side) { */
  /*   return side == SIDE::BID ? bid->getTobNormPrice() : ask->getTobNormPrice(); */
  /* } */

  inline Price getEntryPrice(SIDE side, uint idx) {
    return side == SIDE::BID ? bid->getEntryPrice(idx) : ask->getEntryPrice(idx);
  }

  inline uint getEntrySize(SIDE side, uint idx) {
    return side == SIDE::BID ? bid->getEntrySize(idx) : ask->getEntrySize(idx);
  }

  /* inline Price getStep() { */
  /*   return step; */
  /* } */

  inline bool checkIntegrity() {
    if (ask->isValid(0) && bid->isValid(0) && ask->getEntryPrice(0) < bid->getEntryPrice(0)) {
      on_error("ask->getEntryPrice(0) %ju <= bid->getEntryPrice(0) %ju",
	       ask->getEntryPrice(0), bid->getEntryPrice(0));
    }


    return ask->checkIntegrity() && bid->checkIntegrity();
  }

  void printBook() {
    ask->printSide();
    bid->printSide();
  }
  /* ----------------------------------------------------- */
  inline int newPlevel(MdOut*           mdOut,
		       SIDE             side,
		       PriceLevelIdx    pLevelIdx,
		       Price            _price,
		       Size             _size) {
    if (unlikely(! validPrice(_price))) on_error("Price %ju is out-of-range",_price);
    if (side == BID) return bid->newPlevel(mdOut,pLevelIdx,_price,_size);
    if (side == ASK) return ask->newPlevel(mdOut,pLevelIdx,_price,_size);
    return 0;
  }
      
  /* ----------------------------------------------------- */
  
  inline int changePlevel(MdOut*           mdOut,
			  SIDE             side,
			  PriceLevelIdx    pLevelIdx,
			  Price            _price,
			  Size             _size) {
    if (unlikely(! validPrice(_price))) on_error("Price %ju is out-of-range",_price);
    if (side == BID) return bid->changePlevel(mdOut,pLevelIdx,_price,_size);
    if (side == ASK) return ask->changePlevel(mdOut,pLevelIdx,_price,_size);
    return 0;
  }
/* ----------------------------------------------------- */
  inline int deletePlevel(MdOut*           mdOut,
			  SIDE             side,
			  PriceLevelIdx    pLevelIdx) {
    if (side == BID) return bid->deletePlevel(mdOut,pLevelIdx);
    if (side == ASK) return ask->deletePlevel(mdOut,pLevelIdx);
    return 0;
  }

/* ----------------------------------------------------- */



  /* inline ProductId getProdId() { */
  /*   return prodId; */
  /* } */

  // private:
  /* ExchSecurityId   securityId; */
  /* ProductId        prodId; */

  EkaCmeBookSide*  bid;
  EkaCmeBookSide*  ask;

  /* Price            midpoint; */
  /* Price            bottom; // lowest price */
  /* Price            top; // highest price */
  /* Price            step; */

  /* Price            max_book_spread; */

  EkaDev*          dev;
  uint8_t          coreId;
};

#endif

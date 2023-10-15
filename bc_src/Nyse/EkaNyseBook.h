#ifndef _EKA_NYSE_BOOK_H
#define _EKA_NYSE_BOOK_H

#include "eka_macros.h"
#include "eka_hw_conf.h"

#include "EkaNyse.h"
#include "EkaBook.h"

#include "EkaBookTypes.h"
#include "EkaBookOrder.h"
#include "EkaBookPriceLevel.h"

using ProductId      = EkaNyse::ProductId;
using ExchSecurityId = EkaNyse::ExchSecurityId;
using Price          = EkaNyse::Price;
using Size           = EkaNyse::Size;
using NormPrice      = EkaNyse::NormPrice;

using OrderId        = EkaNyse::OrderID;
using PriceLevelIdx  = EkaNyse::PriceLevelIdx;

using CntT           = uint32_t;

using PriceLevel     = EkaBookPriceLevel<Price,Size,CntT>;
using Order          = EkaBookOrder<PriceLevel,OrderId,Price,Size>;

/* ##################################################### */

class EkaNyseBookSide;
class EkaStrat;

class EkaNyseBook : public EkaBook {
 public:
  static const uint ENTRIES4SANITY = 10*1024;

  /* using ProductId      = EkaNyse::ProductId; */
  /* using ExchSecurityId = EkaNyse::ExchSecurityId; */
  /* using Price          = EkaNyse::Price; */
  /* using Size           = EkaNyse::Size; */
  /* using NormPrice      = EkaNyse::NormPrice; */

  /* using OrderId        = EkaNyse::OrderID; */
  /* using PriceLevelIdx  = EkaNyse::PriceLevelIdx; */

  /* using CntT           = uint32_t; */

  //####################################################

  EkaNyseBook(EkaDev* _dev, EkaStrat* strat, uint8_t _coreId, EkaProd* _prod);

  /* ----------------------------------------------------- */

  inline bool validPrice(Price price) {
    /* if (price < bottom || price >= top) { */
    /*   on_error("price %ju is out of range: %ju .. %ju",price,bottom,top); */
    /*   return false; */
    /* } */
    return true;
  }
  /* ----------------------------------------------------- */

  Price getEntryPrice(SIDE side, uint idx);

  /* ----------------------------------------------------- */

  Size getEntrySize(SIDE side, uint idx);

  /* ----------------------------------------------------- */

  void printBook();

  /* ----------------------------------------------------- */

  void printStats();

  /* ----------------------------------------------------- */

  bool checkIntegrity();

  /* ----------------------------------------------------- */

  int addOrder(OrderId orderId, SIDE side, Price price, Size size);

  /* ----------------------------------------------------- */

  int modifyOrder(OrderId orderId, SIDE side, Price price, Size size);

  /* ----------------------------------------------------- */

  SIDE deleteOrder(OrderId orderId, SIDE side);

  /* ----------------------------------------------------- */

  /* inline ExchSecurityId getSecurityId() { */
  /*   return securityId; */
  /* } */
  /* ----------------------------------------------------- */

  /* inline ProductId getProdId() { */
  /*   return prodId; */
  /* } */
  /* ----------------------------------------------------- */

  /* ExchSecurityId        securityId; */
  /* ProductId        prodId; */

  /* Price        max_book_spread; */

  EkaNyseBookSide* bid;
  EkaNyseBookSide* ask;

 private:
  static const uint64_t   debugEvents2stop = 0; //1000;

  uint64_t        eventCnt; // for debug

};

#endif

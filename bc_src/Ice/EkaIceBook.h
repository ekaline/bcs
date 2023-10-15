#ifndef _EKA_ICE_BOOK_H
#define _EKA_ICE_BOOK_H

#include "eka_macros.h"
#include "eka_hw_conf.h"

#include "EkaIce.h"
#include "EkaBook.h"

#include "EkaBookTypes.h"
#include "EkaBookOrder.h"
#include "EkaBookPriceLevel.h"

using ProductId      = EkaIce::ProductId;
using ExchSecurityId = EkaIce::ExchSecurityId;
using Price          = EkaIce::Price;
using Size           = EkaIce::Size;
using NormPrice      = EkaIce::NormPrice;

using OrderId        = EkaIce::OrderId;
using PriceLevelIdx  = EkaIce::PriceLevelIdx;

using CntT           = uint32_t;

using PriceLevel     = EkaBookPriceLevel<Price,Size,CntT>;
using Order          = EkaBookOrder<PriceLevel,OrderId,Price,Size>;

/* ##################################################### */

class EkaIceBookSide;
class EkaStrat;

class EkaIceBook : public EkaBook {
 public:
  static const uint ENTRIES4SANITY = 10*1024;

  /* using ProductId      = EkaIce::ProductId; */
  /* using ExchSecurityId = EkaIce::ExchSecurityId; */
  /* using Price          = EkaIce::Price; */
  /* using Size           = EkaIce::Size; */
  /* using NormPrice      = EkaIce::NormPrice; */

  /* using OrderId        = EkaIce::OrderId; */
  /* using PriceLevelIdx  = EkaIce::PriceLevelIdx; */

  /* using CntT           = uint32_t; */

  //####################################################

  EkaIceBook(EkaDev* _dev, EkaStrat* strat, uint8_t _coreId, EkaProd* _prod);

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


  int addModifyOrder(OrderId orderId, SIDE side, Price price, Size size, bool isAddOrder);

  /* ----------------------------------------------------- */

  SIDE deleteOrder(OrderId orderId);

  /* ----------------------------------------------------- */

  /* inline ExchSecurityId getSecurityId() { */
  /*   return securityId; */
  /* } */
  /* ----------------------------------------------------- */

  /* inline ProductId getProdId() { */
  /*   return prodId; */
  /* } */
  /* ----------------------------------------------------- */

 private:
  /* ExchSecurityId        securityId; */
  /* ProductId        prodId; */

  /* Price        max_book_spread; */

  EkaIceBookSide* bid;
  EkaIceBookSide* ask;

  static const uint64_t   debugEvents2stop = 0; //1000;

  uint64_t        eventCnt; // for debug

};

#endif

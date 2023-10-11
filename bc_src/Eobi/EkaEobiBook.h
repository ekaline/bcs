#ifndef _EKA_EOBI_BOOK_H
#define _EKA_EOBI_BOOK_H

#include <string.h>

#include "eka_macros.h"
#include "eka_hw_conf.h"

#include "EkaBookTypes.h"

#include "EkaEobi.h"
#include "EkaBook.h"

class EkaEobiBookSide;
class EkaStrat;

//##############################################################################

class EkaEobiBook : public EkaBook {
 public:
  using ProductId      = EkaEobi::ProductId;
  using ExchSecurityId = EkaEobi::ExchSecurityId;
  using Price          = EkaEobi::Price;
  using Size           = EkaEobi::Size;
  using NormPrice      = EkaEobi::NormPrice;

  static const uint ENTRIES = 32 * 1024;


  //##############################################################################

  EkaEobiBook(EkaDev* _dev, EkaStrat* strat, uint8_t _coreId, EkaProd* _prod);
  /* ----------------------------------------------------- */
  int invalidate();
  /* ----------------------------------------------------- */

  int orderAdd(uint8_t _side, Price price, Size size) ;
  int orderModify(uint8_t _side, Price prevPrice, Size prevSize,Price price, Size size);
  int orderModifySamePrio(uint8_t _side, Size prevSize,Price price, Size size);
  int orderDelete(uint8_t _side, Price price, Size size);
  int orderPartialExecution(uint8_t _side, Price price, Size size);
  int orderFullExecution(uint8_t _side, Price price, Size size);
  int orderMassDelete();

  /* ----------------------------------------------------- */
  uint64_t getEntryPrice(SIDE side, uint bookEntry);
  uint     getEntrySize(SIDE side,  uint bookEntry);
  /* ----------------------------------------------------- */
  bool     testTobChange(SIDE side, uint64_t ts);
  /* ----------------------------------------------------- */
  void     printBook();
  /* ----------------------------------------------------- */
  void     printTob();
  /* ----------------------------------------------------- */
  bool     validPrice(uint64_t price);
  /* ----------------------------------------------------- */


  inline SIDE decodeSide(uint8_t _side) {
    if (_side == 1) return SIDE::BID;
    if (_side == 2) return SIDE::ASK;
    on_error("unexpected side = 0x%x",_side);
  }
  /* ----------------------------------------------------- */

  bool checkIntegrity();

  /* ----------------------------------------------------- */

  ExchSecurityId   securityId;
  ProductId        prodId;

  Price            bottom; // lowest price
  Price            top; // highest price
  Price            step;


  /* ----------------------------------------------------- */

  EkaEobiBookSide*  bookSide[2]; // 0 - bid, 1 - ask

 private:
  Price            midpoint;

  Price            max_book_spread;
};

#endif

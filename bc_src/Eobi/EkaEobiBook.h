#ifndef _EKA_EOBI_BOOK_H_
#define _EKA_EOBI_BOOK_H_

#include <string.h>

#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "EkaEobiBookSide.h"

#include "EkaEobiTypes.h"
using namespace EkaEobi;

class EkaEurStrategy;

// ################################################

class EkaEobiBook {
public:
  static const uint ENTRIES = 32 * 1024;

  // ################################################

  EkaEobiBook(EkaEurStrategy *strat, Price step,
              Price midPoint);
  /* ------------------------------------------------ */
  int invalidate();
  /* ------------------------------------------------ */

  int orderAdd(uint8_t _side, Price price, Size size);
  int orderModify(uint8_t _side, Price prevPrice,
                  Size prevSize, Price price, Size size);
  int orderModifySamePrio(uint8_t _side, Size prevSize,
                          Price price, Size size);
  int orderDelete(uint8_t _side, Price price, Size size);
  int orderPartialExecution(uint8_t _side, Price price,
                            Size size);
  int orderFullExecution(uint8_t _side, Price price,
                         Size size);
  int orderMassDelete();

  /* ------------------------------------------------ */
  uint64_t getEntryPrice(SIDE side, uint bookEntry);
  uint getEntrySize(SIDE side, uint bookEntry);
  /* ------------------------------------------------ */
  bool testTobChange(SIDE side, uint64_t ts);
  /* ------------------------------------------------ */
  void printBook();
  /* ------------------------------------------------ */
  void printTob();
  /* ------------------------------------------------ */
  bool validPrice(uint64_t price);
  /* ------------------------------------------------ */

  inline SIDE decodeSide(uint8_t _side) {
    if (_side == 1)
      return SIDE::BID;
    if (_side == 2)
      return SIDE::ASK;
    on_error("unexpected side = 0x%x", _side);
  }
  /* ------------------------------------------------ */

  bool checkIntegrity();

  /* ------------------------------------------------ */

  // ExchSecurityId securityId;
  // ProductId prodId;

  Price bottom_; // lowest price
  Price top_;    // highest price
  Price step_;

  /* ------------------------------------------------ */

  EkaEobiBookSide *bookSide[2]; // 0 - bid, 1 - ask

private:
  // Price midpoint;

  // Price max_book_spread;
};

#endif

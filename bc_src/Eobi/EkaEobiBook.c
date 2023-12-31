#include "EkaEobiBook.h"
#include "EkaBcEurProd.h"
#include "EkaEobiBookSide.h"

#include "EkaEobiTypes.h"
using namespace EkaEobi;

/* ----------------------------------------------------- */

EkaEobiBook::EkaEobiBook(EkaEurStrategy *strat,
                         EkaBcEurProd *prod, Price step,
                         Price midPoint) {
  prod_ = prod;

  step_ = step;
  if ((int64_t)midPoint < (int64_t)((ENTRIES / 2) * step_))
    bottom_ = 0;
  else
    bottom_ = midPoint - (ENTRIES / 2) * step_;

  top_ = bottom_ + ENTRIES * step_;

  bookSide[SIDE::BID] =
      new EkaEobiBookSide(BID, step_, bottom_, top_);
  bookSide[SIDE::ASK] =
      new EkaEobiBookSide(ASK, step_, bottom_, top_);
}
/* ----------------------------------------------------- */

int EkaEobiBook::invalidate() {
  bookSide[ASK]->invalidate();
  bookSide[BID]->invalidate();
  return 0;
}
/* ----------------------------------------------------- */

bool EkaEobiBook::validPrice(uint64_t price) {
  return (price > (uint64_t)bottom_) &&
         (price < (uint64_t)top_);
}
/* ----------------------------------------------------- */

int EkaEobiBook::orderAdd(uint8_t _side, Price price,
                          Size size) {
  if (!validPrice(price))
    return 0;
  bookSide[decodeSide(_side)]->addSize(price, size);
  return 0;
}
/* ----------------------------------------------------- */

int EkaEobiBook::orderModify(uint8_t _side, Price prevPrice,
                             Size prevSize, Price price,
                             Size size) {
  if (validPrice(prevPrice))
    bookSide[decodeSide(_side)]->substrSize(prevPrice,
                                            prevSize);
  if (validPrice(price))
    bookSide[decodeSide(_side)]->addSize(price, size);
  return 0;
}
/* ----------------------------------------------------- */

int EkaEobiBook::orderModifySamePrio(uint8_t _side,
                                     Size prevSize,
                                     Price price,
                                     Size size) {
  if (!validPrice(price))
    return 0;
  bookSide[decodeSide(_side)]->substrSize(price, prevSize);
  bookSide[decodeSide(_side)]->addSize(price, size);
  return 0;
}
/* ----------------------------------------------------- */

int EkaEobiBook::orderDelete(uint8_t _side, Price price,
                             Size size) {
  if (!validPrice(price))
    return 0;
  bookSide[decodeSide(_side)]->substrSize(price, size);
  return 0;
}
/* ----------------------------------------------------- */

int EkaEobiBook::orderPartialExecution(uint8_t _side,
                                       Price price,
                                       Size size) {
  if (!validPrice(price))
    return 0;
  bookSide[decodeSide(_side)]->substrSize(price, size);
  return 0;
}
/* ----------------------------------------------------- */

int EkaEobiBook::orderFullExecution(uint8_t _side,
                                    Price price,
                                    Size size) {
  if (!validPrice(price))
    return 0;
  bookSide[decodeSide(_side)]->substrSize(price, size);
  return 0;
}
/* ----------------------------------------------------- */

uint64_t EkaEobiBook::getEntryPrice(SIDE _side,
                                    uint bookEntry) {
  return bookSide[_side]->getEntryPrice(bookEntry);
}
/* ----------------------------------------------------- */

uint EkaEobiBook::getEntrySize(SIDE _side, uint bookEntry) {
  return bookSide[_side]->getEntrySize(bookEntry);
}

/* ----------------------------------------------------- */

bool EkaEobiBook::testTobChange(SIDE side, uint64_t ts) {
  switch (side) {
  case ASK:
    return bookSide[ASK]->testTobChange(ts);
  case BID:
    return bookSide[BID]->testTobChange(ts);
  case BOTH:
    return bookSide[ASK]->testTobChange(ts) ||
           bookSide[BID]->testTobChange(ts);
  default:
    return false;
  }
  //  return bookSide[side]->testTobChange(ts);
}

/* ----------------------------------------------------- */

int EkaEobiBook::orderMassDelete() {
  invalidate();
  return 0;
}

/* ----------------------------------------------------- */

void EkaEobiBook::printBook() {
  bookSide[ASK]->printSide();
  bookSide[BID]->printSide();
}
/* ----------------------------------------------------- */

void EkaEobiBook::printTob() {
  bookSide[ASK]->printTob();
  printf(" --- ");
  bookSide[BID]->printTob();
  printf("\n");
}
/* ----------------------------------------------------- */

bool EkaEobiBook::checkIntegrity() {
  bool res = true;
  if (bookSide[BID]->tobIdx >= bookSide[ASK]->tobIdx) {
    EKA_WARN("bookSide[BID]->tobIdx %u >= "
             "bookSide[ASK]->tobIdx %u",
             bookSide[BID]->tobIdx, bookSide[ASK]->tobIdx);
    res = false;
  }
  return res;
}

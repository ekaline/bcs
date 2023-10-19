#include "EkaBcEurProd.h"

#include "EkaEobiBook.h"

#include "EkaEobiTypes.h"
using namespace EkaEobi;

class EkaEurStrategy;

EkaBcEurProd::EkaBcEurProd(
    EkaEurStrategy *strat, EkaBcSecHandle handle,
    const EkaBcEurProductInitParams *p) {
  strat_ = strat;

  handle_ = handle;

  secId_ = p->secId;
  maxBookSpread_ = p->maxBookSpread;
  midPoint_ = p->midPoint;
  priceDiv_ = p->priceDiv;
  step_ = p->step;
  isBook_ = p->isBook;
  eiPriceFlavor_ = p->eiPriceFlavor;

  hwMidPoint_ = midPoint_ / step_;

  if ((hwMidPoint_ * step_ != midPoint_) && isBook_)
    on_error("Product[%jd]: Misconfiguration: "
             "step_=%ju, "
             "midPoint_=%ju, "
             "hwMidPoint_=%u",
             handle_, step_, midPoint_, hwMidPoint_);

  EKA_LOG("Initialized Product[%jd]: "
          "secId_ = %jd, "
          "midPoint_ = %ju, "
          "step_ = %ju, "
          "hwMidPoint_ = %u, "
          "priceDiv_ = %ju, "
          "%s, ",
          handle_, secId_, midPoint_, step_, hwMidPoint_,
          priceDiv_, isBook_ ? "HAS BOOK" : "NO BOOK");

  book_ = new EkaEobiBook(strat_, step_, midPoint_);
  if (!book_)
    on_error("!book_");
}
/* --------------------------------------------------- */

EkaBCOpResult EkaBcEurProd::setJumpParams(
    const EkaBcEurJumpParams *params) {
  memcpy(&jumpParams_, params, sizeof(*params));
  return EKABC_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaBCOpResult EkaBcEurProd::setReferenceJumpParams(
    EkaBcSecHandle fireProd,
    const EkaBcEurReferenceJumpParams *params) {
  memcpy(&refJumpParams_[fireProd], params,
         sizeof(*params));

  return EKABC_OPRESULT__OK;
}
/* --------------------------------------------------- */

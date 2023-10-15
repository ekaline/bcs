#include "EkaNyseBook.h"
#include "EkaNyseBookSide.h"
#include "EkaBookOrder.h"
#include "EkaBookPriceLevel.h"
#include "EkaProd.h"


EkaNyseBook::EkaNyseBook(EkaDev* _dev, EkaStrat* _strat,uint8_t _coreId, EkaProd* _prod) : EkaBook(_dev,_strat,_coreId,_prod) {
  bid = new EkaNyseBookSide(BID, this);
  ask = new EkaNyseBookSide(ASK, this);
}


inline Price EkaNyseBook::getEntryPrice(SIDE side, uint idx) {
  return side == BID ? bid->getEntryPrice(idx) : ask->getEntryPrice(idx);
}
/* ----------------------------------------------------- */

inline Size EkaNyseBook::getEntrySize(SIDE side, uint idx) {
  return side == BID ? bid->getEntrySize(idx) : ask->getEntrySize(idx);
}

/* ----------------------------------------------------- */

void EkaNyseBook::printBook() {
  ask->printSide();
  bid->printSide();
}
/* ----------------------------------------------------- */

void EkaNyseBook::printStats() {
  ask->printStats();
  bid->printStats();
}
/* ----------------------------------------------------- */

bool EkaNyseBook::checkIntegrity() {
  return ask->checkIntegrity() && bid->checkIntegrity();
}

/* ----------------------------------------------------- */

int EkaNyseBook::addOrder(OrderId orderId, SIDE side, Price price, Size size) {
  if (side == BID) return bid->addOrder(orderId,price,size);
  if (side == ASK) return ask->addOrder(orderId,price,size);
  return 0;
}
/* ----------------------------------------------------- */

int EkaNyseBook::modifyOrder(OrderId orderId, SIDE side, Price price, Size size) {
  if (side == BID) return bid->modifyOrder(orderId,price,size);
  if (side == ASK) return ask->modifyOrder(orderId,price,size);
  return 0;
}
/* ----------------------------------------------------- */

SIDE EkaNyseBook::deleteOrder(OrderId orderId, SIDE _side) {
  if (_side == BID) bid->deleteOrder(bid->findOrder(orderId));
  if (_side == ASK) ask->deleteOrder(bid->findOrder(orderId));
  return _side;
}


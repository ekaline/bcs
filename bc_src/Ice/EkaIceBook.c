#include "EkaIceBook.h"
#include "EkaIceBookSide.h"
#include "EkaBookOrder.h"
#include "EkaBookPriceLevel.h"
#include "EkaProd.h"

EkaIceBook::EkaIceBook(EkaDev* _dev, EkaStrat* _strat,uint8_t _coreId, EkaProd* _prod) : EkaBook(_dev,_strat,_coreId,_prod) {
  bid = new EkaIceBookSide(BID, this);
  ask = new EkaIceBookSide(ASK, this);
}


  inline Price EkaIceBook::getEntryPrice(SIDE side, uint idx) {
    return side == BID ? bid->getEntryPrice(idx) : ask->getEntryPrice(idx);
  }
  /* ----------------------------------------------------- */

  inline Size EkaIceBook::getEntrySize(SIDE side, uint idx) {
    return side == BID ? bid->getEntrySize(idx) : ask->getEntrySize(idx);
  }

  /* ----------------------------------------------------- */

  void EkaIceBook::printBook() {
    ask->printSide();
    bid->printSide();
  }
  /* ----------------------------------------------------- */

  void EkaIceBook::printStats() {
    ask->printStats();
    bid->printStats();
  }
  /* ----------------------------------------------------- */

//  inline bool EkaIceBook::checkIntegrity() {
bool EkaIceBook::checkIntegrity() {
  return ask->checkIntegrity() && bid->checkIntegrity();
}

  /* ----------------------------------------------------- */


//  inline int EkaIceBook::addModifyOrder(OrderId orderId, SIDE side, Price price, Size size, bool isAddOrder) {
int EkaIceBook::addModifyOrder(OrderId orderId, SIDE side, Price price, Size size, bool isAddOrder) {
  if (side == BID) return bid->addModifyOrder(orderId,price,size,isAddOrder);
  if (side == ASK) return ask->addModifyOrder(orderId,price,size,isAddOrder);
  return 0;
}
  /* ----------------------------------------------------- */

//inline int EkaIceBook::deleteOrder(OrderId orderId) {
SIDE EkaIceBook::deleteOrder(OrderId orderId) {
  Order* o = NULL;
  if      ((o = bid->findOrder(orderId)) != NULL) {
    bid->deleteOrder(o);
    return SIDE::BID;
  }
  if ((o = ask->findOrder(orderId)) != NULL) {
    ask->deleteOrder(o);
    return SIDE::ASK;
  }

  return SIDE::IRRELEVANT;
}


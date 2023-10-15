#ifndef _EKA_ICE_BOOK_SIDE_H_
#define _EKA_ICE_BOOK_SIDE_H_

#include "EkaBookTypes.h"
#include "EkaIceBook.h"
#include "EkaBookOrder.h"

class EkaIceBookSide {
 public:
  static const uint ENTRIES = 10; //10*1024;
  static const uint ENTRIES4SANITY = 10*1024;
  static const uint HASH_TABLE_SIZE = 64 * 1024;
  static const uint ORDERS_FREE_LIST_SIZE = HASH_TABLE_SIZE;
  static const uint PLEVELS_FREE_LIST_SIZE = 1024;

  /* ##################################################### */

  /* ----------------------------------------------------- */
  inline uint16_t calcOrderHash(OrderId _orderId) {
    return static_cast<uint16_t>(_orderId && 0xFFFF);
  }

  /* ----------------------------------------------------- */

  Order* findOrder(OrderId orderId);

  /* ----------------------------------------------------- */
  int addOrder2Hash(Order* o, OrderId _id);

  /* ----------------------------------------------------- */

  int addOrder(OrderId _id, Price price, uint32_t _size); 

  /* ----------------------------------------------------- */

  int freeOrder(Order* o) ;
  /* ----------------------------------------------------- */

  Order* getNewFreeOrder() ;
  /* ----------------------------------------------------- */

  int freePLevel(PriceLevel* p);

  /* ----------------------------------------------------- */

  PriceLevel* getNewFreePlevel();
  /* ----------------------------------------------------- */

  PriceLevel* addPlevelAfterTob(Price _price);

  /* ----------------------------------------------------- */

  PriceLevel* addPlevelAfterP(PriceLevel* p, Price _price);

  /* ----------------------------------------------------- */

  PriceLevel* findOrAddPlevel(Price _price);
  /* ----------------------------------------------------- */

  int deletePLevel(PriceLevel* p);

  /* ----------------------------------------------------- */

  int removeOrderFromPLevel(PriceLevel* p, uint32_t _size);

  /* ----------------------------------------------------- */
 
  int removeOrderFromHash(Order* o2rm);
  /* ----------------------------------------------------- */
 
  int addModifyOrder(OrderId _id, Price _price, Size _size, bool isAddOrder);
  /* ----------------------------------------------------- */
 
  int deleteOrder(Order* o);
  /* ----------------------------------------------------- */

  PriceLevel* getPlevelByIdx(uint idx);
  /* ----------------------------------------------------- */
     
  Price getEntryPrice(uint idx);
  /* ----------------------------------------------------- */
     
  uint getEntrySize(uint idx);
  /* ----------------------------------------------------- */

  template <class T> uint64_t countElems(T* head);
  /* ----------------------------------------------------- */
  uint64_t countOrders();
  /* ----------------------------------------------------- */

  void printSide();

  /* ----------------------------------------------------- */
  bool checkPlevels();
  /* ----------------------------------------------------- */

  bool checkOrders();
  /* ----------------------------------------------------- */

  bool checkPlevelsCorrectness();
 
  /* ----------------------------------------------------- */
  bool checkIntegrity();
  /* ----------------------------------------------------- */

  void printStats() ;
  /* ----------------------------------------------------- */

  EkaIceBookSide(SIDE _side, EkaIceBook* _parent) ;
  /* ----------------------------------------------------- */

  //####################################################
 private:
  SIDE           side;
  Order*         ordHashTable[HASH_TABLE_SIZE]; // array of pointers to in-use Orders
  PriceLevel*    tob; // pointer to best price level

  Order*         freeOrders;
  PriceLevel*    freePriceLevels;

  EkaIceBook*    parent;
  /* Price          bottom; // lowest price */
  /* Price          top; // highest price */
  /* Price          step; */


  uint32_t       ordersInUse;
  uint32_t       maxOrdersInUse;

  uint32_t       pLevelsInUse;
  uint32_t       maxPLevelsInUse;
};

#endif

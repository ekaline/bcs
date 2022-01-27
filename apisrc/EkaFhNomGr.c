#include "EkaFhNomGr.h"
#include "eka_fh_q.h"

int EkaFhNomGr::bookInit () {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  return 0;
}

int EkaFhNomGr::invalidateQ () {
  if (! q)
    on_error("Q does not exist");
  return q->invalidate();
}

int EkaFhNomGr::invalidateBook () {
  if (! book)
    on_error("book does not exist");
  return book->invalidate();
}

void EkaFhNomGr::print_q_state () {
  static const int CriticalCollisionsNum = 4;
  static int everMaxSecCollisions = 0;
  static int everMaxOrdCollisions = 0;
  static int everCriticalSecCollisions = 0;
  static int everCriticalOrdCollisions = 0;

  int maxSecCollisions = 0;
  int maxOrdCollisions = 0;
  
  int criticalSecCollisions = 0;
  for (uint64_t i = 0; i < book->SEC_HASH_LINES; i++) {
    if (! book->sec[i]) continue;
    int lineCollisions = 0;
    auto s = book->sec[i];
    while (s) {
      lineCollisions++;
      auto n = s->next;
      s = dynamic_cast<FhSecurity*>(n);
    }
    if (lineCollisions > CriticalCollisionsNum)
      criticalSecCollisions++;
    if (lineCollisions > maxSecCollisions)
      maxSecCollisions = lineCollisions;
    if (maxSecCollisions > everMaxSecCollisions)
      everMaxSecCollisions = maxSecCollisions;
    if (criticalSecCollisions > everCriticalSecCollisions)
      everCriticalSecCollisions = criticalSecCollisions;
  }

  int criticalOrdCollisions = 0;
  for (uint64_t i = 0; i < book->ORDERS_HASH_LINES; i++) {
    if (! book->ord[i]) continue;
    int lineCollisions = 0;
    auto o = book->ord[i];
    while (o) {
      lineCollisions++;
      auto n = o->next;
      o = dynamic_cast<FhOrder*>(n);
    }
    if (lineCollisions > CriticalCollisionsNum)
      criticalOrdCollisions++;
    if (lineCollisions > maxOrdCollisions)
      maxOrdCollisions = lineCollisions;
    if (maxOrdCollisions > everMaxOrdCollisions)
      everMaxOrdCollisions = maxOrdCollisions;
    if (criticalOrdCollisions > everCriticalOrdCollisions)
      everCriticalOrdCollisions = criticalOrdCollisions;
  }
  EKA_LOG("\n========================\n"
	  "%s:%u: gaps %4u, Subscribed Securities: %u\n"
	  "Securities Hash: %ju (0x%jx) lines, collisions: "
	  "max %d, critical (> %d) %d, ever max %d, ever critical %d"
	  "\n"
	  "Orders Hash: %ju (0x%jx) lines, collisions: "
	  "max %d, critical (> %d) %d, ever max %d, ever critical %d",
	  EKA_EXCH_DECODE(exch),id,gapNum,numSecurities,
	  book->SEC_HASH_LINES,book->SEC_HASH_LINES,
	  maxSecCollisions,CriticalCollisionsNum,criticalSecCollisions,
	  everMaxSecCollisions,everCriticalSecCollisions,
	  book->ORDERS_HASH_LINES,book->ORDERS_HASH_LINES,
	  maxOrdCollisions,CriticalCollisionsNum,criticalOrdCollisions,
	  everMaxOrdCollisions,everCriticalOrdCollisions
	  );
  fflush(stdout);
}

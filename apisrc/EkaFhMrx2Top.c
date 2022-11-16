#include "EkaFhMrx2TopGr.h"
#include "EkaFhMrx2Top.h"

EkaFhGroup* EkaFhMrx2Top::addGroup() {
  return new EkaFhMrx2TopGr();
}

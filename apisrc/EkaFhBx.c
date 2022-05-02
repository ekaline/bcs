#include "EkaFhBxGr.h"
#include "EkaFhBx.h"

EkaFhGroup* EkaFhBx::addGroup() {
  return new EkaFhBxGr();
}

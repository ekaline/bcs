#include "EkaFhGemGr.h"
#include "EkaFhGem.h"

EkaFhGroup* EkaFhGem::addGroup() {
  return new EkaFhGemGr();
}

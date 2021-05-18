#include "EkaFhNom.h"
#include "EkaFhNomGr.h"

EkaFhGroup* EkaFhNom::addGroup() {
  return new EkaFhNomGr();
}

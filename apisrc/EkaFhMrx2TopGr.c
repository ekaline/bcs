#include "EkaFhMrx2TopGr.h"

int EkaFhMrx2TopGr::bookInit () {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  return 0;
}

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

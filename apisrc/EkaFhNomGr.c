#include "EkaFhNomGr.h"
#include "eka_fh_book.h"

int EkaFhNomGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new NomBook(pEfhCtx,pEfhInitCtx,this);
  if (book == NULL) on_error("book == NULL, &book = %p",&book);
  ((NomBook*)book)->init();
  return 0;
}


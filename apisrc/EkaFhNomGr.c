#include "EkaFhNomGr.h"
#include "eka_fh_book.h"

#include "EkaFhFullBook.h"

int EkaFhNomGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  /* book = new NomBook(pEfhCtx,pEfhInitCtx,this); */
  /* if (book == NULL) on_error("book == NULL, &book = %p",&book); */
  /* ((NomBook*)book)->init(); */


  fhBook = new FhBook(dev,id,exch);
  if (fhBook == NULL) on_error("fhBook = NULL");

  fhBook->init();

  return 0;
}

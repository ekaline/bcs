#include "EkaFhNomGr.h"
#include "eka_fh_book.h"

#include "EkaFhFullBook.h"

int EkaFhNomGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new NomBook(pEfhCtx,pEfhInitCtx,this);
  if (book == NULL) on_error("book == NULL, &book = %p",&book);
  ((NomBook*)book)->init();

  /* fhBook = new EkaFhFullBook<SCALE, SEC_HASH_SCALE, */
  /*   SecurityIdT, OrderIdT, PriceT, SizeT>(dev,this,exch); */
  
  fhBook = new FhBook(dev,this,exch);
 
  return 0;
}


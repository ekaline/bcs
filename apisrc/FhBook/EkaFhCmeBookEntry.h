#ifndef _EKA_FH_CME_BOOK_ENTRY_H_
#define _EKA_FH_CME_BOOK_ENTRY_H_

template <class PriceT, 
  class SizeT>
  class EkaFhCmeBookEntry {
 public:
  SizeT     size    = 0;
  SizeT     accSize = 0;
  PriceT    price   = 0;
  bool      valid   = 0;
  char      pad[64 - 
		sizeof(size) - 
		sizeof(accSize) - 
		sizeof(price) - 
		sizeof(valid)];
};

#endif

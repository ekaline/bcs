#ifndef _EFH_FAST_BUFFER_H_
#define _EFH_FAST_BUFFER_H_

#include "eka_macros.h"


template <class T, const size_t size>
  class EfhFastBuffer {
 public:
  EfhFastBuffer () {
    wr = 0;
    rd = 0;
  }

  void push (const T* e) {
    if (wr == size)
      on_error("trying to push %ju elements",wr);
    memcpy(&buf[wr++],e,sizeof(T));
  }

  const T* pop() {
    return &buf[rd++];
  }

  size_t getSize() const {
    return wr;
  }
  
 private:
  T buf[size] = {};
  size_t wr = 0;
  size_t rd = 0;
};

#endif

#include "EkaDev.h"
#include "EhpProtocol.h"

EhpProtocol::EhpProtocol(EkaDev* _dev) {
  dev = _dev;
}

int EhpProtocol::download2Hw() {
  copyIndirectBuf2HeapHw_swap4(dev,
			       EhpConfAddr,
			       (uint64_t*)&conf,
			       0, //thrId,
			       sizeof(conf));
  return 0;
}

#include "EhpProtocol.h"
#include "EkaDev.h"

EhpProtocol::EhpProtocol(EkaDev *_dev) { dev = _dev; }

int EhpProtocol::download2Hw() {
  EKA_LOG("Downloading Ehp templates, base=0x%jx, size=%ju",
          EhpConfAddr, sizeof(conf));
  copyBuf2Hw(dev, EhpConfAddr, (uint64_t *)&conf,
             sizeof(conf));

  /* copyIndirectBuf2HeapHw_swap4(dev, */
  /* 			       EhpConfAddr, */
  /* 			       (uint64_t*)&conf, */
  /* 			       0, //thrId, */
  /* 			       sizeof(conf)); */
  return 0;
}

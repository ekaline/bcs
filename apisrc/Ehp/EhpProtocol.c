#include "EhpProtocol.h"
#include "EkaDev.h"

extern EkaDev *g_ekaDev;

EhpProtocol::EhpProtocol(EkaStrategy *strat) {
  dev = g_ekaDev;
  strat_ = strat;
}

int EhpProtocol::download2Hw(int coreId) {
  uint64_t addr = 0x8a000 + coreId * 0x1000;

  EKA_LOG("Downloading Ehp templates: addr=0x%jx, size=%ju",
          addr, sizeof(conf));

  copyBuf2Hw(dev, addr, (uint64_t *)&conf, sizeof(conf));

  /* copyIndirectBuf2HeapHw_swap4(dev, */
  /* 			       EhpConfAddr, */
  /* 			       (uint64_t*)&conf, */
  /* 			       0, //thrId, */
  /* 			       sizeof(conf)); */
  return 0;
}

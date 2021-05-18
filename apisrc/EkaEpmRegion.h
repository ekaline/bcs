#ifndef _EKA_EPM_REGION_H_
#define _EKA_EPM_REGION_H_

#include "EkaEpm.h"
#include "EkaDev.h"

class EkaEpmRegion {
 public:
  EkaEpmRegion(EkaDev* _dev, uint _id, epm_actionid_t _baseActionIdx) {
    dev            = _dev;
    id             = _id;

    baseActionIdx  = _baseActionIdx;
    localActionIdx = 0;

    baseHeapOffs   = id * EkaEpm::HeapPerRegion;
    heapOffs       = baseHeapOffs;

    // writing region's baseActionIdx to FPGA
    eka_write(dev,0x82000 + 8 * id, baseActionIdx);

    EKA_LOG("Created EpmRegion %u: baseActionIdx=%u, baseHeapOffs=%x",
	    id,baseActionIdx,baseHeapOffs);
  }


  EkaDev*   dev            = NULL;
  uint      id             = -1;
  
  epm_actionid_t      baseActionIdx  = -1;
  epm_actionid_t      localActionIdx = 0;

  uint      baseHeapOffs   = 0;
  uint      heapOffs       = 0;
};

#endif

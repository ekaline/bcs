#include "EkaDev.h"
#include "EkaHwExpectedVersion.h"
#include "EkaHwVersion.h"

bool EkaHwVersion::check(EkaDev* dev) {
  uint64_t epmVersion = (eka_read(dev,EPM_VERSION_ADDR) >> EPM_VERSION_SHIFT) & VERSION_MASK;
  if (epmVersion != EKA_EXPECTED_EPM_VERSION) 
    on_error("epmVersion %jx != EKA_EXPECTED_EPM_VERSION %jx",epmVersion,EKA_EXPECTED_EPM_VERSION);

  EKA_LOG ("epmVersion = %jx",epmVersion);

  uint64_t dmaVersion = (eka_read(dev,DMA_VERSION_ADDR) >> DMA_VERSION_SHIFT) & VERSION_MASK;
  if (dmaVersion != EKA_EXPECTED_DMA_VERSION) 
    on_error("dmaVersion %jx != EKA_EXPECTED_DMA_VERSION %jx",dmaVersion,EKA_EXPECTED_DMA_VERSION);
    
  EKA_LOG ("dmaVersion = %jx",dmaVersion);


  return true;
}

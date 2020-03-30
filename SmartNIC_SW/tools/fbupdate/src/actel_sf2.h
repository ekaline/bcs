#ifndef __actel_sf2_h__
#define __actel_sf2_h__

#include "FL_FlashLibrary.h"

int SF2ReadFlash(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress); // Fill read buffer in SF2
int cmdSF2(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv);

#endif /* __actel_sf2_h__ */

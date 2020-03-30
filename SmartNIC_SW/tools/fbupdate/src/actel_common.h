/**
 *      Actel common definitions.
 */

#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "FL_FlashLibrary.h"

extern int32_t FlashSectorEraseMin;
extern int32_t FlashSectorEraseMax;

void ActelWrite(const FL_FlashInfo * pFlashInfo, int Cmd, uint32_t Addr, uint16_t Data);
uint16_t ActelRead(const FL_FlashInfo * pFlashInfo);
uint32_t ActelRevision(const FL_FlashInfo * pFlashInfo);
uint32_t ActelScratchpadRevision(const FL_FlashInfo * pFlashInfo, char* rev_out);
uint16_t ActelReadFlash(const FL_FlashInfo * pFlashInfo, uint32_t FlashAddr);
void ActelFlashReset(const FL_FlashInfo * pFlashInfo);
int ActelMac2Serial(const FL_FlashInfo * pFlashInfo, char* serial);
int ActelFlashAddrToSector(const FL_FlashInfo * pFlashInfo, uint32_t FlashAddr);
uint32_t ActelFlashSectorToAddr(const FL_FlashInfo * pFlashInfo, int sector);
void ActelRebootFPGA(const FL_FlashInfo * pFlashInfo, int image_no);

int HandleCommonFpgaTypes(const FL_FlashLibraryContext * pFlashLibraryContext);
int EraseAndFlashAndVerifyGecko(FL_FlashLibraryContext * pFlashLibraryContext, FL_CardType cardType);

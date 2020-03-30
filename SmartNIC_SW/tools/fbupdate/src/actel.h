/*
 * actel.h
 *
 *  Created on: Mar 29, 2011
 *      Author: rdj
 */

#ifndef ACTEL_H_
#define ACTEL_H_

#include <stdbool.h>
#include <sys/types.h>

#include "FL_FlashLibrary.h"
#include "actel_common.h"

#define ACTEL_STATUS_BOOTED_IMG     (3<<0)
#define ACTEL_STATUS_RESERVED2      (1<<2)
#define ACTEL_STATUS_FAILSAFE       (1<<3)
#define ACTEL_STATUS_FAILSAFE_L     (1<<4)
#define ACTEL_STATUS_POWER_GOOD_N   (1<<5)
#define ACTEL_STATUS_PLL_LOCK_N     (1<<6)
#define ACTEL_STATUS_CLK_LOCK_N     (1<<7)
#define ACTEL_STATUS_ADDRESS_MAP    (0xf<<8)
#define ACTEL_STATUS_RESERVED12     (0xf<<12)

#define SECTOR0DATA_SIZE 0x4000

typedef struct Tsf2DirEntry {
  uint32_t  StartAddr;
  uint32_t  Version;
} Tsf2DirEntry, *Psf2DirEntry;

typedef struct Tsf2Directory {
  struct Tsf2DirEntry Image[7];
  uint32_t  MagicNumber;
  uint32_t  CurrentVersion;
} Tsf2Directory, *Psf2Directory;

int ActelDumpImage(FL_FlashLibraryContext * pFlashLibraryContext, const char* filename, int image_no);
int cmdActel(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv);

int ActelFlashIsReady(const FL_FlashInfo * pFlashInfo, uint32_t FlashAddr);
int ActelWriteFlash(const FL_FlashInfo * pFlashInfo, uint16_t* buffer, uint32_t FlashAddr);
int ActelProtectHcmImage(const FL_FlashInfo * pFlashInfo);
int ActelUnprotectHcmImage(const FL_FlashInfo * pFlashInfo);
int ActelDumpFlash(const FL_FlashInfo * pFlashInfo, const char* filename);
int ActelFlashAddrToBank(uint32_t FlashAddr);
uint32_t ActelFlashBankToAddr(int bank);
void ActelChipErase(const FL_FlashInfo * pFlashInfo);
int ActelSectorErase(const FL_FlashInfo * pFlashInfo, int sector);
int ActelSectorIsLocked(const FL_FlashInfo * pFlashInfo, int sector);
void ActelSectorLock(const FL_FlashInfo * pFlashInfo, int sector);
void ActelSectorUnlockAll(const FL_FlashInfo * pFlashInfo);
int ActelReadMacAddr(const FL_FlashInfo * pFlashInfo, int max_port);
int ActelWriteMacAddr(const FL_FlashInfo * pFlashInfo, const char* serialNo, int max_port);
int ActelFlashToStdout(const FL_FlashInfo * pFlashInfo, uint32_t startAddr, uint32_t len);
int ActelFileToFlash(const FL_FlashInfo * pFlashInfo, uint32_t startAddr, const char *filename);
int ActelFlashToFile(const FL_FlashInfo * pFlashInfo, uint32_t startAddr, uint32_t len, const char *filename);

#endif /* ACTEL_H_ */

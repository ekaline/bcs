/* 
 * File:   flash.h
 * Author: rdj
 *
 * Created on September 26, 2010, 9:01 PM
 */

#ifndef _FLASH_H
#define _FLASH_H

#include <stdio.h>

#include "FL_FlashLibrary.h"

uint8_t swapbit(uint8_t c);

int flash_busy(uint32_t *pRegistersBaseAddress);
int flash_busy_wait(uint32_t *pRegistersBaseAddress, uint32_t timeout_ms);
int flash_dump_file(uint32_t *pRegistersBaseAddress, const char* filename, uint32_t len, int flashSafe);
int flash_verify_file(const FL_FlashLibraryContext * pFlashLibraryContextconst, const char* filename, int flashSafe);
int flash_verify_blank(uint32_t *pRegistersBaseAddress);
int flash_file(const FL_FlashLibraryContext * pFlashLibraryContext, const char * filename, int flashSafe);
int flash_write(const FL_Parameters * pParameters, const FL_FlashInfo * pFlashInfo, const uint32_t addr, uint32_t len, uint8_t *data);
int flash_read(uint32_t *pRegistersBaseAddress, uint32_t addr, uint32_t len, uint16_t *data);
uint32_t flash_id(uint32_t *pRegistersBaseAddress);
int flash_sector_erase(uint32_t *pRegistersBaseAddress, uint8_t sector);
int flash_unlock_sector(uint32_t *pRegistersBaseAddress, uint8_t sector);
int flash_lock_sector(uint32_t *pRegistersBaseAddress, uint8_t sector);
int flash_clear_status(uint32_t *pRegistersBaseAddress);
uint32_t flash_read_status(uint32_t *pRegistersBaseAddress);
int flash_chip_erase(uint32_t *pRegistersBaseAddress);
int FL_CheckFlashFileType(const FL_FlashInfo * pFlashInfo, const char* filename, const char * expectedPartNumber, FL_FpgaType expectedBitFileFpgaType, FL_Parameters * pParameters);
int flash_read_bit_file_header(FILE *f, const char * expectedPartNumber, uint32_t expected_userid);

int FlashFileFpgaTypes_2010_2015_2022(const FL_FlashLibraryContext * pFlashLibraryContext);

int32_t mac_addr_from_serial(const char* serial, uint8_t* mac_arr, uint8_t port);
int32_t mac_addr_write(const FL_Parameters * pParameters, const FL_FlashInfo * pFlashInfo, uint8_t* mac_addr, uint8_t port);
int32_t mac_addr_read(uint32_t *pRegistersBaseAddress, uint8_t port, uint16_t* mac_addr_ptr);
int32_t mac_addr_erase(uint32_t *pRegistersBaseAddress);
int32_t serial2int(const char* serial);
int32_t serial2type(const char* serial);
int32_t mac2serial(char* serial, uint8_t* mac_arr);

#endif  /* _FLASH_H */


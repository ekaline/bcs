/*
 * actel.c
 *
 *  Created on: Mar 29, 2011
 *      Author: rdj
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>

#include "FL_FlashLibrary.h"
#include "actel.h"
#include "actel_common.h"
#include "flash.h"
#include "printlog.h"
#include "fpgaregs.h"
#include "actel_sf2.h"
#include "gecko.h"

#if 0
static void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

/*
    struct timespec t1, t2, res;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    // code to time

    clock_gettime(CLOCK_MONOTONIC, &t2);
    timespec_diff(&t1, &t2, &res);
    printf("sec: %lu nsec: %lu\n", res.tv_sec, res.tv_nsec);

*/
#endif

//Defintion of flash sector0 content
#define PCB_INFO 0x0060 // The lower byte is PCB version, upper byte is FPGA type.

static inline uint16_t swap16bit(uint16_t v) {
    // swap odd and even bits
    v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
    // swap consecutive bit pairs
    v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
    // swap nibbles ...
    v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
    // swap bytes
    v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
    // swap 2-byte pairs - only if 32bit
    //v = ( v >> 16             ) | ( v               << 16);
    return v;
};

#define XC6VLX240T 73859552/8
#define XC5VLX155T 43042304/8

//Function checking that Flash Memory is Ready.
//Spansion Flash Memory is ready when two consecutive reads to the same bank returns the same word.
//Micron Flash Memory is ready when Status Register bit7 is 1.
//Return 0 when ready
//Return 1 when not ready
//Return -1 on error
int ActelFlashIsReady(const FL_FlashInfo * pFlashInfo, uint32_t FlashAddr)
{
    FL_Error errorCode = FL_FlashIsReady(pFlashInfo, FlashAddr);
    if (errorCode == FL_SUCCESS) return 0;
    if (errorCode == FL_ERROR_SPANSION_FLASH_IS_NOT_READY) return 1;
    if (errorCode == FL_ERROR_MICRON_FLASH_IS_NOT_READY) return 2;
    if (errorCode == FL_ERROR_FLASH_IS_NOT_READY) return 1;
    return -1;
}

//Function writing FlashInfo.BufCnt x 16-bit words to Flash Memory.
//FlashAddr must be aligned to FlashInfo.BufCnt.
//Returns 0 if no errors
//Returns -1 if errors
int ActelWriteFlash(const FL_FlashInfo * pFlashInfo, uint16_t * buffer, uint32_t FlashAddr)
{
    FL_Error errorCode = FL_WriteFlashWords(pFlashInfo, FlashAddr, buffer, pFlashInfo->BufferCount);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "Error code %d in ActelWriteFlash\n", errorCode);
    }
    return (int)errorCode;
}

//Function used to write protect an HCM image in the flash
int ActelProtectHcmImage(const FL_FlashInfo * pFlashInfo)
{
    switch (pFlashInfo->FlashCommandSet)
    {
    case FL_FLASH_COMMAND_SET_SPANSION:
        //Calculate Sector number from flash address.
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Write protecting HCM image in flash memory.\n");
        ActelSectorLock(pFlashInfo, 0);
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "HCM image protected.\n");
        break;

    case FL_FLASH_COMMAND_SET_MICRON:
        //Micron Strata Flash doesn't have persistent protection of sectors
        break;

    default:
        {
            static int logOnce = 0;
            if (logOnce == 0)
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Protect HCM image not supported in this flash command set: %u\n", pFlashInfo->FlashCommandSet);
                logOnce = 1;
            }
        }
        break;
    }

    return 0;
}

//Function used to remove write protection on an HCM image in the flash
int ActelUnprotectHcmImage(const FL_FlashInfo * pFlashInfo)
{
    switch (pFlashInfo->FlashCommandSet)
    {
    case FL_FLASH_COMMAND_SET_SPANSION:
        {
            uint32_t i;//loop var
            int Locks[pFlashInfo->SectorCount];//contains locks for all sectors.

            //Read the lock bits for all sectors
            prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Reading locks for individual sectors\n");
            for (i = 0; i < pFlashInfo->SectorCount; i++)
                Locks[i] = ActelSectorIsLocked(pFlashInfo, i);

            ActelSectorUnlockAll(pFlashInfo);//Unlock all sectors

            prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Removing write protection on sector 0\n");
            Locks[0] = 1;//Clear locks for HCM image
            for (i = 0; i < pFlashInfo->SectorCount; i++) {
                if (Locks[i] == 0){//Apply Sector protection to all previously locked sector, except selected image.
                    ActelSectorLock(pFlashInfo, i);
                }
            }
            prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "\nFPGA image unprotected.\n");
        }
        break;

    case FL_FLASH_COMMAND_SET_MICRON:
        //Micron Strata Flash doesn't have persistent protection of sectors
        break;

    default:
        {
            static int logOnce = 0;
            if (logOnce == 0)
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Unprotect HCM image not supported in this flash command set: %u\n", pFlashInfo->FlashCommandSet);
                logOnce = 1;
            }
        }
        break;
    }

    return 0;
}

//Function dumping flash memory to file.
int ActelDumpFlash(const FL_FlashInfo * pFlashInfo, const char* filename)
{
    FILE *imagefile;
    uint32_t RdAddr;
    uint16_t RdData;
    uint32_t MaxAddr;
    FL_Error errorCode = FL_SUCCESS;

    // NOTE that the addresses are word addresses.
    MaxAddr = pFlashInfo->FlashSize / 2;

    imagefile = fopen(filename, "wb");

    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Reading flash\r");

    for (RdAddr = 0; RdAddr < MaxAddr; RdAddr++)
    {
        if (pFlashInfo->FlashCommandSet == FL_FLASH_COMMAND_SET_SF2)
        {
            if ((RdAddr & (pFlashInfo->BufferCount-1)) == 0)
            {
                int state = SF2ReadFlash(pFlashInfo, RdAddr); // Fill read buffer in SF2
                //FL_SF2ReadFlash(pFlashInfo, RdAddr, )
                if (state != 0)  return state;
            }
        }
        RdData = ActelReadFlash(pFlashInfo, RdAddr);//Read data from flash memory
        RdData = ((RdData & 0xFF) << 8) | ((RdData & 0xFF00) >> 8);//byte swap
        fwrite(&RdData, 1, 2, imagefile);
        if ((RdAddr % 0x1000) == 0)//Print progress.
            prn(LOG_SCR | LEVEL_INFO, "Dumping FPGA image @: 0x%X of 0x%X  \r", RdAddr, MaxAddr);
    }

    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Dumping flash: done with no errors                       \n");

    fclose(imagefile);

    return errorCode;
}

#if 0
int ActelFlashAddrToBank(uint32_t FlashAddr)
{
    int Bank;
    //Get bank from flash address. Bank is bit 23:20/24:21/25:22 of FlashAddr depending on flash size
    if(FlashInfo.FlashSize == 512)
    Bank = FlashAddr>>21;
    else if(FLASH_SIZE == 256)
    Bank = FlashAddr>>20;
    else if(FLASH_SIZE == 128)
    Bank = FlashAddr>>19;
    else {
        prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "FlashAddrToBank command: Undefined FLASH_SIZE\n");
        return -1;
    }
    return Bank;
}
uint32_t ActelFlashBankToAddr(int bank)
{
    uint32_t FlashAddr;
    if(bank<16) {
        if(FLASH_SIZE == 512)
        FlashAddr = (uint32_t)bank<<21;
        else if(FLASH_SIZE == 256)
        FlashAddr = (uint32_t)bank<<20;
        else if(FLASH_SIZE == 128)
        FlashAddr = (uint32_t)bank<<19;
        else {
            prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "FlashBankToAddr command: Undefined FLASH_SIZE\n");
            return -1;
        }
    }

    return FlashAddr;
}

#endif


int ActelDumpInfo(const FL_FlashInfo * pFlashInfo)
{
    int i;

    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "FlashSize:%u (0x%X words)\n", pFlashInfo->FlashSize, pFlashInfo->FlashSize / 2);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "LowSecCnt:%u LowSecSize:%u (0x%x words)\n",
        pFlashInfo->LowSectorCount, pFlashInfo->LowSectorSize, pFlashInfo->LowSectorSize / 2);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "MidSecCnt:%u MidSecSize:%u (0x%x words)\n",
        pFlashInfo->MiddleSectorCount, pFlashInfo->MiddleSectorSize, pFlashInfo->MiddleSectorSize / 2);
    for(i=0;i<3;i++){
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "ImageStartAddr[%u]:0x%X SectorsPerImage[%u]:%u\n",
                                 i, pFlashInfo->ImageStartAddress[i], i, pFlashInfo->SectorsPerImage[i]);
    }

    ActelWrite(pFlashInfo, 2, 0x555, 0x98);//CFI entry (same command in Spansion and Micron command sets)

    for(i=0;i<0x40;i++){
        printf("%02x : %02x\n", i, ActelReadFlash(pFlashInfo, i));
    }
    ActelWrite(pFlashInfo, 2, 0x0, 0xF0); //CFI Exit command for bank 0
    return 0;
}

//Function erasing the entire chip(but not protected sectors - should be tested)
//This function doesn't return until the erase is done.
//This can take between 78.4 to 154 SECONDS for the current 128Mbit flash.
void ActelChipErase(const FL_FlashInfo * pFlashInfo)
{
    switch (pFlashInfo->FlashCommandSet)
    {
    case FL_FLASH_COMMAND_SET_SPANSION:
        prn(LOG_SCR | LEVEL_INFO, "                                                                    |\r");
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Erasing entire flash chip |");
        //Wait until flash controller is ready.
        //Only testing bank 0 - This is enough to test.
        while (ActelFlashIsReady(pFlashInfo, 0L) != 0)
            usleep(100);

        //Write Unlock Commands
        ActelWrite(pFlashInfo, 2, 0x555, 0xAA);//Unlock 0
        ActelWrite(pFlashInfo, 2, 0x2AA, 0x55);//Unlock 1
        //Write Chip Erase Commands
        ActelWrite(pFlashInfo, 2, 0x555, 0x80);//Chip Erase 0
        ActelWrite(pFlashInfo, 2, 0x555, 0xAA);//Chip Erase 1
        ActelWrite(pFlashInfo, 2, 0x2AA, 0x55);//Chip Erase 2
        ActelWrite(pFlashInfo, 2, 0x555, 0x10);//Chip Erase 3

        do {
            usleep(5000000);//Sleep 5 seconds
            prn(LOG_SCR | LEVEL_INFO, ".");//Print an alive dot. Should fit between the two pipes. If the flash is slower dots continue through the pipe MS style :-)
            fflush(stdout);
        } while (ActelFlashIsReady(pFlashInfo, 0) != 0);//Wait until flash controller is ready
        prn(LOG_SCR | LEVEL_INFO, "\n");

    case FL_FLASH_COMMAND_SET_MICRON:
        prn(LOG_SCR | LEVEL_WARNING, "Flash chip erase not supported by Strata Flash\n");
        break;

    default:
        {
            static int logOnce = 0;
            if (logOnce == 0)
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR,
                    "Flash chip erase not supported with unknown flash command set %u in ActelChipErase\n",
                    pFlashInfo->FlashCommandSet);
                logOnce = 1;
            }
        }
        break;
    }
}

//Function erasing a single sector
//Return 0 if no errors
//Return -1 if errors
int ActelSectorErase(const FL_FlashInfo * pFlashInfo, int sector)
{
    FL_Error errorCode = FL_FlashEraseSector(pFlashInfo, sector);
    if (errorCode != FL_SUCCESS)
    {
        return -1;
    }
    return 0;
}

//Function returning flash memory sector lock status.
//Return 0 if sector is locked
//Return 1 if sector is unlocked
int ActelSectorIsLocked(const FL_FlashInfo * pFlashInfo, int sector)
{
    FL_Boolean sectorIsLocked = 1;

    FL_Error errorCode = FL_FlashSectorIsLocked(pFlashInfo, sector, &sectorIsLocked);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "AAaaaaaAAArggGGghhhHHH.....\n");
        return 0xDEAD;
    }
    return sectorIsLocked ? 0 : 1;
}

//Function setting Persistent Protection Bits(PPB) for individual sectors
void ActelSectorLock(const FL_FlashInfo * pFlashInfo, int sector)
{
    switch (pFlashInfo->FlashCommandSet)
    {
    case FL_FLASH_COMMAND_SET_SPANSION:
        {
            uint32_t SectorAddr = ActelFlashSectorToAddr(pFlashInfo, sector);

            while (ActelFlashIsReady(pFlashInfo, SectorAddr) != 0)
                ;//Wait until flash controller is ready

            //Write PPB - Non-Volatile Sector Protection Command Set Entry
            ActelWrite(pFlashInfo, 2, 0x555, 0xAA);//Unlock 0
            ActelWrite(pFlashInfo, 2, 0x2AA, 0x55);//Unlock 1
            ActelWrite(pFlashInfo, 2, SectorAddr + 0x555, 0xC0);//Command set entry
            //Program PPB sector lock
            ActelWrite(pFlashInfo, 2, 0x0, 0xA0);//Setup Program Command
            ActelWrite(pFlashInfo, 2, SectorAddr, 0);//Set Lock bit
            while (ActelFlashIsReady(pFlashInfo, SectorAddr) != 0)
                ;//Wait until flash controller is ready
            //Write PPB - Non-Volatile Sector Protection Command Set Exit
            ActelWrite(pFlashInfo, 2, 0x0, 0x90);//PPB Exit 0
            ActelWrite(pFlashInfo, 2, 0x0, 0x00);//PPB Exit 1
            while (ActelFlashIsReady(pFlashInfo, SectorAddr) != 0)
                ;//Wait until flash controller is ready
        }
        break;

    case FL_FLASH_COMMAND_SET_MICRON:
        //Micron Strata Flash doesn't have persistent protection of sectors
        break;

    default:
        {
            static int logOnce = 0;
            if (logOnce == 0)
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR,
                    "Flash sector locking not supported with unknown flash command set %u in ActelSectorLock\n",
                    pFlashInfo->FlashCommandSet);
                logOnce = 1;
            }
        }
        break;
    }
}

//Function unlocking all sectors.
//Returns 0 if no errors
void ActelSectorUnlockAll(const FL_FlashInfo * pFlashInfo)
{
    FL_Error errorCode = FL_FlashUnlockAllSectors(pFlashInfo);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "aAAAAAAAAaaaaaaaarrrrGGGGGGggggHHHHHHHhhh..\n");
    }
}

//Write flash to file.
//@param len is length in bytes
int ActelFlashToFile(const FL_FlashInfo * pFlashInfo, uint32_t startAddr, uint32_t len, const char *filename)
{
    uint16_t flashData;
    uint32_t flashAddr;
    FILE *fileToFlass;

    // len is in bytes, convert to words
    len = (len +1)/2;
    if (startAddr >= pFlashInfo->FlashSize / 2 || startAddr + len > pFlashInfo->FlashSize / 2) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Trying to read outside valid Flash Address range!\n");
        return -1;
    }
    fileToFlass = fopen(filename, "wb");
    if (!fileToFlass) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Unable to create file: %s\n", filename);
        return -1;
    }
    for (flashAddr = startAddr; flashAddr < startAddr + len; flashAddr++)
    {
        if (pFlashInfo->FlashCommandSet == FL_FLASH_COMMAND_SET_SF2)
        {
            if (((flashAddr - startAddr) & (pFlashInfo->BufferCount - 1)) == 0)
            {
                int state = SF2ReadFlash(pFlashInfo, flashAddr); // Fill read buffer in SF2
                if (state != 0)  return state;
            }
        }
        flashData = ActelReadFlash(pFlashInfo, flashAddr);
        // Write data to file
        fwrite(&flashData, 1, 2, fileToFlass);
    }
    return 0;
}

//Write flash to stdout.
//@param len is length in bytes
int ActelFlashToStdout(const FL_FlashInfo * pFlashInfo, uint32_t startAddr, uint32_t len)
{
    uint16_t flashData;
    uint32_t flashAddr;
    // len is in bytes, convert to words
    len = (len +1)/2;
    if (startAddr >= pFlashInfo->FlashSize / 2 || startAddr + len > pFlashInfo->FlashSize / 2) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Trying to read outside valid Flash Address range!\n");
        return -1;
    }
    for (flashAddr = startAddr; flashAddr < startAddr + len; flashAddr++) {
        if (pFlashInfo->FlashCommandSet == FL_FLASH_COMMAND_SET_SF2) {
            if (((flashAddr - startAddr) & (pFlashInfo->BufferCount-1)) == 0) {
                int state = SF2ReadFlash(pFlashInfo, flashAddr); // Fill read buffer in SF2
                if (state != 0)  return state;
            }
        }
        flashData = ActelReadFlash(pFlashInfo, flashAddr);
        printf("%c%c", (char)(flashData&0xFF), (char)(flashData>>8));// Write data to stdout
    }
    return 0;
}

//Write file to flash.
int ActelFileToFlash(const FL_FlashInfo * pFlashInfo, uint32_t startAddr, const char *filename)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint16_t buffer[pFlashInfo->BufferCount];
    FILE *fileToFlass;
    struct stat sbuf;
    uint32_t wordsRead;
    uint32_t bytesTotal=0;
    int i;

    if (startAddr % pFlashInfo->BufferCount != 0) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Start address is not aligned to 0x%X\n", pFlashInfo->BufferCount);
        return -1;
    }
    if (startAddr >= pFlashInfo->FlashSize / 2) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Trying to write outside valid Flash Address range!\n");
        return -1;
    }
    fileToFlass = fopen(filename, "rb");
    if (!fileToFlass) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Unable to open file: %s\n", filename);
        return -1;
    }
    fstat(fileno(fileToFlass), &sbuf);// Get file statistics
    if (startAddr + (sbuf.st_size +1)/2 > pFlashInfo->FlashSize / 2) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Trying to write outside valid Flash Address range!\n");
        return -1;
    }

    if (pFlashInfo->FlashCommandSet == FL_FLASH_COMMAND_SET_MICRON){
        //The Flash chip was reset within ActelInitFlash(), so the sectors must be unlocked again
        uint32_t SectorAddr;
        int StartSector = ActelFlashAddrToSector(pFlashInfo, startAddr);
        int EndSector = ActelFlashAddrToSector(pFlashInfo, startAddr + (sbuf.st_size +1)/2 -1);
        for (i = StartSector; i <= EndSector; i++) {
            //Write Unlock Commands
            SectorAddr = ActelFlashSectorToAddr(pFlashInfo, i);
            ActelWrite(pFlashInfo, 2, SectorAddr, 0x60);//Unlock 0
            ActelWrite(pFlashInfo, 2, SectorAddr, 0xD0);//Unlock 1
        }
    }

    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Writing %s @ 0x%X\n", filename, startAddr);
    do {
        wordsRead = fread(&buffer, 2, pFlashInfo->BufferCount, fileToFlass);
        if (wordsRead == 0)  break;
        for (i = wordsRead; i < pFlashInfo->BufferCount; i++)  buffer[i] = 0; //Fill unused part of buffer with 0
        FL_ExitOnError(pLogContext, FL_WriteFlashWords(pFlashInfo, startAddr, buffer, pFlashInfo->BufferCount));
        startAddr += pFlashInfo->BufferCount;// We write FlashInfo.BufCnt 16-bit words so the address is incremented accordingly.
        bytesTotal += wordsRead*2;
    } while (wordsRead == pFlashInfo->BufferCount);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "%u bytes written\n", bytesTotal);

    return 0;
}

// Parse input cmds
int cmdActel(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    const FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
    const FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    int argp = 0;//Parse counter
    uint32_t addr;
    uint32_t len = 0;
    //uint16_t status; // ELE comment: was written into but never used for anything
    uint16_t data;
    uint32_t actel_revision;
    FL_Error errorCode = FL_SUCCESS;

    while (argc > argp) {
        if (strcmp("write", argv[argp]) == 0) {
            argp += 2;
            if (argc > argp) {
                const char * expectedPartNumber = FL_GetPartNumberFromFpgaModel(pFlashInfo->pErrorContext, FPGA_MODEL_V6_LX240); // "6vlx240tff1156"
                if (strcmp("golden", argv[argp - 1]) == 0) {
                    if ((errorCode = FL_FlashWriteImage(pParameters, pFlashInfo, argv[argp], FL_IMAGE_TARGET_SECONDARY, expectedPartNumber)) != FL_SUCCESS)
                        return errorCode;
                } else if (strcmp("probe", argv[argp - 1]) == 0) {
                    if ((errorCode = FL_FlashWriteImage(pParameters, pFlashInfo, argv[argp], FL_IMAGE_TARGET_PRIMARY, expectedPartNumber)) != FL_SUCCESS)
                        return errorCode;
                }
            }
        } else if (strcmp("verify", argv[argp]) == 0) {
            argp += 2;
            if (argc > argp) {
                const char * expectedPartNumber = FL_GetPartNumberFromFpgaModel(pFlashInfo->pErrorContext, FPGA_MODEL_V6_LX240); // "6vlx240tff1156"
                if (strcmp("golden", argv[argp - 1]) == 0) {
                    if ((errorCode = FL_FlashVerifyImage(pParameters, pFlashInfo, argv[argp], FL_IMAGE_TARGET_SECONDARY, expectedPartNumber)) != FL_SUCCESS)
                        return errorCode;
                } else if (strcmp("probe", argv[argp - 1]) == 0) {
                    if ((errorCode = FL_FlashVerifyImage(pParameters, pFlashInfo, argv[argp], FL_IMAGE_TARGET_PRIMARY, expectedPartNumber)) != FL_SUCCESS)
                        return errorCode;
                }
            }
        } else if (strcmp("erase", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                if (strcmp("golden", argv[argp]) == 0) {
                    FL_ExitOnError(pLogContext, FL_FlashEraseImage(pFlashInfo, FL_IMAGE_TARGET_SECONDARY, NULL));
                } else if (strcmp("probe", argv[argp]) == 0) {
                    FL_ExitOnError(pLogContext, FL_FlashEraseImage(pFlashInfo, FL_IMAGE_TARGET_PRIMARY, NULL));
                } else {
                    addr = strtol(argv[argp], NULL, 0);
                    ActelSectorErase(pFlashInfo, addr);
                }
            }
        } else if (strcmp("chiperase", argv[argp]) == 0) {
            ActelChipErase(pFlashInfo);
        } else if (strcmp("protect", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                if (strcmp("golden", argv[argp]) == 0) {
                    FL_ExitOnError(pLogContext, FL_FlashProtectImage(pFlashInfo, FL_IMAGE_TARGET_SECONDARY));
                } else if (strcmp("probe", argv[argp]) == 0) {
                    FL_ExitOnError(pLogContext, FL_FlashProtectImage(pFlashInfo, FL_IMAGE_TARGET_PRIMARY));
                }
            }
        } else if (strcmp("unprotect", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                if (strcmp("golden", argv[argp]) == 0) {
                    FL_ExitOnError(pLogContext, FL_FlashUnprotectImage(pFlashInfo, FL_IMAGE_TARGET_SECONDARY));
                } else if (strcmp("probe", argv[argp]) == 0) {
                    FL_ExitOnError(pLogContext, FL_FlashUnprotectImage(pFlashInfo, FL_IMAGE_TARGET_PRIMARY));
                }
            }
        } else if (strcmp("reboot", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                if (strcmp("golden", argv[argp]) == 0) {
                    len = 1;
                } else if (strcmp("probe", argv[argp]) == 0) {
                    len = 0;
                }
                argp++;
                // Look for optional timeout value for delayed reboot
                if(argc > argp){
                    len += strtoul(argv[argp], NULL, 0) & ~0x3;
                }
                if (pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA || pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI) {
                    uint32_t *pRegistersBaseAddress = (uint32_t *)pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
                    GeckoRebootFPGA(pRegistersBaseAddress, len);
                } else {
                    ActelRebootFPGA(pFlashInfo, len);
                }
            }
        } else if (strcmp("status", argv[argp]) == 0) {
            uint16_t adminStatus;
            FL_ExitOnError(pLogContext, FL_AdminReadStatus(pFlashInfo, &adminStatus));
        } else if (strcmp("revision", argv[argp]) == 0) {
            actel_revision = ActelRevision(pFlashInfo);
            prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Actel Revision %d.%d.%d.%d\n", (actel_revision >> 24), ((actel_revision >> 16) & 0xFF), ((actel_revision >> 8) & 0xFF), (actel_revision & 0xFF));
        } else if (strcmp("lock", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                addr = strtol(argv[argp], NULL, 0);
                ActelSectorLock(pFlashInfo, addr);
            }
        } else if (strcmp("islocked", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                addr = strtol(argv[argp], NULL, 0);
                data = ActelSectorIsLocked(pFlashInfo, addr);
                prn(LOG_SCR | LEVEL_INFO, "Sector %u lock %u\n", addr, data);
                return data;
            }
        } else if (strcmp("unlockall", argv[argp]) == 0) {
            ActelSectorUnlockAll(pFlashInfo);
        } else if (strcmp("flash2file", argv[argp]) == 0) {
            argp += 3;
            if (argc > argp) {
                addr = strtol(argv[argp - 2], NULL, 0);
                len = strtol(argv[argp], NULL, 0);
                ActelFlashToFile(pFlashInfo, addr, len, argv[argp - 1]);
            }
        } else if (strcmp("file2flash", argv[argp]) == 0) {
            argp += 2;
            if (argc > argp) {
                addr = strtol(argv[argp - 1], NULL, 0);
                ActelFileToFlash(pFlashInfo, addr, argv[argp]);
            }
        } else if (strcmp("flashread", argv[argp]) == 0) {
            argp += 2;
            if (argc > argp) {
                addr = strtol(argv[argp - 1], NULL, 0);
                len = strtol(argv[argp], NULL, 0);
                ActelFlashToStdout(pFlashInfo, addr, len);
            }
        } else if (strcmp("scratch", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                if (strcmp("wr", argv[argp]) == 0) {
                    argp++;
                    if (argc > argp) {
                        data = strtol(argv[argp], NULL, 0);
                        ActelWrite(pFlashInfo, 0xB, 0, data);
                    }
                } else if (strcmp("rd", argv[argp]) == 0) {
                    ActelWrite(pFlashInfo, 0xC, 0, 0);
                    data = ActelRead(pFlashInfo);
                    prn(LOG_SCR | LEVEL_INFO, "Actel scratch pad:0x%04X\n", data);
                    return data;
                }
            }
        } else if (strcmp("dumpinfo", argv[argp]) == 0) {
            return ActelDumpInfo(pFlashInfo);
        } else if (strcmp("-h", argv[argp]) == 0) {
            prn(LOG_SCR | LEVEL_INFO, "Valid commands:\n"
                "write probe|golden <FILE>      : Write image\n"
                "verify probe|golden <FILE>     : Verify image\n"
                "erase probe|golden|<sector>    : Erase image\n"
                "protect probe|golden           : Write protect image\n"
                "unprotect probe|golden         : Remove write protection\n"
                "reboot probe|golden            : Reprogram FPGA\n"
                "chiperase                      : Erase entire flash chip\n"
                "revision                       : Read Actel FPGa revision\n"
                "status                         : Read Actel FPGA status\n"
                "flash2file <addr> <FILE> <len> : Write flash to file\n"
                "flashout <addr> <len>          : Flash to stdout\n"
                "file2flash <addr> <FILE>       : Write file to flash\n"
                "scratch wr|rd [data]           : Write or read scratch pad\n"
                "lock <sector>                  : Lock flash sector\n"
                "islocked <sector>              : Check if sector is locked(0).\n"
                "unlockall                      : Unlock all sectors\n"
                "");
            return 0;
        } else {
            prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Invalid argument: %s\nUse -h for help\n", argv[argp]);
            return -1;
        }
        argp++;
    }

    //port = strtol(argv[0], NULL, 0);
    //len = strtol(argv[1], NULL, 0);

    return 0;
}

/*
 * Read and print all MAC addresses
 */
int ActelReadMacAddr(const FL_FlashInfo * pFlashInfo, int max_port)
{
    union{
        uint8_t d8[256];
        uint16_t d16[128];
    }buffer;
    uint8_t tmp;
    int i, j;

    //Read flash memory
    for(i=0; i<128; i++){
        buffer.d16[i] = ActelReadFlash(pFlashInfo, i);
    }

    //Byte swap buffer
    for(i=0; i<max_port*6; i+=2){
        tmp = buffer.d8[i];
        buffer.d8[i] = buffer.d8[i+1];
        buffer.d8[i+1] = tmp;
    }

    //Print MAC addresses from buffer
    for(i=0;i<max_port;i++){
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "MAC address port %u: %02X", i, buffer.d8[i*6]);
        for(j=1;j<6;j++){
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ":%02X", buffer.d8[i*6+j]);
        }
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "\n");
    }
    return 0;
}


int ActelWriteMacAddr(const FL_FlashInfo * pFlashInfo, const char* serialNo, int max_port)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    union{
        uint8_t d8[256];
        uint16_t sector0_copy[SECTOR0DATA_SIZE];
    }buffer;
    uint8_t tmp;
    int i, j;

    //Read back flash memory so we can erase the sector and update the MAC addresses
    for(i=0; i<SECTOR0DATA_SIZE; i++){
        buffer.sector0_copy[i] = ActelReadFlash(pFlashInfo, i);
    }

    //Write MAC addresses to buffer
    for(i=0;i<max_port;i++){
        if(mac_addr_from_serial(serialNo, buffer.d8+i*6, i) != 0){
            return -1;
        }
    }

    //Check and remove flash lock
    if(ActelSectorIsLocked(pFlashInfo, 0) == 0){
        prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "WARNING: MAC address locked. Will unlock and continue.\n");
        ActelUnprotectHcmImage(pFlashInfo);
    }

    //Erase sector 0
    ActelSectorErase(pFlashInfo, 0);

    //Write MAC address to log
    for(i=0;i<max_port;i++){
       prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Writing MAC address port %u: %02X", i, buffer.d8[i*6]);
       for(j=1;j<6;j++){
           prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ":%02X", buffer.d8[i*6+j]);
       }
       prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "\n");
    }
    //Byte swap image
    for(i=0; i<max_port*6; i+=2){
        tmp = buffer.d8[i];
        buffer.d8[i] = buffer.d8[i+1];
        buffer.d8[i+1] = tmp;
    }

    //Write updated sector 0 to flash memory
    for(i=0; i<SECTOR0DATA_SIZE; i+=pFlashInfo->BufferCount)
    {
        //ActelWriteFlash(pFlashInfo, buffer.sector0_copy+i, i);
        FL_ExitOnError(pLogContext, FL_WriteFlashWords(pFlashInfo, i, buffer.sector0_copy + i, pFlashInfo->BufferCount));
    }
    return 0;
}

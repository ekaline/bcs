/**
 *      Actel common definitions.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "actel_common.h"
#include "actel.h"
#include "flash.h"
#include "fpgaregs.h"
#include "printlog.h"
#include "gecko.h"


int32_t FlashSectorEraseMin = -1;    /**<  */
int32_t FlashSectorEraseMax = -1;    /**<  */


//Function writing to Actel Controller
//Commands:
//1: Async read command
//2: Asynchronous Write command.
//8: Read back revision LSB from Actel FPGA.
//9: Read back revision MSB from Actel FPGA.
//A: Read back status from Actel FPGA.
//B: Scratch pad write command.
//C: Scratch pad read command.
//E: Reboot Virtex-6 with new Image.
void ActelWrite(const FL_FlashInfo * pFlashInfo, int Cmd, uint32_t Addr, uint16_t Data)
{
    FL_Error errorCode = FL_AdminWriteCommand(pFlashInfo, Cmd, Addr, Data);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "AAAAAAAAAAAAAAAAAAAAAARGH AAAAAAAAAAAAARGH!\n");
    }
}



//Function reading from Actel Controller.
//Returns data without ready bit.
uint16_t ActelRead(const FL_FlashInfo * pFlashInfo)
{
    FL_Error errorCode = FL_AdminWaitStatusReady(pFlashInfo); /* Wait until actel controller is ready. */
    if (errorCode == FL_SUCCESS)
    {
        return rd64(pFlashInfo->pRegistersBaseAddress, REG_FLASH_CMD) & 0xFFFF;
    }
    fprintf(stderr, "AAAAAAAAAAAAAARGGGGGGGGGGGGGGGGH!\n");
    return 0xDEAD;
}


//Function reading flash memory.
//Returns flash data.
uint16_t ActelReadFlash(const FL_FlashInfo * pFlashInfo, uint32_t FlashAddr)
{
    uint16_t flashData;
    FL_Error errorCode = FL_AdminReadWord(pFlashInfo, FlashAddr, &flashData);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "AAAAAAAAAAAAAAAAAARGH!\n");
    }
    return flashData;
}


uint32_t ActelRevision(const FL_FlashInfo * pFlashInfo)
{
    uint32_t rev = 0LL;
    FL_Error errorCode = FL_AdminGetSoftwareRevision(pFlashInfo, &rev);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "aaaaaaaaaaAAAAAAAAAAAAAAArrrgggggggGHHHHhhhhhhhh\n");
    }
    return rev;
}

/* Read Atmel Microcontroller revision number. This is written to Actel Scratchpad at boot
 */
uint32_t ActelScratchpadRevision(const FL_FlashInfo * pFlashInfo, char* rev_out)
{
    uint32_t rev = 0LL;
    char rev_str[20];
    FL_AdminWaitStatusReady(pFlashInfo);
    ActelWrite(pFlashInfo, 0xC, 0, 0);//Read scratchpad
    rev = (uint32_t) ActelRead(pFlashInfo);
    sprintf(rev_str, "%d-%d-%d:%d", (rev >> 12)+2016, ((rev >> 8) & 0xF), ((rev >> 3) & 0x1F), (rev & 0x7)+8);
    prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Atmel Revision %s\n", rev_str);
    if(rev_out) {
        strncpy(rev_out, rev_str, 20);
    }
    return rev;
}

//Function resetting Flash
void ActelFlashReset(const FL_FlashInfo * pFlashInfo)
{
    FL_Error errorCode = FL_AdminReset(pFlashInfo);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "AAAaaaAAAAaaarrrRRRRRRRgggGGGHhHHHH...ohohohh\n");
    }
}

int ActelMac2Serial(const FL_FlashInfo * pFlashInfo, char* serial)
{
    int32_t rtn;
    uint16_t flashData;
    int i;
    uint8_t mac[6];
    for(i=0; i<3; i++){
        flashData = ActelReadFlash(pFlashInfo, i);
        mac[i*2] = flashData>>8;
        mac[i*2+1] = flashData&0xFF;
    }
#if 0
    for(i=0; i<6; i++){
        printf(" %02X", mac[i]);
    }
    printf("\n");
#endif
    rtn = mac2serial(serial, mac);
    return rtn;
}

int ActelFlashAddrToSector(const FL_FlashInfo * pFlashInfo, uint32_t FlashAddr)
{
    uint32_t sector;
    FL_Error errorCode = FL_FlashConvertAddressToSector(pFlashInfo, FlashAddr, &sector);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "AAAAAAAAAARGH AAARGH AAAAAAARGH...\n");
        exit(EXIT_FAILURE);
    }
    return (int)sector;
}

//Convert Sector number to an address within that sector
uint32_t ActelFlashSectorToAddr(const FL_FlashInfo * pFlashInfo, int sector)
{
    uint32_t flashWordAddress;
    FL_Error errorCode = FL_FlashConvertSectorToAddress(pFlashInfo, sector, &flashWordAddress);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "AAAAAAAAAARGH AAARGH AAAAUUURGH...\n");
        exit(EXIT_FAILURE);
    }
    return flashWordAddress;
}

//Function which reboots Main FPGA with a new image
void ActelRebootFPGA(const FL_FlashInfo * pFlashInfo, int image_no)
{
    ActelWrite(pFlashInfo, 0xE, 0, image_no);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Main FPGA is rebooted. Remember to reboot PC!\n");
}



// Handle FPGA types that have the same functionality in fbupdate and testfpga
int HandleCommonFpgaTypes(const FL_FlashLibraryContext * pFlashLibraryContext)
{
    const FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_FlashInfo *  pFlashInfo = &pFlashLibraryContext->FlashInfo;
    const FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
    uint32_t * baseAddr = (uint32_t *)pFlashInfo->pRegistersBaseAddress;
    int j, error = 0;
    FL_ImageTarget imageTarget;

    switch (pFpgaInfo->Type)
    {
         case FPGA_TYPE_BERGAMO_TEST:
         case FPGA_TYPE_COMO_TEST:
            if(pParameters->ReadFlashStatus ) {
               uint32_t flashID = flash_read_status(baseAddr);
               prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash ID: 0x%X  [ OK ]\n", flashID);
               prn(LOG_ACP|LEVEL_ALL, "Flash ID: 0x%X &&&&& [ OK ]\\\\ \n", flashID);
            }
            if(pParameters->ClearFlashStatus ) {
               flash_clear_status(baseAddr);
            }
            if( (FlashSectorEraseMin >= 0)
                    && (FlashSectorEraseMin < 256)
                    && (FlashSectorEraseMax >= FlashSectorEraseMin)
                    && (FlashSectorEraseMax < 256)
                    )
            {
                int32_t sector;

                prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash sector %d to %d erase\n", FlashSectorEraseMin, FlashSectorEraseMax);
                for(sector = FlashSectorEraseMin; sector <= FlashSectorEraseMax; sector++)
                {
                    prn(LOG_SCR|LEVEL_INFO, "%d of %d\r", sector - FlashSectorEraseMin + 1, FlashSectorEraseMax - FlashSectorEraseMin + 1);
                    fflush(stdout);
                    flash_unlock_sector(baseAddr, sector);
                    flash_sector_erase(baseAddr, sector);
                }
                prn(LOG_SCR|LEVEL_INFO, "\n");
            }

            uint32_t flashID = flash_id(baseAddr);
            for (imageTarget = FL_IMAGE_TARGET_PRIMARY; imageTarget <= FL_IMAGE_TARGET_SECONDARY; ++imageTarget) {
                if(pParameters->FlashFileName[imageTarget]) {
                   if((flashID&0xFFFF) == 0x891C){
                      if (imageTarget == FL_IMAGE_TARGET_PRIMARY){
                          prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash sector %d to %d erase\n", 0, 127);
                          //Erase old image
                          for(j=0; j<=127; j++) {
                              prn(LOG_SCR|LEVEL_INFO, "%d of %d\r", j, 127);
                              fflush(stdout);
                              flash_unlock_sector(baseAddr, j);
                              flash_sector_erase(baseAddr, j);
                          }
                      }else{
                          prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash sector %d to %d erase\n", 128, 258);
                          //Erase old image
                          for(j=imageTarget*128; j<=258; j++) {
                              prn(LOG_SCR|LEVEL_INFO, "%d of %d\r", j, 258);
                              fflush(stdout);
                              flash_unlock_sector(baseAddr, j);
                              flash_sector_erase(baseAddr, j);
                          }
                      }
                    }
                    else{
                       prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash sector %d to %d erase\n", imageTarget*64, imageTarget*64+63);
                       //Erase old image
                       for(j=imageTarget*64; j<=imageTarget*64+63; j++) {
                           prn(LOG_SCR|LEVEL_INFO, "%d of %d\r", j, 164);
                           fflush(stdout);
                           flash_unlock_sector(baseAddr, j);
                           flash_sector_erase(baseAddr, j);
                       }
                    }                    
                    
                    
                    //Flash image
                    prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash file:%s\n", pParameters->FlashFileName[imageTarget]);
                    if(flash_file(pFlashLibraryContext, pParameters->FlashFileName[imageTarget], imageTarget)==0) {
                        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash file OK\n");
                    }
                    else {
                        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash file  [ FAILED ] \n");
                        error |= 1;
                    }
                }
                if (pParameters->FlashVerifyFileName[imageTarget])
                {
                    prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash verify file:%s\n", pParameters->FlashVerifyFileName[imageTarget]);
                    if(flash_verify_file(pFlashLibraryContext, pParameters->FlashVerifyFileName[imageTarget], imageTarget) == 0) {
                        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash verify file  [ OK ]\n");
                    }
                    else {
                       prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash verify file  [ FAILED ] \n");
                       error |= 1;
                    }
                }
            }
            break;
         case FPGA_TYPE_MARSALA_TEST:
             for (imageTarget = FL_IMAGE_TARGET_PRIMARY; imageTarget <= FL_IMAGE_TARGET_SECONDARY; ++imageTarget) {
                 if(pParameters->FlashFileName[imageTarget]) {
                     prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Marsala flashing not supported\n");
                     error |= 1;
                 }
             }
             break;
         case FPGA_TYPE_TIVOLI_TEST:
         case FPGA_TYPE_TIVOLI_AUGSBURG_2X40:
         case FPGA_TYPE_SAVONA_TEST:         
         case FPGA_TYPE_SAVONA_AUGSBURG_8X10:
         case FPGA_TYPE_SAVONA_AUGSBURG_2X25:
         case FPGA_TYPE_SAVONA_AUGSBURG_2X40:
         case FPGA_TYPE_SAVONA_AUGSBURG_2X100:
         case FPGA_TYPE_SILIC100_AUGSBURG:
             for (imageTarget = FL_IMAGE_TARGET_PRIMARY; imageTarget <= FL_IMAGE_TARGET_SECONDARY; ++imageTarget) {
                 if(pParameters->FlashFileName[imageTarget]) {
                     prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Flashing of this card not supported by this program\n");
                     error |= 1;
                 }
             }
             break;
         default:
             prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Unsupported FPGA image type. Type:0x%X Model:0x%X\n", pFpgaInfo->Type, pFpgaInfo->Model);
             error |= 1;
    }

    return error;
}

/*
 * Shared commands for flashing FPGA images etc for adminStyle Gecko
 */
int EraseAndFlashAndVerifyGecko(FL_FlashLibraryContext * pFlashLibraryContext, FL_CardType cardType)
{
    const FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint32_t * baseAddr = pFlashInfo->pRegistersBaseAddress;

    //The first is separate functions that do not need the rest of functionality
    if (pParameters->ReadPcbVersion)
    {
        uint16_t pcb_ver;
        if (cardType == FL_CARD_TYPE_SAVONA || cardType == FL_CARD_TYPE_TIVOLI)
        {
            pcb_ver = GeckoReadPcbVer(baseAddr);
        }
        else
        {
            FL_ExitOnError(pLogContext, FL_ReadPcbVersion(pFlashInfo, &pcb_ver));
        }
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCB version: %u\n", pcb_ver);
        return 0;
    }

    return 0; //!!! The rest is copy and must be changed to Gecko specific.
#if 0
    if (pParameters->FlashFileName[0] || pParameters->FlashFileName[1] ||
        pParameters->FlashVerifyFileName[0] || pParameters->FlashVerifyFileName[1] ||
        pParameters->FlashErase[0] || pParameters->FlashErase[1]
        || pParameters->FlashFileName[2] || pParameters->FlashVerifyFileName[2] || pParameters->FlashErase[2])
    {
        //ActelInitFlash(baseAddr);
    }

    int i;
    for (i = 0; i < 3; i++) // loop through images, Herculaneum has extra Clock Calibration preload image (i == 2)
    {
        if (cardType == FL_CARD_TYPE_MANGO || cardType == FL_CARD_TYPE_PADUA)
        {
            if (i == 0) // Primary image
            {
                SwitchToMangoPaduaPrimaryFlash(baseAddr);
            }
            else if (i == 1) // Secondary fail-safe image
            {
                SwitchToMangoPaduaSecondaryFlash(baseAddr);
            }
        }

        if (pParameters->FlashErase[i])
        {
            if (ActelEraseImage(baseAddr, i, pParameters->FlashFileName[i]) != 0)
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Flash erase [ FAILED ] \n");
                goto error1;
            }
        }

        if (pParameters->FlashFileName[i])
        {
            //Remove protection (should only be set on Clock Calibration preload image, but just in case...)
            ActelUnprotectImage(baseAddr, i);

            //Erase Image
            if (ActelEraseImage(baseAddr, i, pParameters->FlashFileName[i]) != 0) {
                prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Flash erase [ FAILED ] \n");
                goto error1;
            }
            //Write image
            if (ActelWriteImage(baseAddr, pParameters->FlashFileName[i], i, fpga_part) != 0) {
                prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Flash file [ FAILED ] \n");
                goto error1;
            }
        }

        if (pParameters->FlashVerifyFileName[i])
        {
            //Verify image
            if (ActelVerifyImage(baseAddr, pParameters->FlashVerifyFileName[i], i, fpga_part) != 0)
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Flash verify [ FAILED ] \n");
                goto error1;
            }
        }

        //Protect Clock Calibration preload image
        if ((i == 2) && pParameters->FlashFileName[2])
        {
            ActelProtectImage(baseAddr, i);
        }

        // Switch back to primary (probe) flash
        SwitchToMangoPaduaPrimaryFlash(baseAddr);
        continue;

    error1:
        SwitchToMangoPaduaPrimaryFlash(baseAddr);
        return -1;
    }

    return 0;
#endif
}

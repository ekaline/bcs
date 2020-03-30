#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>

#include "flash.h"
#include "printlog.h"
#include "fpgaregs.h"
#include "serial_list.h"

#ifdef WIN32
    #include <windows.h>
    #define sleep(x)    Sleep(x)  /* sleep is defined by POSIX, not C++ so we need Windows Sleep */
#endif

// Flash commands
#define FLASH_CMD_CLEAR_STATUS  0x0080000000000000LL
#define FLASH_CMD_READ_STATUS   0x0100000000000000LL
#define FLASH_CMD_LOCK_SECTOR   0x0200000000000000LL
#define FLASH_CMD_UNLOCK_SECTOR 0x0400000000000000LL
#define FLASH_CMD_DEVICE_ID     0x0800000000000000LL
#define FLASH_CMD_SECTOR_ERASE  0x1000000000000000LL
#define FLASH_CMD_CHIP_ERASE    0x2000000000000000LL
#define FLASH_CMD_READ          0x4000000000000000LL
#define FLASH_CMD_WRITE         0x8000000000000000LL

#define FLASH_BUSY_MASK         0x8000000000000000LL

// Swap bits in a byte
inline uint8_t swapbit(uint8_t v) {
    // swap odd and even bits
    v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
    // swap consecutive pairs
    v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
    // swap nibbles ... 
    v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
    // swap bytes - only if >8bit
    //v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
    // swap 2-byte pairs - only if 32bit
    //v = ( v >> 16             ) | ( v               << 16);
    return v;
}

//Convert Sector number to an address within that sector
//Due to invert of MSB address pin in FPGA flash controller the 4 low parameter sectors(small sectors) are placed at sector 128 to 131
uint64_t FlashSectorToAddr(int sector) {
    // Sector boundary
    int MidSecSize = 32768; // sectors at 32KB size
    int LowSecSize = 131072; // sectors at 128KB size
    int HighSecSize = 131072; // sectors at 128KB size
    int LowSecCnt = 128;
    int MidSecCnt = 4;
    int LowSecBoundary = LowSecCnt * LowSecSize; 
    int MidSecBoundary = MidSecCnt * MidSecSize; 
    uint64_t FlashAddr;

    //Convert Sector number to an address within that sector
    if (sector < LowSecCnt) {// Low sector limit
        FlashAddr = LowSecSize * sector;
    } else if(sector < (LowSecCnt + MidSecCnt))
    {
        FlashAddr = LowSecBoundary + MidSecSize * (sector - LowSecCnt);
    } else
    {
        FlashAddr = LowSecBoundary + MidSecBoundary + HighSecSize * (sector - (LowSecCnt + MidSecCnt));
    }
    return FlashAddr / 2; //Since it is the address of 16bit words
}

int flash_busy(uint32_t *baseAddr) {
    if( rd64(baseAddr, REG_FLASH_CMD) & FLASH_BUSY_MASK )
        return 1;
    else
        return 0;
}

int flash_busy_wait(uint32_t *baseAddr, uint32_t timeout_ms) {
    while( (flash_busy(baseAddr)==1) && (timeout_ms != 0) ){
        usleep(1000);
        timeout_ms--;
        //printf(".");
    }

    //printf("t-left %u\n", timeout_ms);
    if( timeout_ms == 0)
        return 1;
    else
        return 0;
}

int flash_file(const FL_FlashLibraryContext * pFlashLibraryContext, const char * filename, int flashSafe)
{
    const FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    uint32_t *baseAddr = (uint32_t *)pFlashInfo->pRegistersBaseAddress;
    FILE *f ;
    struct stat sbuf;
    uint8_t data[2048];
    int fileSize;
    int total = 0;
    int bytes;
    uint32_t addr = 0;
    int flash_wr_rtn = 0;
    int flash_is_P30_type  = 0;
    f = fopen(filename, "rb");
    if(f) {
        fstat(fileno(f), &sbuf);// Get file statistics

        //Read bit-file header
        if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT){
            if (FL_FileReadXilinxBitFileHeader2(pFlashInfo, f, pFpgaInfo->PartNumber, pParameters->ExpectedBitFileFpgaType) != FL_SUCCESS){
                return -2;
            }
        }
        fileSize = sbuf.st_size - ftell(f); // Get file size of payload

        // Set initial flash address
        if(flashSafe){
           if ((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C) {
              addr = 1L<<23;// fail safe address
           }else{
              addr = 1L<<22;// fail safe address
           }
        }else{
            addr = 0;// standard address
        }
       if( ((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x8818) || ((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C)) { // if flash is the one on bergamo or como PCB
           flash_is_P30_type = 1;
        }else{
           flash_is_P30_type = 0;
        }

        do {
            if(pParameters->FlashIs8Bit){
                bytes = fread(data, 1, 1024, f);
            }else{
                if(flash_is_P30_type) {
                   bytes = fread(data, 1, 64, f);
                }
                else {
                   bytes = fread(data, 1, 2048, f);
                }
            }
            total += bytes;
            //printf("Read %d : %d bytes\n", bytes, total);
            if(bytes > 0) {
                flash_wr_rtn = flash_write(pParameters, pFlashInfo, addr, bytes, data);
                if(pParameters->FlashIs8Bit){
                    addr += bytes;
                }else{
                    addr += bytes/2;
                }
            }
            if(total%20480 == 0) {
                prn(LOG_SCR|LEVEL_INFO, "%u of %u\r", total, fileSize);
                fflush(stdout);
            }
        }while( (bytes != 0) && (flash_wr_rtn == 0) );
        fclose(f);
        prn(LOG_SCR|LEVEL_INFO, "%u of %u\n", total, fileSize);
    }
    else {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash file: No such file:%s\n", filename);
        return -1;
    }
    return 0;
}

uint16_t prev_flash;
uint16_t prev_file;

int flash_verify_file(const FL_FlashLibraryContext * pFlashLibraryContextconst, const char * filename, int flashSafe)
{
    const FL_Parameters * pParameters = &pFlashLibraryContextconst->Parameters;
    const FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContextconst->FpgaInfo;
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContextconst->FlashInfo;
    uint32_t *baseAddr = (uint32_t *)pFlashInfo->pRegistersBaseAddress;
    FILE *f ;
    struct stat sbuf;
    uint8_t file_data[2048];
    uint16_t flash_data[1024];
    int fileSize;
    int total = 0;
    int bytes;
    int error = 0;
    uint32_t addr = 0;
    int verify_offset;
    uint16_t file_word;
    f = fopen(filename, "rb");
    if(f) {
        fstat(fileno(f), &sbuf);// Get file statistics

        if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT){
            if(FL_FileReadXilinxBitFileHeader2(pFlashInfo, f, pFpgaInfo->PartNumber, pParameters->ExpectedBitFileFpgaType) != FL_SUCCESS){
                return -2;
            }
        }
        fileSize = sbuf.st_size - ftell(f); // Get file size of payload
        prn(LOG_SCR|LOG_FILE|LEVEL_TRACE, "Flash file data start addr:0x%lX\n", ftell(f));

        // Set initial flash address
        if(flashSafe){
           if ((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C) {
              addr = 1L<<23;// fail safe address
           }else{
              addr = 1L<<22;// fail safe address
           }
        }else{
            addr = 0;// standard address
        }
        do {
            bytes = fread(file_data, 1, 2048, f);
            total += bytes;
            //printf("Verify %d : %d bytes\n", bytes, total);
            if(pParameters->FlashIs8Bit){
                bytes *= 2;
            }
            flash_read(baseAddr, addr, bytes, flash_data);
            verify_offset = 0;
            error = 0;
            while( (verify_offset < (bytes/2)) && !error) {
                if(pParameters->FlashIs8Bit){
                    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT){
                        //Bit-file
                        file_word = swapbit(file_data[verify_offset]);
                    }else{
                        //Bin-file
                        file_word = file_data[verify_offset];
                    }
                }else{
                    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT){
                        //Bit-file
                        // prn(LOG_SCR|LOG_FILE|LEVEL_TRACE, "%u bytes of file: %02X %02X\n", verify_offset, file_data[verify_offset*2], file_data[1+verify_offset*2]);
                        file_word = (((uint16_t)swapbit(file_data[verify_offset*2]))<<8) | (uint16_t)swapbit(file_data[1+verify_offset*2]);
                        // prn(LOG_SCR|LOG_FILE|LEVEL_TRACE, "%u bytes of file: %04X\n", verify_offset, file_word);
                    }else{
                        //Bin-file
                        //Byte swap
                        file_word = ((file_data[1+verify_offset*2])<<8) | file_data[verify_offset*2];
                    }
                }
                do{
                    if( file_word != flash_data[verify_offset] ) {
                        flash_read(baseAddr, addr, bytes, flash_data);
                        error++;
                    }else{
                        error = 0;
                    }
                    // Read 5 times if there is an error to make sure that the error exist
                }while( (error < 5) && error);
                if(error){
                    prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash verify error: Addr:0x%X  FILE:0x%04X != FLASH:0x%04X\n", addr+verify_offset, file_word, flash_data[verify_offset]);
                }
                verify_offset++;
            }
            if(total%20480 == 0) {
                prn(LOG_SCR|LEVEL_INFO, "%u of %u\r", total, fileSize);
                fflush(stdout);
            }
            addr += bytes/2;
        }while( (bytes != 0) && !error );
        fclose(f);
        prn(LOG_SCR|LEVEL_INFO, "%u of %u\n", total, fileSize);
    }
    else {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash verify: No such file:%s\n", filename);
        return -1;
    }
    if(error)
        return -2;
    else
        return 0;
}

/**
 * Dump flash memory to file
 */
int flash_dump_file(uint32_t *baseAddr, const char* filename, uint32_t len, int flashSafe) {
    FILE *f ;
    uint8_t file_data[2048];
    uint16_t flash_data[1024];
    int bytes;
    uint32_t addr = 0;
    int i;
    f = fopen(filename, "wb");
    if(f) {
        // Set initial flash address
        if(flashSafe){
           if ((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C) {
              addr = 1L<<23;// fail safe address
           }else{
              addr = 1L<<22;// fail safe address
           }
        }else{
            addr = 0;// standard address
        }
        while ( len > 0) {
            //Set no of bytes to read from flash
            bytes = len>2048 ? 2048:len;
            //Read from flash
            flash_read(baseAddr, addr, bytes, flash_data);
            for(i=0; i<bytes/2; i++) {
#ifdef BYTE_SWAP
                file_data[i*2] =  (flash_data[i]>>8)&0xFF;
                file_data[i*2+1] = flash_data[i]&0xFF;
#else
                file_data[i*2] =    flash_data[i]&0xFF;
                file_data[i*2+1] = (flash_data[i]>>8)&0xFF;
#endif

            }
            //Write flash data to file
            fwrite(file_data, 1, bytes, f);
            addr += bytes/2;
            len -= bytes;
        }
        fclose(f);
    }
    else {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash dump: No such file:%s\n", filename);
        return -1;
    }
    return 0;
}

int flash_verify_blank(uint32_t *baseAddr) {
    uint16_t flash_data[1024];
    int error = 0;
    uint32_t addr = 0;
    int verify_offset;
    uint16_t comp_word = 0xFFFF;

    do {
        flash_read(baseAddr, addr, 2048, flash_data);
        verify_offset = 0;
        while( (verify_offset < 1024) && !error) {
            if( comp_word != flash_data[verify_offset] ) {
                prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash verify blank error: Addr:0x%X  BLANK:0x%04X != FLASH:0x%04X\n", addr+verify_offset, comp_word, flash_data[verify_offset]);
                error = 1;
            }
            verify_offset++;
        }
        addr += 1024;
    }while( (addr<0x800000) && !error );
    if(error)
        return -1;
    else
        return 0;
}
int flash_write(const FL_Parameters * pParameters, const FL_FlashInfo * pFlashInfo, uint32_t addr, uint32_t len, uint8_t *data)
{
    uint32_t *baseAddr = (uint32_t *)pFlashInfo->pRegistersBaseAddress;
    uint64_t writeCmd;
    uint16_t writeAddr = 0;
    uint16_t writeWord;
    //Check that len is an equal number of bytes.
    if(len&1) {
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Flash device: length not modulo 2\n");
        return -10;
    }
    if(!pParameters->FlashIs8Bit){
        len = len/2;//Recalculate to 16-bit words from 8-bit.
    }

    if( flash_busy_wait(baseAddr, 1000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Busy before starting\n");
        return -1;
    }
    else {
        while(writeAddr < len) {
            if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT){
                //Bit-file
                if(pParameters->FlashIs8Bit){
                    writeWord = (swapbit(*(data+writeAddr)));
                }else{
                    writeWord = (swapbit(*(data+writeAddr*2))<<8) | swapbit(*(data+1+writeAddr*2));
                }
            }else if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIN){
                //Bin-file
                if(pParameters->FlashIs8Bit){
                    writeWord = (*(data+writeAddr*2));
                }else{
                    writeWord = ((*(data+1+writeAddr*2))<<8) | (*(data+writeAddr*2));
                }
            }else{
                prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Unsupported value in flashformat\n");
                return -1;
            }
            writeCmd = (writeAddr<<16) | writeWord;
            //Write 16-bit word to internal SRAM
            wr64(baseAddr, REG_FLASH_WR, writeCmd );

            //printf("%04X %04X\n", writeAddr, writeWord);
#if 0
            if( writeAddr%8 == 0)
                printf("%04X : ", writeAddr);
            printf("%04X ", writeWord);
            if( writeWord%8 == 7)
                printf("\n");
#endif
            writeAddr++;
        }
        writeCmd = FLASH_CMD_WRITE | ((uint64_t)len<<32) | (addr&0xFFFFFFLL);
        //Write content of internal SRAM to flash
        wr64(baseAddr, REG_FLASH_CMD,  writeCmd);
        if( flash_busy_wait(baseAddr, 1000) ) {
            prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Write failed - flash still busy\n");
            return -1;
        }
        return 0;
    }
}

/**
 * Read from flash.
 * @param len is number of bytes.
 */
int flash_read(uint32_t *baseAddr, uint32_t addr, uint32_t len, uint16_t *data) {
    uint16_t readAddr = 0;

    //Check that len is an equal number of bytes.
    if(len&1) {
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Flash device: length not modulo 2\n");
        return -10;
    }
    len = len/2;//Recalculate to 16-bit words from 8-bit.
    if( flash_busy_wait(baseAddr, 1000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Busy before starting\n");
        return -1;
    }
    wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_READ | (addr&0xFFFFFFLL) );
    if( flash_busy_wait(baseAddr, 10000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Timeout reading data\n");
        return -2;
    }
    else {
       if( ((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x8818) || ((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C)) { // if flash is the one on bergamo or como PCB
           //printf("Flash device: Reading ok\n");
           while(readAddr != len) {           
               wr64(baseAddr, REG_BERGAMO_FLASH_RD, readAddr);
               data[readAddr] = (uint16_t)(rd64(baseAddr, REG_BERGAMO_FLASH_RD) & 0xFFFF);
#if 0
               if(readAddr%8 == 0)
                   printf("%04X : ", readAddr);
               printf("%04X ", *(data+readAddr));
               if(readAddr%8 == 7)
                   printf("\n");
#endif
               readAddr++;
           }
       }
       else {
           //printf("Flash device: Reading ok\n");
           while(readAddr != len) {           
               wr64(baseAddr, REG_FLASH_RD, readAddr);
               data[readAddr] = (uint16_t)(rd64(baseAddr, REG_FLASH_RD) & 0xFFFF);
#if 0
               if(readAddr%8 == 0)
                   printf("%04X : ", readAddr);
               printf("%04X ", *(data+readAddr));
               if(readAddr%8 == 7)
                   printf("\n");
#endif
               readAddr++;
           }
       }
       return 0;
    }
}

int flash_sector_erase(uint32_t *baseAddr, uint8_t sector) {
    if( flash_busy_wait(baseAddr, 1000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Busy before starting\n");
        return -1;
    }
    if((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C) { // if flash is type "BOTTOM" the one on bergamo or como PCB
       wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_SECTOR_ERASE | FlashSectorToAddr(sector));
    }
    else{
       wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_SECTOR_ERASE | (uint64_t)sector<<16);
    }
    if( flash_busy_wait(baseAddr, 10000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Timeout erasing sector\n");
        return -2;
    }
    else {
        return 0;
    }
}

int flash_unlock_sector(uint32_t *baseAddr, uint8_t sector) {
    if( flash_busy_wait(baseAddr, 1000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Busy before unlocking starting\n");
        return -1;
    }
    if((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C) { // if flash is type "BOTTOM" the one on bergamo or como PCB
       wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_UNLOCK_SECTOR | FlashSectorToAddr(sector));
    }
    else{
       wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_UNLOCK_SECTOR | (uint64_t)sector<<16);
    }
    if( flash_busy_wait(baseAddr, 10000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Timeout unlocking sector\n");
        return -2;
    }
    else {
        return 0;
    }
}
int flash_lock_sector(uint32_t *baseAddr, uint8_t sector) {
    if( flash_busy_wait(baseAddr, 1000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Busy before starting\n");
        return -1;
    }
    if((rd64(baseAddr, REG_FLASH_CMD)&0xFFFFLL) == 0x891C) { // if flash is type "BOTTOM" the one on bergamo or como PCB
       wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_LOCK_SECTOR | FlashSectorToAddr(sector));
    }
    else{
       wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_LOCK_SECTOR | (uint64_t)sector<<16);
    }
    if( flash_busy_wait(baseAddr, 10000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Timeout unlocking sector\n");
        return -2;
    }
    else {
        return 0;
    }
}

int flash_clear_status(uint32_t *baseAddr) {
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash device: Clear status register\n");
    wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_CLEAR_STATUS);
    return 0;
}

uint32_t flash_read_status(uint32_t *baseAddr) {
    uint32_t id;
    wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_READ_STATUS);
    if(flash_busy_wait(baseAddr, 1000) == 1)
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Timeout reading ID\n");
    id = rd64(baseAddr, REG_FLASH_CMD)&0xFFFFFFLL;
    return id;
}

int flash_chip_erase(uint32_t *baseAddr) {
    uint32_t timeout = 0;
    if( flash_busy_wait(baseAddr, 1000) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Busy before starting\n");
        return -1;
    }
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash device: Erasing chip\n");
    wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_CHIP_ERASE);
    prn(LOG_SCR|LEVEL_INFO, "                                                                     |\r|");
    while( (flash_busy(baseAddr)==1) && (timeout < 200) ){
        sleep(1);
        timeout++;
        prn(LOG_SCR|LEVEL_INFO, ".");
        fflush(stdout);
    }
    prn(LOG_SCR|LEVEL_INFO, "\n");
    if( flash_busy(baseAddr ) ) {
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Timeout erasing chip\n");
        return -2;
    }
    else {
        return 0;
    }
}

uint32_t flash_id(uint32_t *baseAddr) {
    uint32_t id;
    wr64(baseAddr, REG_FLASH_CMD, FLASH_CMD_DEVICE_ID);
    if(flash_busy_wait(baseAddr, 1000) == 1)
        prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash device: Timeout reading ID\n");
    id = rd64(baseAddr, REG_FLASH_CMD)&0xFFFFFFLL;
    return id;
}

/**
 * Get serial number from serial string
 */
int32_t serial2int(const char* serial){
    int serial_int = 0;
    //find dot in serial number
    while( (*serial != 0) && (*serial != '.')){
        serial++;
    }
    if( (*serial == 0) || (*(serial+1) == 0) ){
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Invalid serial number\n");
        return -1;
    }
    serial++;
    serial_int = strtol(serial, NULL, 10);
    return serial_int;
}

/**
 * Get type number from serial string
 */
int32_t serial2type(const char* serial){
    int32_t type;
    int l, i;

    for(l=0; l<20; l++){
        if( (serial[l] == '\0' ) || (l == 19) ){
            prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Invalid serial number. Missing dot.\n");
            return -3;
        }
        if( serial[l] == '.'){
            break;
        }
    }
    for(i=0;i<sizeof(serialList)/sizeof(serialList[0]);i++) {
        if( strncmp(serialList[i].name, serial, l) == 0){
            type = serialList[i].type;
            prn(LOG_SCR|LOG_FILE|LEVEL_DEBUG, "Type: %d\n", type);
            return type;
        }
    }
    //No match found
    prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Invalid serial number. No type match.\n");
    type = -2;
    return type;
}

/**
 * Calculate serial from MAC address
 */
int32_t mac2serial(char* serial, uint8_t* mac_arr){
    int32_t rtn = -1;
    int32_t secondPart;
    int i;
    if(!((mac_arr[0] == 0x00) && (mac_arr[1] == 0x21) && (mac_arr[2] == 0xB2))){
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "MAC address is not using a Fiberblaze OUI.\n");
        return -1;
    }
    //Translate type part to first part of the serial
    for(i=0;i<sizeof(serialList)/sizeof(serialList[0]);i++) {
        if( serialList[i].type == mac_arr[3] ){
            strncpy(serial, serialList[i].name, 20);
            rtn = 0;
        }
    }
    if(rtn != 0){
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "No matching type found in MAC to serial translation.\n");
        return -2;
    }
    //Translate the second part of serial
    secondPart = (mac_arr[4]<<4) + (mac_arr[5]>>4);
    sprintf(serial+strlen(serial), ".%04u", secondPart);
    prn(LOG_SCR|LOG_FILE|LEVEL_ALL, "Serial number: %s\n", serial);
    return 0;
}
/**
 * Calculate MAC address from serial number
 */
int32_t mac_addr_from_serial(const char* serial, uint8_t* mac_arr, uint8_t port){
    int32_t serial_int = 0;
    int32_t type;
    serial_int = serial2int(serial);
    type = serial2type(serial);

    if((serial_int>0xFFF) || (serial_int < 1)){
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Serial(%d) number out of bounds.\n", serial_int);
        return -2;
    }
    if( type < 0 ){
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "Type(%d) number out of bounds.\n", type);
        return -3;
    }
    mac_arr[0] = 0x00;
    mac_arr[1] = 0x21;
    mac_arr[2] = 0xB2;
    mac_arr[3] = type & 0xFF;
    mac_arr[4] = (serial_int>>4)&0xFF;
    mac_arr[5] = ((serial_int<<4)&0xF0) | (port&0xF);
    return 0;
}

/**
 * Parse MAC address out of string
 */
int32_t mac_addr_parse(char* mac_str, uint8_t* mac_arr){
    char * byte_str;
    int i = 0;
    if(strlen(mac_str) != 17){
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "MAC address has invalid length\n");
        return -1;
    }
    byte_str = strtok(mac_str, ":");
    while((byte_str != NULL) && (i<6)){
        mac_arr[i] = strtol(byte_str, NULL, 16);
        byte_str = strtok(mac_str, ":");
        i++;
    }
    if((i != 6) || (byte_str != NULL)){
        prn(LOG_SCR|LOG_FILE|LEVEL_ERROR, "MAC address is invalid\n");
        return -2;
    }
    return 0;
}

/**
 * Write MAC address
 */
int32_t mac_addr_write(const FL_Parameters * pParameters, const FL_FlashInfo * pFlashInfo, uint8_t* mac_addr, uint8_t port){
    int i;
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Writing MAC address port %u: %02X", port, mac_addr[0]);
    for(i=1;i<6;i++){
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ":%02X", mac_addr[i]);
    }
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "\n");
    return flash_write(pParameters, pFlashInfo, 0x7F0000+3*port, 6, mac_addr);
}

/**
 * Read MAC address
 * mac_addr_ptr must be able to hold 6 bytes
 */
int32_t mac_addr_read(uint32_t *baseAddr, uint8_t port, uint16_t* mac_addr_ptr){
    return flash_read(baseAddr, 0x7F0000+3*port, 6, mac_addr_ptr);
}

/**
 * Erase all MAC addresses
 */
int32_t mac_addr_erase(uint32_t *baseAddr){
    return flash_sector_erase(baseAddr, 127);
}

/**
 *  Flash FPGA types 2010, 2015 and 2022
 */
int FlashFileFpgaTypes_2010_2015_2022(const FL_FlashLibraryContext * pFlashLibraryContext)
{
    const FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    uint32_t * baseAddr = (uint32_t *)pFlashInfo->pRegistersBaseAddress;
    int j, error = 0;
    FL_ImageTarget imageTarget;

    for (imageTarget = FL_IMAGE_TARGET_PRIMARY; imageTarget <= FL_IMAGE_TARGET_SECONDARY; ++imageTarget)
    {
        if (pParameters->FlashFileName[imageTarget])
        {
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash sector %d to %d erase\n", imageTarget*64, imageTarget*64+63);
            //Erase old image
            for(j=imageTarget*64; j<=imageTarget*64+63; j++)
            {
                prn(LOG_SCR|LEVEL_INFO, "%d of %d\r", j, 64);
                fflush(stdout);
                flash_sector_erase(baseAddr, j);
            }
            //Flash image
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash file:%s\n", pParameters->FlashFileName[imageTarget]);
            if(flash_file(pFlashLibraryContext, pParameters->FlashFileName[imageTarget], imageTarget)==0)
            {
                prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash file OK\n");
            }
            else
            {
                prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash file  [ FAILED ] \n");
                error |= 1;
            }
        }
        if (pParameters->FlashVerifyFileName[imageTarget])
        {
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash verify file:%s\n", pParameters->FlashVerifyFileName[imageTarget]);
            if (flash_verify_file(pFlashLibraryContext, pParameters->FlashVerifyFileName[imageTarget], imageTarget) == 0)
            {
                prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Flash verify file  [ OK ]\n");
            }
            else
            {
                prn(LOG_SCR|LOG_FILE|LEVEL_WARNING, "Flash verify file  [ FAILED ] \n");
                return -1;
            }
        }
    }

    return error;
}

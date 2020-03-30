/*
 ***************************************************************************
 *
 * Copyright (c) 2008-2019, Silicom Denmark A/S
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Silicom nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with a
 *  Silicom network adapter product.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "printlog.h"
#include "fpgaregs.h"
#include "flash.h" //mac_addr_from_serial()
#include "gecko.h"
#include "admin_commands.h"

/*
 * Wait for FPGA to indicate Gecko ready, maybe with data
 */
static int GeckoWaitReadyData(uint32_t * pRegistersBaseAddress, uint16_t * pData)
{
    uint32_t regVal;
    int timeout = 20000;
    while (1)
    {
        regVal = rd32(pRegistersBaseAddress, REG_FLASH_CMD);
        if ((regVal & (1UL << 16)) == (1UL << 16))
        {//If controller is ready
            *pData = regVal & 0xFFFF; // return value even if gecko timed out
            if ((regVal & (1UL << 18)) == (1UL << 18))
            {//If timeout flag set
                prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "timeout reg: 0x%0X, Read: 0x%0X, timeout %d\n",
                    regVal, *pData, timeout);
                return 2;
            }
            else
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "reg: 0x%0X, Read: 0x%0X, timeout %d\n",
                    regVal, *pData, timeout);
            }
            return 0;// OK
        }
        else
        {
            if (--timeout == 0) {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Timeout waiting for Gecko\n");
                return 1;
            }
            usleep(10); //with this value it polls 1-3 times
        }
    }
}

/*
 * Wait for FPGA to indicate Gecko ready for new command (discard data)
 */
static int GeckoWaitReady(uint32_t * pRegistersBaseAddress)
{
    uint16_t dummyData;
    int returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &dummyData);
    if (returnCode == 2)
    {
        return 0;  //ignore timeout flag, when just waiting for ready
    }
    return returnCode;
}

/*
 * Write a command to Gecko
 */
int GeckoWrite(uint32_t * pRegistersBaseAddress, const uint8_t cmd, uint16_t subCmd, uint32_t data)
{
    uint64_t regVal;
    regVal = ((uint64_t)cmd & 0xF) << 60 | ((uint64_t)subCmd & 0xFFF) << 48 | data;
    prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "GeckoWrite cmd: 0x%lX\n", regVal);
    //add check ready before write ?
    if (GeckoWaitReady(pRegistersBaseAddress) != 0)
    {//Gecko timed out
        return 1;
    };
    wr64(pRegistersBaseAddress, REG_FLASH_CMD, regVal);//Write command to Gecko Controller
    return 0;
}

/*
 * Read and print all MAC addresses
 */
int GeckoReadMacAddr(uint32_t *pRegistersBaseAddress, int maxPort)
{
    uint8_t buffer[GeckoFlash1PageSize];
    int i, j;

    GeckoReadLenFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, 0, maxPort*6, buffer);

    //Print MAC addresses from buffer
    for (i = 0; i < maxPort; i++)
    {
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "MAC address port %u: %02X", i, buffer[i*6]);
        for (j=1; j<6; j++)
        {
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ":%02X", buffer[i*6+j]);
        }
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "\n");
    }
    return 0;
}

/*
 * Read and print serial.
 */
int GeckoReadSerial(uint32_t *pRegistersBaseAddress)
{
    uint32_t rtn;
    char serial[30];
    uint8_t mac[6];

    GeckoReadLenFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, 0, 6, mac);
    rtn = mac2serial(serial, mac);
    return rtn;
}


/*
 * Read Flash1Page 2 times and compare (for debug)
 */
int GeckoReadTest(uint32_t *pRegistersBaseAddress)
{
    uint8_t buffer1[GeckoFlash1PageSize];
    uint8_t buffer2[GeckoFlash1PageSize];
    int i;
    int error = 0;
    GeckoReadFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer1);
    GeckoReadFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer2);

    for (i = 0; i < GeckoFlash1PageSize; i++)
    {
        if (buffer1[i] != buffer2[i])
        {
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Error at %d 1:0x%0X 2:0x%0X\n", i, buffer1[i], buffer2[i]);
            error++;
        }
    }
    if ( error == 0 )
    {
         prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "[ OK ]\n");
    }
    else
    {
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Found %d errors [ FAIL ]\n", error);
    }
    return 0;
}

int GeckoWriteMacAddr(uint32_t *pRegistersBaseAddress, const char* serialNo, int maxPort)
{
    uint8_t buffer[GeckoFlash1PageSize];
    int i, j;
    //memset(buffer, 0x0, GeckoFlash1PageSize);
    GeckoReadFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer);

    //Write MAC addresses to buffer
    for (i = 0; i < maxPort; i++)
    {
        if (mac_addr_from_serial(serialNo, buffer + i * 6, i) != 0)
        {
            return 1;
        }
    }

    //Write MAC address to log
    for (i = 0; i < maxPort; i++)
    {
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "Writing MAC address port %u: %02X", i, buffer[i*6]);
        for (j = 1; j < 6; j++)
        {
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ":%02X", buffer[i*6+j]);
        }
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "\n");
    }

    GeckoWriteFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer);

    return 0;
}

const uint32_t GECKO_PCB_INFO = 0x0060 * 2; // byte address of PCB version

int GeckoReadPcbVer(uint32_t *pRegistersBaseAddress)
{
    uint8_t data[2]; // space for 2 bytes required
    if (GeckoReadLenFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, GECKO_PCB_INFO, 2, data) == 0)
    {
        prn(LOG_SCR|LOG_FILE|LEVEL_DEBUG, "PCB version 0: %d, 1: %d\n", data[0], data[1]);
        return data[0];
    };
    return 0;
}


int GeckoWritePcbVer(uint32_t *pRegistersBaseAddress, int pcbVer)
{
    if (pcbVer < 0 || pcbVer > 255)
    {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: illegal PCB version number\n");
        return 1;
    }
    else
    {
        uint8_t buffer[GeckoFlash1PageSize];
        GeckoReadFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer);

        // Only use the last byte for PCB ver, other reserved for FPGA type;
        uint8_t  pcbVerData = pcbVer & 0xFF;
        buffer[GECKO_PCB_INFO] = pcbVerData;
        GeckoWriteFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer);

        //Check read
        uint8_t pcbVerRead[2]; // space for 2 bytes required
        GeckoReadLenFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, GECKO_PCB_INFO, 2, pcbVerRead);
        prn(LOG_SCR|LOG_FILE|LEVEL_DEBUG, "PCB version written: %d, read: %d\n", pcbVerData, pcbVerRead[0]);
        if (pcbVerRead[0] == pcbVerData)
        {
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "PCB version written: %d\n", pcbVerData);
        }
        else
        {
            prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: PCB version number verification failed\n");
            return 1;
        }
    }
    return 0;
}

/* Read len bytes from Flash1Page from offset*/
int GeckoReadLenFlash1Page(uint32_t *pRegistersBaseAddress, const GeckoFlashArea geckoFlashArea,
                           const uint16_t offset, const uint16_t len, uint8_t *data)
{
    unsigned int i = 0;
    unsigned int subCmd = ~0;
    switch (geckoFlashArea)
    {
    case GECKO_FLASH_PLL:
        subCmd = GECKO_SUBCMD_FLASH_READ_PLL;
        break;
    case GECKO_FLASH_SILICOM_AREA_1:
        subCmd = GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_1;
        break;
    case GECKO_FLASH_SILICOM_AREA_2:
        subCmd = GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_2;
        break;
    case GECKO_FLASH_SILICOM_AREA_3:
        subCmd = GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_3;
        break;
    case GECKO_FLASH_SILICOM_AREA_4:
        subCmd = GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_4;
        break;

    }
    //Define data as an uint16_t array
    uint16_t * pData16 = (uint16_t *)data;
    for (i = 0; i < len / 2; i++)
    {
        //The byte sequence in flash is kept in byte data array by reading 2 bytes as an uint16_t at a time
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_READ_FLASH, subCmd, offset + (i << 1));
        if (GeckoWaitReadyData(pRegistersBaseAddress, &pData16[i]) != 0)
        {//Gecko timed out
            return 1;
        };
    }
    return 0;
}


/* Read whole Flash1Page */
int GeckoReadFlash1Page(uint32_t *pRegistersBaseAddress, const GeckoFlashArea geckoFlashArea, uint8_t *data)
{
    GeckoReadLenFlash1Page(pRegistersBaseAddress, geckoFlashArea, 0, GeckoFlash1PageSize, data);
    return 0;
}


/* Erase and write whole Flash1Page */
int GeckoWriteFlash1Page(uint32_t *pRegistersBaseAddress, const GeckoFlashArea geckoFlashArea, uint8_t *data)
{
    unsigned int subCmd = ~0;
    switch (geckoFlashArea)
    {
    case GECKO_FLASH_PLL:
        subCmd = GECKO_SUBCMD_FLASH_WRITE_START_PLL;
        break;
    case GECKO_FLASH_SILICOM_AREA_1:
        subCmd = GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_1;
        break;
    case GECKO_FLASH_SILICOM_AREA_2:
        subCmd = GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_2;
        break;
    case GECKO_FLASH_SILICOM_AREA_3:
        subCmd = GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_3;
        break;
    case GECKO_FLASH_SILICOM_AREA_4:
        subCmd = GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_4;
        break;
    }

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, subCmd, 0);
    //data: Startpage=1, Endpage=1
    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_ERASE_PAGES, 0x0101);
    //Must wait for erase to finish, is there a check cmd?
    usleep(100000);
    {
        unsigned int i = 0;
        uint32_t * pData32 = (uint32_t *)data;
        for (i = 0; i < GeckoFlash1PageSize / 4; i++)
        {
            //The byte sequence in data array is kept in flash by writing 4 bytes as an uint32_t at a time
            //prn(LOG_SCR|LEVEL_DEBUG,"write at: 0x%0X ", i);
            GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_DATA, pData32[i]);
        }
    }
    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_END, 0);
    return 0;
}


/* Erase and write Image 2 */
int GeckoWriteImage2Flash(uint32_t *pRegistersBaseAddress, uint64_t slotId, char *filename)
{
    uint32_t seqTx;
    uint32_t eraseReady=0;
    uint16_t flashStatus;
    int returnCode;
    char line[4];
    FILE *fin;
    int total=0;
    uint16_t imgVerified = 0;
    uint16_t retryCount;

    if (slotId > 1)
    {
       prn(LOG_SCR | LEVEL_INFO, "   Illegal slotId '%lu' (should be 0 or 1).\n", slotId);
        return 1;
    }

    fin = fopen(filename, "rb");
    if (fin == NULL)
    {
       prn(LOG_SCR | LEVEL_INFO, "   Can't open file '%s' for reading.\n", filename);
        return 1;
    }

    if (slotId == 0)
    {
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_START_IMAGE_1, 0);
    } else {
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_START_IMAGE_2, 0);
    }

    usleep(10000);

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_ERASE_PAGES, 0x0000);
    //Must wait for erase to finish, is there a check cmd?

    while (eraseReady == 0)
    {
        usleep(100000);
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_FLASH_STATUS, 0);
        usleep(100000);
        returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &flashStatus);
        if (returnCode == 1)
        {//Gecko timed out
            return 1;
        }

        if (returnCode == 2)
        {
            eraseReady = 0;
        }
        else
        {
            if (flashStatus == 0)
            {
                eraseReady = 1;
            }
        }
        prn(LOG_SCR|LOG_FILE|LEVEL_DEBUG,"flash state: %d - Erase ready: %d\n", flashStatus, eraseReady);
    }
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Image erased\n");

    while (!feof(fin) && !ferror(fin))
    {
        size_t numberOfItemsRead;

        // Temporary fix for Gecko SW releases before 1.0.2.0 (Can be removed later, issue fixed in Gecko SW)
        usleep(1000);

        line[0] = '\0';
        numberOfItemsRead = fread(line, 4, 1, fin);
        numberOfItemsRead = numberOfItemsRead;
        seqTx = (((line[3]) & 0xFF) << 24) | (((line[2]) & 0xFF) << 16) | (((line[1]) & 0xFF) << 8) | ((line[0]) & 0xFF);
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_DATA, seqTx);
        total+=4;
    }
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Image flashed\n");

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_END, 0);

    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Image being Verified\n");

    usleep(5000000);

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_READ_FLASH, GECKO_SUBCMD_FLASH_READ_VERIFIED_IMAGE_1, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &imgVerified);
    retryCount = 0;
    while (returnCode != 0)
    {
        retryCount++;
        if (retryCount > 10)
        {
            //Gecko timed out
            prn(LOG_SCR | LEVEL_INFO, "Error reading Image Verify status\n");
            return 0;
        }
        else
        {
            prn(LOG_SCR | LEVEL_INFO, "%d - ", retryCount);
            usleep(1000000);
        }
        returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &imgVerified);
    }
    if (imgVerified == GECKO_VERIFY_SUCCEDED)
    {
        prn(LOG_SCR | LEVEL_INFO, "Gecko Image Verified OK\n");
    }
    else
    {
        if (imgVerified == GECKO_VERIFY_FAILED)
        {
            prn(LOG_SCR | LEVEL_INFO, "Gecko Image Verification Failed\n");
        }
        else
        {
            prn(LOG_SCR | LEVEL_INFO, "Gecko Image Unknown verification status (%d)\n", imgVerified);
            prn(LOG_SCR | LEVEL_INFO, "Not supported before Gecko SW version 1.0.2.0\n");
        }
    }

    return 0;
}


/* Erase and write PLL Image to FLASH */
int GeckoWritePll2Flash(uint32_t *pRegistersBaseAddress, char *filename)
{
    FILE *fin;
    char pll_buffer[GeckoFlash1PageSize]; //gecko page size is 4096
    int i = 0, j = 0, image_size=0;
    char admin_data[20];
    memset(admin_data,0,sizeof(admin_data));
    uint8_t pad_size=0;
    uint32_t txData = 0;
    uint32_t eraseReady=0;
    uint16_t flashStatus;
    int returnCode;

    fin = fopen(filename, "rb");
    if (fin == NULL)
    {
       prn(LOG_SCR | LEVEL_INFO, "   Can't open file '%s' for reading.\n", filename);
        return 1;
    }


    while (fread(pll_buffer+i, 1, 1, fin) == 1) {
        i++;
        if(i==GeckoFlash1PageSize) { //max size of LTC configuration
           prn(LOG_SCR | LEVEL_INFO, "Check PLL configuration binary file. Invalid length>=4096\n");
            exit(-1);
        }
    }
    if(!feof(fin))
       prn(LOG_SCR | LEVEL_INFO, "An error occurred while reading the file\n");
    else
       prn(LOG_SCR | LEVEL_INFO, "Read %u bytes\n",i);

    image_size = i;

    //pad to word size.
    pad_size = i%4;
    if (pad_size != 0)
    {
        for(j=0;j<(4-pad_size);j++)
            pll_buffer[i++] = 0; //pad dummy bytes.
        image_size += (4-pad_size);
    }

    memset(admin_data,0,sizeof(admin_data));

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_START_PLL, 0);


    usleep(10000);

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_ERASE_PAGES, 0x0101);
    //Must wait for erase to finish, is there a check cmd?

    while (eraseReady == 0)
    {
        usleep(100000);
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_FLASH_STATUS, 0);
        usleep(100000);
        returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &flashStatus);
        if (returnCode == 1)
        {//Gecko timed out
            return 1;
        }

        if (returnCode == 2)
        {
            eraseReady = 0;
        }
        else
        {
            if (flashStatus == 0)
            {
                eraseReady = 1;
            }
        }
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"flash state: %d - Erase ready: %d\n", flashStatus, eraseReady);
    }


    j = 0;
    //iterate through pll data buffer and write 4 bytes at a time.
    while(1) {
        if (j >= image_size)
            break;
        memcpy(&txData,pll_buffer+j,4);
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_DATA, txData);
        j += 4;
    }

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_END, 0);
    return 0;
}


void GeckoReadPllVersion(uint32_t *pRegistersBaseAddress)
{
    int returnCode;
    uint16_t pllVersion = 0;

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_GET_PLL_VERSION, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &pllVersion);
    if (returnCode != 0)
    {
        //Gecko timed out
        prn(LOG_SCR | LEVEL_INFO, "Error reading PLL version\n");
        return;
    }
    prn(LOG_SCR | LEVEL_INFO, "Gecko PLL Version %d\n", pllVersion);
}

void GeckoLedTestStart(uint32_t *pRegistersBaseAddress)
{
    int returnCode;
    uint16_t retVal = 0;

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_LED_TEST_START, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &retVal);
    if (returnCode != 0)
    {
        //Gecko timed out
        prn(LOG_SCR | LEVEL_INFO, "Gecko LED test start failed\n");
        return;
    }
    prn(LOG_SCR | LEVEL_INFO, "Gecko LED test initiated\n");
}

void GeckoLedTestStop(uint32_t *pRegistersBaseAddress)
{
    int returnCode;
    uint16_t retVal = 0;

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_LED_TEST_STOP, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &retVal);
    if (returnCode != 0)
    {
        //Gecko timed out
        prn(LOG_SCR | LEVEL_INFO, "Gecko LED test stop failed\n");
        return;
    }
    prn(LOG_SCR | LEVEL_INFO, "Gecko LED test stopped\n");
}


void GeckoFailsafeButtonStatus(uint32_t *pRegistersBaseAddress)
{
    int returnCode;
    uint16_t buttonStatus = 0;

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_CHECK_FAILSAFE_BUTTON, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &buttonStatus);
    if (returnCode != 0)
    {
        //Gecko timed out
        prn(LOG_SCR | LEVEL_INFO, "Gecko failsafe button status failed\n");
        return;
    }
    switch (buttonStatus)
    {
    case 1:
        prn(LOG_SCR | LEVEL_INFO, "Gecko failsafe button released\n");
        break;
    case 2:
        prn(LOG_SCR | LEVEL_INFO, "Gecko failsafe button pressed\n");
        break;
    default:
        prn(LOG_SCR | LEVEL_INFO, "Gecko failsafe button status unknown\n");
        break;
    }
}



void GeckoPrintTime(int64_t utcTime)
{
    if (utcTime > 0x50000000)
    {
        struct tm  ts;
        char       buf[80];

        // Format time, "UTC yyyy-mm-dd hh:mm:ss"
        ts = *gmtime((const time_t *)&utcTime);
        strftime(buf, sizeof(buf), "UTC %Y-%m-%d %H:%M:%S", &ts);
        printf("%s", buf);
    }
    else
    {
        printf("Time counter %9u s", (uint32_t)utcTime);
    }
}

void GeckoPrintLog(uint8_t *logEntry)
{
    uint16_t length = 55;
    int i;
    uint16_t sensorValue;

    while ((logEntry[length] == '\0') || (logEntry[length] == 0xFF))
    {
        length--;
        if (length == 0)
        {
            return;
        }
    }

    if (length < 55)
    {
        if (logEntry[length] != '\n')
        {
            length++;
            logEntry[length] = '\n';
        }

        if (length < 55)
        {
            length++;
            logEntry[length] = '\0';
        }
    }

    if ((length > 12) &&  (logEntry[0] == 'D') && (logEntry[1] == 'P') && (logEntry[2] == ':'))
    {
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ": Dump %d entries\n", logEntry[3]);

        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "              SensorId        Value\n");

        for (i=0;i<logEntry[3];i++)
        {
            sensorValue = logEntry[(3*i+5)] << 8 | logEntry[(3*i+6)];
            prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "                0x%02X      0x%04X (%5d)\n", logEntry[(3*i+4)], sensorValue, sensorValue);
        }
    }
    else
    {
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ": %s", logEntry);
    }
}

void GeckoReadLogs(uint32_t *pRegistersBaseAddress, uint64_t noOfLogs)
{
    uint16_t logStart;
    uint16_t nextLog=0;
    int returnCode;
    int i;
    uint16_t logData[32];
    int64_t logTime;

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_READ_FLASH, GECKO_SUBCMD_FLASH_READ_GET_FIRST_LOG, noOfLogs);

    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &logStart);
    if (returnCode == 1)
    {//Gecko timed out
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Read log failed\n");
        return;
    }
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Read log start: %u\n", logStart);

    nextLog = logStart;
    while (nextLog != 0xFFFF)
    {
        memset(logData,0,sizeof(logData));
        for (i=0;i<32;i++)
        {
            usleep(100); // Allow Flash operations in the Gecko to keep up
            GeckoWrite(pRegistersBaseAddress, GECKO_CMD_READ_FLASH, GECKO_SUBCMD_FLASH_READ_LOG, nextLog + (i << 1));
            if (GeckoWaitReadyData(pRegistersBaseAddress, &logData[i]) != 0)
            {//Gecko timed out
                return;
            }
        }
        logTime = (logData[1] << 16) | logData[0];
        GeckoPrintTime(logTime);
        GeckoPrintLog((uint8_t *)&logData[4]);
//        prn(LOG_SCR|LOG_FILE|LEVEL_INFO, ": %s\n", (uint8_t *)&logData[2]);

        usleep(1000);
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_READ_FLASH, GECKO_SUBCMD_FLASH_READ_GET_NEXT_LOG, nextLog);
        if (GeckoWaitReadyData(pRegistersBaseAddress, &nextLog) != 0)
        {//Gecko timed out
            return;
        }
    }
}

void GeckoClearLogs(uint32_t *pRegistersBaseAddress)
{
    uint32_t eraseReady=0;
    uint16_t flashStatus;
    int returnCode;


    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_START_LOG_AREA, 0);

    usleep(10000);

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_ERASE_PAGES, 0x0110);
    //Must wait for erase to finish, is there a check cmd?

    while (eraseReady == 0)
    {
        usleep(100000);
        GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_FLASH_STATUS, 0);
        usleep(100000);
        returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &flashStatus);
        if (returnCode == 1)
        {//Gecko timed out
            return;
        }

        if (returnCode == 2)
        {
            eraseReady = 0;
        }
        else
        {
            if (flashStatus == 0)
            {
                eraseReady = 1;
            }
        }
        prn(LOG_SCR|LOG_FILE|LEVEL_DEBUG,"flash state: %d - Erase ready: %d\n", flashStatus, eraseReady);
    }

    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Log erased\n");

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_WRITE_FLASH, GECKO_SUBCMD_FLASH_WRITE_END, 0);
    return;
   
}


void GeckoReadTime(uint32_t *pRegistersBaseAddress)
{
    int returnCode;
    uint16_t lsbTime = 0;
    uint16_t msbTime = 0;
    int64_t myTime;

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_TIMER_GET_LSB, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &lsbTime);
    if (returnCode != 0)
    {//Gecko timed out
        prn(LOG_SCR | LEVEL_INFO, "Error reading time\n");
        return;
    }
    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_TIMER_GET_MSB, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &msbTime);
    if (returnCode != 0)
    {//Gecko timed out
        prn(LOG_SCR | LEVEL_INFO, "Error reading time\n");
        return;
    }

    myTime = msbTime << 16 | lsbTime;
    GeckoPrintTime(myTime);
    printf("\n");
}

void GeckoSetTime(uint32_t *pRegistersBaseAddress, uint64_t setTime)
{
    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_SET_TIMER_VALUE, setTime);

    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Time is set to: ");
    GeckoPrintTime((int64_t)setTime);
    printf("\n");
}

#define MEAS_12BIT             4096
#define MEAS_13BIT             8192
#define TELE_VOLTAGE_BIT       0x01
#define TELE_CURRENT_BIT       0x02
#define TELE_TEMPERATURE_BIT   0x04

void GeckoConvertSensor(uint16_t sensor, uint16_t sensorValue, char *strResult)
{
    uint32_t convValue;
    uint16_t N, Y;
    int i;

    switch (sensor)
    {
    case GECKO_SUBCMD_TELEMETRY_17V:
        sprintf(strResult, "%3.3f V", ((float)sensorValue)*1.25*16/MEAS_12BIT);
        break;
    case GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3PCI:
    case GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3:
        sprintf(strResult, "%3.3f V", ((float)sensorValue)*1.25*3.05/MEAS_12BIT);
        break;
    case GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6:
    case GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6:
        sprintf(strResult, "%3.3f V", ((float)sensorValue)*1.25*1.02/MEAS_12BIT);
        break;
    case GECKO_SUBCMD_TELEMETRY_DDR_VPP_2V5:
        sprintf(strResult, "%3.3f V", ((float)sensorValue)*1.25*2.24/MEAS_12BIT);
        break;
    case GECKO_SUBCMD_TELEMETRY_AUX_12V_SOURCE:
    case GECKO_SUBCMD_TELEMETRY_PCIE_12V_SOURCE:
        sprintf(strResult, "%3.3f V", ((float)sensorValue)*1.25*11.7/MEAS_12BIT);
        break;
    case GECKO_SUBCMD_TELEMETRY_AUX_12V_OUTPUT_CURRENT:
    case GECKO_SUBCMD_TELEMETRY_PCIE_12V_OUTPUT_CURRENT:
        sprintf(strResult, "%3.3f A", ((float)sensorValue)*1.25*5/MEAS_12BIT);
    case GECKO_SUBCMD_TELEMETRY_FPGA_I_3V3:
    case GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6_CURRENT:
    case GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6_CURRENT:
        sprintf(strResult, "%3.3f A", ((float)sensorValue)*1.25*2.86/MEAS_12BIT);
        break;

    case GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_VOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_VOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_VOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_VOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_VOUT:
        sprintf(strResult, "%3.3f V", ((float)sensorValue)/MEAS_13BIT);
        break;
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_IOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_IOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_IOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_IOUT:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_IOUT:
        N = (32-((sensorValue & 0xF800) >> 11));
        Y = (sensorValue & 0x07FF);
        convValue = Y * 1000;
        for (i=0;i<N;i++) (convValue/=2);
        sprintf(strResult, "%3.3f A", ((float)convValue)/1000);
        break;
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_TEMP:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_TEMP:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_TEMP:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_TEMP:
    case GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_TEMP:
        N = (32-((sensorValue & 0xF800) >> 11));
        Y = (sensorValue & 0x07FF);
        convValue = Y * 1000;
        for (i=0;i<N;i++) (convValue/=2);
        sprintf(strResult, "%3.3f °C", ((float)convValue)/1000);
        break;
    case GECKO_SUBCMD_TELEMETRY_FPGA_CORE_TEMP:
    case GECKO_SUBCMD_TELEMETRY_FAN_INLET_TEMP:
    case GECKO_SUBCMD_TELEMETRY_DDR4_TEMP:
        sprintf(strResult, "%2d.%03d °C", ((sensorValue >> 8) & 0xFF), (((sensorValue >> 5) & 0x07) * 125));
        break;
    default:
        break;
    }
}

void GeckoReadSensor(uint32_t *pRegistersBaseAddress, uint16_t sensor, uint16_t *sensorValue)
{
    int returnCode;

    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_READ_TELEMETRY, sensor, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, sensorValue);
    if (returnCode != 0)
    {//Gecko timed out
        prn(LOG_SCR | LEVEL_INFO, "Error reading sensor\n");
        return;
    }
}

void GeckoReadTelemetry(uint32_t *pRegistersBaseAddress, uint64_t telemetryLevel)
{
    uint16_t sensorValue=0;
    char result[20];
    uint32_t teleMask;

    switch (telemetryLevel)
    {
    case 0:
        teleMask = 0xFF;
        break;
    case 1:
        teleMask = 0x01;
        break;
    case 2:
        teleMask = 0x02;
        break;
    case 3:
        teleMask = 0x04;
        break;
    default:
        prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"Illegal Telemetry Level\n");
        return;
    }
        
if (teleMask & TELE_VOLTAGE_BIT)
{
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"========== ADC VOLTAGES ==========\n");
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_17V, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_17V, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"17V voltage: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3PCI, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3PCI, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"3V3_PCI voltage: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"3V3 voltage: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"DDR1 voltage: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"DDR2 voltage: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_DDR_VPP_2V5, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_DDR_VPP_2V5, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"DDR 2V5 voltage: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_AUX_12V_SOURCE, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_AUX_12V_SOURCE, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"AUX voltage: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_PCIE_12V_SOURCE, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_PCIE_12V_SOURCE, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"PCI voltage: %s\n", result);
}

if (teleMask & TELE_CURRENT_BIT) {
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"========== ADC CURRENTS ===========\n");

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_AUX_12V_OUTPUT_CURRENT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_AUX_12V_OUTPUT_CURRENT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"AUX 12V current: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_PCIE_12V_OUTPUT_CURRENT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_PCIE_12V_OUTPUT_CURRENT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"PCI 12V current: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_FPGA_I_3V3, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_FPGA_I_3V3, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"3V3 current: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6_CURRENT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6_CURRENT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"DDR1 current: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6_CURRENT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6_CURRENT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"DDR2 current: %s\n", result);
}

    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"======== INTERSIL DEVICES ========\n");
if (teleMask & TELE_VOLTAGE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_VOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_VOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"VCORE 2 voltage: %s\n", result);
}
if (teleMask & TELE_CURRENT_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_IOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_IOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"VCORE 2 current: %s\n", result);
}
if (teleMask & TELE_TEMPERATURE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"VCORE 2 temperature: %s\n", result);
}
if (teleMask & TELE_VOLTAGE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_VOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_VOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"1V2 voltage: %s\n", result);
}
if (teleMask & TELE_CURRENT_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_IOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_IOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"1V2 current: %s\n", result);
}
if (teleMask & TELE_TEMPERATURE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"1V2 temperature: %s\n", result);
}
if (teleMask & TELE_VOLTAGE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_VOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_VOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"0V9 voltage: %s\n", result);
}
if (teleMask & TELE_CURRENT_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_IOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_IOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"0V9 current: %s\n", result);
}
if (teleMask & TELE_TEMPERATURE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"0V9 temperature: %s\n", result);
}
if (teleMask & TELE_VOLTAGE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_VOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_VOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"1V8 voltage: %s\n", result);
}
if (teleMask & TELE_CURRENT_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_IOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_IOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"1V8 current: %s\n", result);
}
if (teleMask & TELE_TEMPERATURE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"1V8 temperature: %s\n", result);
}
if (teleMask & TELE_VOLTAGE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_VOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_VOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"VCORE 1 voltage: %s\n", result);
}
if (teleMask & TELE_CURRENT_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_IOUT, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_IOUT, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"VCORE 1 current: %s\n", result);
}
if (teleMask & TELE_TEMPERATURE_BIT) {
    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"VCORE 1 temperature: %s\n", result);
}
if (teleMask & TELE_TEMPERATURE_BIT) {
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"======== TEMPERATURES ========\n");

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_FPGA_CORE_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_FPGA_CORE_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"FPGA temperature: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_FAN_INLET_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_FAN_INLET_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"FAN Inlet temperature: %s\n", result);

    GeckoReadSensor(pRegistersBaseAddress, GECKO_SUBCMD_TELEMETRY_DDR4_TEMP, &sensorValue);
    GeckoConvertSensor(GECKO_SUBCMD_TELEMETRY_DDR4_TEMP, sensorValue, result);
    prn(LOG_SCR|LOG_FILE|LEVEL_INFO,"DDR4 temperature: %s\n", result);
}

}

//Function which reboots Main FPGA with a new image
void GeckoRebootFPGA(uint32_t *pRegistersBaseAddress, int image_no)
{
    // image_no contains time (in milliseconds) to reboot and image to reboot
       prn(LOG_SCR | LEVEL_INFO, "Gecko Reboot FPGA: %d\n", image_no);
    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_REBOOT_FPGA, image_no);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Main FPGA is rebooted. Remember to reboot PC!\n");
}


/*
 * Return Gecko revision
 */
uint32_t GeckoRevision(uint32_t *pRegistersBaseAddress)
{
    int returnCode;
    uint16_t lshData = 0;
    uint16_t mshData = 0;
    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_GET_REVISION_LSH, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &lshData);
    if (returnCode != 0)
    {//Gecko timed out
        //prn(LOG_SCR | LEVEL_INFO, "Error reading revision\n");
        //return 1;
    }
    GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_GET_REVISION_MSH, 0);
    returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &mshData);
    if (returnCode != 0)
    {//Gecko timed out
        //prn(LOG_SCR | LEVEL_INFO, "Error reading revision\n");
        //return 1;
    }
    return mshData << 16 | lshData;
}

/*
 * Parse Gecko input cmds
 */
int cmdGecko(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t *pRegistersBaseAddress = pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
    int argp = 0;//Parse counter
    char *filename = "savona.hex";
    uint64_t slotId;
    uint64_t noOfLogs;
    uint64_t telemetryLevel;
    uint64_t setTime;
    int returnCode;

    if( (argc == 2) && (strcmp("flashImage", argv[0]) == 0) ) {
        filename = argv[1];
        prn(LOG_SCR | LEVEL_INFO, "Gecko SW flashImage: file: %s\n", filename);
        GeckoWriteImage2Flash(pRegistersBaseAddress, 0, filename);
    } else if( (argc == 2) && (strcmp("readLog", argv[0]) == 0) ) {
        noOfLogs = strtoull(argv[1], NULL, 0);
        prn(LOG_SCR | LEVEL_INFO, "Gecko Read logs: Number: %lu\n", noOfLogs);
        GeckoReadLogs(pRegistersBaseAddress, noOfLogs);
    } else if( (argc == 2) && (strcmp("telemetry", argv[0]) == 0) ) {
        telemetryLevel = strtoull(argv[1], NULL, 0);
        prn(LOG_SCR | LEVEL_INFO, "Gecko Read Telemetry: Level: %lu\n", telemetryLevel);
        GeckoReadTelemetry(pRegistersBaseAddress, telemetryLevel);
    } else if( (argc == 1) && (strcmp("clearLog", argv[0]) == 0) ) {
        prn(LOG_SCR | LEVEL_INFO, "Gecko Clear logs\n");
        GeckoClearLogs(pRegistersBaseAddress);
    } else if( (argc == 1) && (strcmp("ledTestStart", argv[0]) == 0) ) {
        prn(LOG_SCR | LEVEL_INFO, "Gecko LED test start\n");
        GeckoLedTestStart(pRegistersBaseAddress);
    } else if( (argc == 1) && (strcmp("ledTestStop", argv[0]) == 0) ) {
        prn(LOG_SCR | LEVEL_INFO, "Gecko LED test stop\n");
        GeckoLedTestStop(pRegistersBaseAddress);
    } else if( (argc == 1) && (strcmp("failsafeButtonStatus", argv[0]) == 0) ) {
        prn(LOG_SCR | LEVEL_INFO, "Gecko Read Failsafe Button Status\n");
        GeckoFailsafeButtonStatus(pRegistersBaseAddress);
    } else if( (argc == 1) && (strcmp("readTime", argv[0]) == 0) ) {
        prn(LOG_SCR | LEVEL_INFO, "Read Gecko Internal Time\n");
        GeckoReadTime(pRegistersBaseAddress);
    } else if( (argc == 1) && (strcmp("pllVersion", argv[0]) == 0) ) {
        GeckoReadPllVersion(pRegistersBaseAddress);
    } else if( (argc == 2) && (strcmp("setTime", argv[0]) == 0) ) {
        if (strcmp("UTC", argv[1]) == 0) {
            setTime = time(NULL);
        } else {
            setTime = strtoull(argv[1], NULL, 0);
        }
        prn(LOG_SCR | LEVEL_INFO, "Set Gecko Internal Time (seconds): %lu\n", setTime);
        GeckoSetTime(pRegistersBaseAddress, setTime);
    } else if( (argc == 3) && (strcmp("extendedFlashImage", argv[0]) == 0) ) {
        slotId = strtoull(argv[1], NULL, 0);
        filename = argv[2];
        prn(LOG_SCR | LEVEL_INFO, "Gecko SW flashImage: slotId %lu, file: %s\n", slotId, filename);
        GeckoWriteImage2Flash(pRegistersBaseAddress, slotId, filename);
    } else if( (argc == 2) && (strcmp("flashPll", argv[0]) == 0) ) {
        filename = argv[1];
        prn(LOG_SCR | LEVEL_INFO, "Gecko SW flash PLL Image: file: %s\n", filename);
        GeckoWritePll2Flash(pRegistersBaseAddress, filename);
    } else if( (argc == 1) && (strcmp("bootImage", argv[0]) == 0) ) {
           prn(LOG_SCR | LEVEL_INFO, "Booting image from FLASH, please wait 5 seconds and reboot\n");
            GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_FLASH_BOOT_IMAGE_1, 0);
    } else if( (argc == 2) && (strcmp("extendedBootImage", argv[0]) == 0) ) {
        slotId = strtoull(argv[1], NULL, 0);
        switch (slotId)
        {
        case 0:
           prn(LOG_SCR | LEVEL_INFO, "Booting image from FLASH (slot 0), please wait 5 seconds and reboot\n");
            GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_FLASH_BOOT_IMAGE_1, 0);
            break;
        case 1:
           prn(LOG_SCR | LEVEL_INFO, "Booting image from FLASH (slot 1), please wait 5 seconds and reboot\n");
            GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_FLASH_BOOT_IMAGE_2, 0);
            break;
        default:
           prn(LOG_SCR | LEVEL_INFO, "Illegal slotId %lu (should be 0 or 1)\n", slotId);
            break;
        }
    } else {
        while (argc > argp) {
            if (strcmp("revision", argv[argp]) == 0) {
                uint16_t lshData = 0;
                uint16_t mshData = 0;
                GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_GET_REVISION_LSH, 0);
                returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &lshData);
                if (returnCode != 0)
                {//Gecko timed out
                    prn(LOG_SCR | LEVEL_INFO, "Error reading revision\n");
                    return 1;
                }
                GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_CTRL_GET_REVISION_MSH, 0);
                returnCode = GeckoWaitReadyData(pRegistersBaseAddress, &mshData);
                if (returnCode != 0)
                {//Gecko timed out
                    prn(LOG_SCR | LEVEL_INFO, "Error reading revision\n");
                    return 1;
                }
               prn(LOG_SCR | LEVEL_INFO, "Gecko SW Revision %d.%d.%d.%d\n", ((mshData >> 8) & 0xFF), ((mshData) & 0xFF), ((lshData >> 8) & 0xFF), ((lshData) & 0xFF));
            } else if (strcmp("bootImage", argv[argp]) == 0) {
                GeckoWrite(pRegistersBaseAddress, GECKO_CMD_CONTROL, GECKO_SUBCMD_FLASH_BOOT_IMAGE_1, 0);

            }
            else if (strcmp("test-flash-read", argv[argp]) == 0)
            {
                prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Test Gecko flash read\n");
                GeckoReadTest(pRegistersBaseAddress);
            }
            else if (strcmp("-h", argv[argp]) == 0) {
                prn(LOG_SCR | LEVEL_INFO, "Valid commands:\n"
                    "revision                       : Read Gecko SW revision\n"
                    "flashImage <filename>          : Flash an embedded SW image\n"
                    "flashPll <filename>            : Flash a PLL image\n"
                    "pllVersion                     : Read PLL version\n"
                    "bootImage                      : Boot the new image\n"
                    "readLog <entries>              : Get the last log entries, 0 means all \n"
                    "clearLog                       : Will erase all logs, use with care\n"
                    "readTime                       : Reads the time in the Gecko (seconds)\n"
                    "setTime <seconds>              : Reads the time in the Gecko (seconds)\n"
                    "telemetry <level>              : Reads the telemetry data (only level 0 active)\n"
                    "");
                return 0;
            } else {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Invalid argument: %s\nUse -h for help\n", argv[argp]);
                return -1;
            }
            argp++;
        }
    }
    return 0;
}

/*
 * qspi.c
 *
 *  Created on: 17 Aug, 2018
 *      Author: mm
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>

#include "printlog.h"
#include "fpgaregs.h"
#include "qspi.h"

static int _QSPIFlashGetFlagStatus(const FL_FlashInfo * pFlashInfo, bool output);

uint32_t axird(const FL_FlashInfo * pFlashInfo, uint64_t axireg)
{
#if 1
    uint32_t data;
    FL_ExitOnError(pFlashInfo->pLogContext, FL_QSPI_AXI_Read(pFlashInfo, axireg, &data));
    return data;
#else
    uint32_t *baseAddr = pFlashInfo->pRegistersBaseAddress;
    uint64_t rd = 0;
    time_t startTime = time(NULL);

    wr64(baseAddr, AXIREG, 0x8000000000000000L + (axireg<<32));
    rd = rd64(baseAddr, AXIREG);
    //printf("RD DEBUG 0x%llx\n",rd);

//    return 0xffffffff&rd64(baseAddr,AXIREG);

    while((rd>>63) == 0){
        rd = rd64(baseAddr, AXIREG);
        //printf("RD DEBUG 0x%llx\n",rd);
        if (time(NULL) - startTime > 10)
        {
            fprintf(stderr, "\n *** ERROR: %s took over 10 seconds\n", __func__);
            exit(EXIT_FAILURE);
        }
    }
    //printf("DEBUG 0x%llx\n",rd64(baseAddr,REG));
    return 0xffffffff&rd64(baseAddr, AXIREG);
#endif
}

int axiwr(const FL_FlashInfo * pFlashInfo,uint64_t axireg, uint64_t data)
{
#if 1
    FL_Error errorCode;
    FL_ExitOnError(pFlashInfo->pLogContext, errorCode = FL_QSPI_AXI_Write(pFlashInfo, axireg, (uint32_t)data));
    return errorCode;
#else
    uint32_t *baseAddr = pFlashInfo->pRegistersBaseAddress;
    uint64_t rd = 0;
    time_t startTime = time(NULL);

    wr64(baseAddr, AXIREG, (axireg<<32) + data);

    //rd = rd64(baseAddr,AXIREG);
  //  for(rd=0;rd<1000;rd++);
    //return 0;
    rd = rd64(baseAddr, AXIREG);
    while((rd>>63) == 0){
        rd = rd64(baseAddr, AXIREG);
        //printf("WR DEBUG 0x%llx\n",rd);
        if (time(NULL) - startTime > 10)
        {
            fprintf(stderr, "\n *** ERROR: %s took over 10 seconds\n", __func__);
            exit(EXIT_FAILURE);
        }
    }
    //printf("DEBUG 0x%llx\n",rd64(baseAddr,REG));
    return 0;
#endif
}


int QSPI_Transfer(const FL_FlashInfo * pFlashInfo, uint8_t *rd_buf,uint16_t rd_size, const uint8_t *wr_buf, uint16_t wr_size)
{
#if 1
    return FL_QSPI_Transfer(pFlashInfo, rd_buf, rd_size, wr_buf, wr_size);
#else
    uint32_t cr; //control register
    uint32_t i;
    uint32_t empty;

    // Read and store contents of control reg
    cr = axird(pFlashInfo,SPICR);
    // reset fifos.
    cr |= XSP_CR_TXFIFO_RESET_MASK | XSP_CR_RXFIFO_RESET_MASK | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK ;
    //write to control reg
    axiwr(pFlashInfo,SPICR,cr);

    // Send CMD + dummies
    //write bytes in wr_buf to fpga fifo
    for(i=0;i<wr_size;i++){
        axiwr(pFlashInfo,SPIDTR,wr_buf[i]);
    }

    // select slave. pull SS low.
    axiwr(pFlashInfo, SPISSR, 0);

    // setup controller to start transmit on the physical QSPI lines.
    cr = axird(pFlashInfo, SPICR);
    cr &= ~XSP_CR_TRANS_INHIBIT_MASK;
    axiwr(pFlashInfo, SPICR, cr);

    //read tx&rx fifo empty bits
    empty = FIFO_empty(pFlashInfo);

    // wait for the tx fifo to be empty
    while ((empty&(XSP_SR_TX_EMPTY_MASK | XSP_SR_RX_EMPTY_MASK)) == 0) {
        empty = FIFO_empty(pFlashInfo);
    }

    if(rd_size>0){
        if (wr_size > rd_size)
        for(i=0;i<wr_size-rd_size;i++){
            axird(pFlashInfo,SPIDRR);
        }
        for(i=0;i < rd_size;i++){
            rd_buf[i] = axird(pFlashInfo,SPIDRR);
        }
    }

    // release SPI bus
    cr = axird(pFlashInfo,SPICR);
    cr |= XSP_CR_TRANS_INHIBIT_MASK;
    axiwr(pFlashInfo,SPICR,cr);
    axiwr(pFlashInfo,SPISSR,1);

    return 0;
#endif
}

int FIFO_empty(const FL_FlashInfo * pFlashInfo)//, uint8_t rdwr)
{
    return (axird(pFlashInfo,SPISR)& 0x5); // RxFIFO empty bit 0; TxFIFO empty bit 2;
}

int FIFO_fill(const FL_FlashInfo * pFlashInfo,uint8_t txrx)
{
    return (axird(pFlashInfo,SPITFIFOFILL + (txrx<<2) )& 0xff); // TXFIFO fill @ 0x74 , RXFIFO fill @ 0x78;
}

int QSPIFlashReadID(const FL_FlashInfo * pFlashInfo)
{
	uint8_t *rd_buf = calloc(3, sizeof(uint8_t));
	uint8_t *wr_buf = calloc(1, sizeof(uint8_t));

	wr_buf[0] = COMMAND_DEV_READ_ID;

	// write 4 bytes. CMD + dummys for 3 expected bytes (ID,Type,Capacity)
	QSPI_Transfer(pFlashInfo,rd_buf,3,wr_buf,1+3);
	printf("0x%02x 0x%02x 0x%02x\n",rd_buf[2],rd_buf[1],rd_buf[0]);
	if ( (rd_buf[0] == 0x20)){
			printf("\n\rManufacturer ID:\t0x%x\t:= MICRON\n\r", rd_buf[0]);
			if ( (rd_buf[1] == 0xBA))
			{
				printf("Memory Type:\t\t0x%x\t:= N25Q 3V0\n\r", rd_buf[1]);
			}
			else
			{
				if ((rd_buf[1] == 0xBB))
				{
					printf("Memory Type:\t\t0x%x\t:= N25Q 1V8\n\r", rd_buf[1]);
				} else printf("Memory Type:\t\t0x%x\t:= QSPI Data\n\r", rd_buf[1]);
			}
			if ((rd_buf[2] == 0x18))
			{
				printf("Memory Capacity:\t0x%x\t:=\t128Mbit\n\r", rd_buf[2]);
			}
			else if ( (rd_buf[2] == 0x19))
			{
				printf("Memory Capacity:\t0x%x\t:= 256Mbit\n\r", rd_buf[2]);
			}
			else if ((rd_buf[2] == 0x20))
			{
				printf("Memory Capacity:\t0x%x\t:= 512Mbit\n\r", rd_buf[2]);
			}
			else if ((rd_buf[2] == 0x21))
			{
				printf("Memory Capacity:\t0x%x\t:= 1024Mbit\n\r", rd_buf[2]);
			}
	}

	free(rd_buf);
	free(wr_buf);
	return 0;

}

static int SIZE = 32;

int QSPI_ReadPage(const FL_FlashInfo * pFlashInfo, uint32_t Addr, const char * fileName)
{
    uint32_t *baseAddr = pFlashInfo->pRegistersBaseAddress;
    int Status = 0;
    int i,j;
    //int const flash_size=256  ;//00000;//0x1fff0;// 256;//31072;//*4;//262144;
    //int const flash_size = 64 * 1024 * 1024; // 64 MB in chunks of 256
    int const flash_size = SIZE * 1024 * 1024; // SIZE MB in chunks of 256

    uint8_t *file_buf = calloc(flash_size + 100, sizeof(uint8_t));
    FILE *f;

    uint16_t wr_size = 256;
    uint16_t rd_size = 128;

    uint8_t *wr_buf = calloc(wr_size+10, sizeof(uint8_t));
    uint8_t *rd_buf = calloc(rd_size+10, sizeof(uint8_t));

    //return 0;
    uint64_t previousPercentage = ~0;
    for(i=Addr;i<flash_size;i=i+256)
    {
        //i=Addr;//<<7;  ;
        //printf("i=%d\n",i);
        //SpiFlash4bytemodeEnable(baseAddr);
        wr_buf[0] = 0x6b    ;//ReadCmd;
        //BYTE4
        //wr_buf[1] = (uint8_t) (i >> 24);
        wr_buf[1] = (uint8_t) (i >> 16);
        wr_buf[2] = (uint8_t) (i >> 8);
        wr_buf[3] = (uint8_t) i;
//        Status = QSPI_Transfer_reg/**/(baseAddr,rd_buf,240,wr_buf,5/*writebytes*/ + 4 /*dummy cycles*/);
        wr64(baseAddr,AXIREG,CS_TRIG);
        Status = QSPI_Transfer(pFlashInfo,rd_buf,rd_size,wr_buf,rd_size+4/*writebytes*/ + 4 /*dummy cycles*/);
        Status = _QSPIFlashGetFlagStatus(pFlashInfo, false);
        for(j=0;j<rd_size;j++)
        {
            file_buf[i + j] = rd_buf[j];
            //if (j%16==0)
            //    printf("\n");
            //printf("%02x",rd_buf[j]);

        }

        uint64_t percentage = 100ULL * (uint64_t)(i + 256);
        percentage /= flash_size;
        if (percentage != previousPercentage)
        {
            fprintf(stdout, "Reading flash %u%%, %d (0x%X) bytes   \r", (uint32_t) percentage, i + 256, i + 256);
            fflush(stdout);
            previousPercentage = percentage;
        }
    }

//    return 0;
    f = fopen(fileName, "wb");

    //for(i=0;i<flash_size;i++)
    {
        printf("\nWriting to file '%s'\n", fileName);
        fwrite(file_buf,1, flash_size, f);
    }

    free(wr_buf);
    free(rd_buf);
    free(file_buf);

    fclose(f);

    return Status;
}


int QSPI_ReadFlash(const FL_FlashInfo * pFlashInfo)
{
    int Status;
    int i,j;
    //int const flash_size=256  ;//00000;//0x1fff0;// 256;//31072;//*4;//262144;
    //int const flash_size = 64 * 1024 * 1024; // 64 MB in chunks of 256
    int const flash_size = 256;//SIZE * 1024 * 1024; // 32 MB in chunks of 256

    uint8_t *file_buf = calloc(flash_size, sizeof(uint8_t));
    FILE *f;

    uint16_t rd_size = 16;
    uint16_t wr_size = rd_size + 5 + 4;

    uint8_t *wr_buf = calloc(wr_size, sizeof(uint8_t));
    uint8_t *rd_buf = calloc(rd_size, sizeof(uint8_t));

    //return 0;
    //uint64_t previousPercentage = ~0;

    for(i=0;i<flash_size;i=i+rd_size){
        //SpiFlash4bytemodeEnable(baseAddr);
        wr_buf[0] = 0x6C;//ReadCmd;
        //BYTE4


        printf("i=%d ,",i);

        //wr_buf[1] = (uint8_t) (i >> 24);
        wr_buf[1] = (uint8_t) (i >> 16);
        wr_buf[2] = (uint8_t) (i >> 8);
        wr_buf[3] = (uint8_t) i;

      //  wr64(baseAddr,AXIREG,CS_TRIG);
        Status = QSPI_Transfer(pFlashInfo,rd_buf,rd_size,wr_buf,rd_size+4/*writebytes*/ + 4 /*dummy cycles*/);
        QSPIFlashGetFlagStatus(pFlashInfo);
        for(j=0;j<rd_size;j++)
        {
            file_buf[i + j] = rd_buf[j];
            //printf("ji=%d ,",j+i);
        }
    }
    const char * fileName = "flash_read.bin";
    f = fopen(fileName, "wb");

    //for(i=0;i<flash_size;i++)
    {
        printf("\nWriting to file '%s'\n", fileName);
        fwrite(file_buf,1, flash_size, f);
    }

    free(wr_buf);
    free(rd_buf);
    free(file_buf);

    fclose(f);

    return Status;
}

int QSPI_ProgramPage(const FL_FlashInfo * pFlashInfo, uint32_t Addr)
{
    int Status;
    int i, j;
    //int const flash_size=256*256  ;//00000;//0x1fff0;// 256;//31072;//*4;//262144;
    //int const flash_size = 512;

    uint16_t wr_size = 128;
    uint16_t rd_size = 256;

    uint8_t *wr_buf = calloc(wr_size+10, sizeof(uint8_t));
    uint8_t *rd_buf = calloc(rd_size+10, sizeof(uint8_t));


    //for(i=256;i<flash_size;i=i+256){


        //SpiFlash4bytemodeEnable(baseAddr);
        //printf("wren Read status register = 0x%02x \n",QSPI_ReadStatusReg(baseAddr));
        QSPIFlashWriteEnable(pFlashInfo);
        //    QSPIFlashGetFlagStatus(baseAddr);


      //  printf("wren Read status register = 0x%02x \n",QSPI_ReadStatusReg(baseAddr));

        wr_buf[0] = 0x32;//0x12;//COMMAND_PAGE_PROGRAM;//ReadCmd;
        //BYTE4
        i = Addr;
        printf("i=%d\n",i);
        wr_buf[1] = (uint8_t) (i >> 16);
        wr_buf[2] = (uint8_t) (i >> 8);
        wr_buf[3] = (uint8_t) (i >> 0);

        printf("wr_buf[123]=%02x%02x%02x\n",wr_buf[1],wr_buf[2],wr_buf[3]);

        //wr_buf[4] = (uint8_t) Addr;
//
//        wr_buf[1] = (uint8_t)(Addr >> 16);
//        wr_buf[2] = (uint8_t)(Addr >> 8);
//        wr_buf[3] = (uint8_t)Addr;
        for(j=0;j<wr_size;j++){//j+4){
             wr_buf[4+j]= (uint8_t) 256-j;
         }

       // QSPIFlashGetFlagStatus(baseAddr);
       // wr64(baseAddr,AXIREG,CS_TRIG);
//        Status = QSPI_Transfer_program(baseAddr,wr_buf,256+5/*250 writebytes*/);
        Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,wr_size+4/*250 writebytes*/);
      //  wr64(baseAddr,AXIREG,CS_TRIG);
//        while(QSPI_FlashBusy(pFlashInfo));
        QSPIFlashGetFlagStatus(pFlashInfo);
        QSPIFlashGetFlagStatus(pFlashInfo);
        QSPIFlashGetFlagStatus(pFlashInfo);
        QSPIFlashGetFlagStatus(pFlashInfo);
        QSPIFlashGetFlagStatus(pFlashInfo);
      //}

//    return 0;

    free(wr_buf);
    free(rd_buf);


    return Status;
}

int QSPI_Program(const FL_FlashInfo * pFlashInfo)
{
    int Status;
    int i, j;
    //int const flash_size=256*256  ;//00000;//0x1fff0;// 256;//31072;//*4;//262144;
    int const flash_size = 512;

    uint16_t wr_size = 128;
    //uint16_t rd_size = 0;
    //int Addr=0;
    uint8_t *wr_buf = calloc(wr_size+10, sizeof(uint8_t));


    for(i=0;i<flash_size;i=i+wr_size){


        //wren is cleared each time SS is pulled high,so set it for each transaction
        QSPIFlashWriteEnable(pFlashInfo);
        //Addr=i;
        wr_buf[0] = 0x32;//0x12;//COMMAND_PAGE_PROGRAM;//ReadCmd;
        //BYTE4
        printf("wr_buf[123]=%02x%02x%02x\n",wr_buf[1],wr_buf[2],wr_buf[3]);

        wr_buf[1] = (uint8_t) (i >> 16);
        wr_buf[2] = (uint8_t) (i >> 8);
        wr_buf[3] = (uint8_t) (i >> 0);

        for(j=0;j<wr_size;j++){
            wr_buf[4+j]= (uint8_t) i>>8;
//            printf("j+i=%d, ",(uint8_t) j+i);
        }

        Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,wr_size+4/*250 writebytes*/);
//        while(QSPI_FlashBusy(baseAddr));
        QSPIFlashGetFlagStatus(pFlashInfo);
    }
    free(wr_buf);


    return Status;
}

int QSPI_ProgramFile(const FL_FlashInfo * pFlashInfo, const char * fileName)
{
    uint32_t *baseAddr = pFlashInfo->pRegistersBaseAddress;
    int Status = 0;
    int j;
    //int const flash_size = 256 * 256;//00000;//0x1fff0;// 256;//31072;//*4;//262144;
    //uint32_t Addr = 0;
    uint32_t start = 0;// 16 * 1024 * 1024;
    uint32_t Addr = start;
    FILE * f;

    f = fopen(fileName, "rb");
    if (f == NULL)
    {
        fprintf(stderr, " *** ERROR: failed to open file '%s', errno %d\n", fileName, errno);
        return 1;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fprintf(stderr, " *** ERROR: failed to seek to end of file '%s', errno %d\n", fileName, errno);
        fclose(f);
        return 1;
    }
    long fileSize = ftell(f);
    if (fileSize == -1)
    {
        fprintf(stderr, " *** ERROR: failed to tell file '%s' size, errno %d\n", fileName, errno);
        fclose(f);
        return 1;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, " *** ERROR: failed to seek to start of file '%s', errno %d\n", fileName, errno);
        fclose(f);
        return 1;
    }
    fileSize -= ftell(f);

    //printf("\nFile '%s' length is %ld bytes\n", fileName, fileSize);

    uint64_t previousPercentage = ~0;
    //uint8_t previousSegment = 0xFF;

    uint8_t wr_buf[128 + 10];
    memset(wr_buf, 0, sizeof(wr_buf));

    while (1)
    {
        uint8_t fileReadBuffer[128];

        size_t numberOfBytesRead = fread(fileReadBuffer, 1, sizeof(fileReadBuffer), f);

        if (ferror(f) != 0)
        {
            fprintf(stderr, " *** ERROR: fread error file %s errno %d\n", fileName, errno);
            Status = 3;
            break;
        }
        //printf("i=%d\n",i);

        while (QSPI_FlashBusy(pFlashInfo));

        QSPIFlashWriteEnable(pFlashInfo);
        _QSPIFlashGetFlagStatus(pFlashInfo, false);

        Status = QSPI_ReadStatusReg(pFlashInfo);

        //        printf("Wrote %u (0x%X) bytes   \n", Addr, Addr);


        // 3-byte address mode:

        uint8_t segment = (uint8_t)(Addr / (16 * 1024 * 1024));
        QSPIReadRegWrite(pFlashInfo,COMMAND_EXT_REG_WRITE,segment);

        QSPIFlashWriteEnable(pFlashInfo);
        //printf("Read status register = 0x%02x \n",QSPI_ReadStatusReg(baseAddr));

        wr_buf[0] = COMMAND_QUAD_WRITE;
        wr_buf[1] = (uint8_t)(Addr >> 16);
        wr_buf[2] = (uint8_t)(Addr >> 8);
        wr_buf[3] = (uint8_t)Addr;

        for (j = 0; j < numberOfBytesRead; j++) {
            wr_buf[4 + j] = (uint8_t)fileReadBuffer[j];
        }

        while (QSPI_FlashBusy(pFlashInfo));
        //printf("numberofreadbytes 0x%02x\n", numberOfBytesRead);

        wr64(baseAddr,AXIREG,CS_TRIG);
        Status = QSPI_Transfer(pFlashInfo, NULL, 0, wr_buf, 4 + numberOfBytesRead/*writebytes*/);
        Status = QSPI_ReadStatusReg(pFlashInfo);
        //         printf("there: Read status register = 0x%02x\n", Status);

        while (QSPI_FlashBusy(pFlashInfo));

        Status = QSPI_ReadStatusReg(pFlashInfo);
        //printf("there: Read status register = 0x%02x\n", Status);

        Addr += numberOfBytesRead;

        int eof = feof(f);

        uint64_t percentage = 100ULL * (Addr - start);
        percentage /= fileSize;
        if (percentage != previousPercentage || eof != 0)
        {
            fprintf(stdout, "\rWriting to flash %.0f%%, %u (0x%X) of %ld bytes   ", (double)percentage, Addr - start, Addr - start, fileSize);
            fflush(stdout);
            previousPercentage = percentage;
        }

        if (eof != 0)
        {
            //printf("\nEOF!\n");
            break;
        }

#if 0
        if (Addr >= start + SIZE * 1024 * 1024)
        {
            printf("\nAddr >= start + SIZE : %d >= %d + %d\n", Addr, start, SIZE * 1024 * 1024);
            break;
        }
#endif
    }

    fclose(f);

    printf("\n");

    while (QSPI_FlashBusy(pFlashInfo));

    return Status;
}


int QSPI_VerifyFile(const FL_FlashInfo * pFlashInfo, uint32_t Addr, const char * fileName)
{
    int Status = 0;
    size_t j;
    FILE * f;

    errno = 0;

    f = fopen(fileName, "rb");
    if (f == NULL)
    {
        fprintf(stderr, " *** ERROR: failed to open file '%s', errno %d\n", fileName, errno);
        return 1;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fprintf(stderr, " *** ERROR: failed to seek to end of file '%s', errno %d\n", fileName, errno);
        fclose(f);
        return 1;
    }
    long fileSize = ftell(f);
    if (fileSize == -1)
    {
        fprintf(stderr, " *** ERROR: failed to tell file '%s' size, errno %d\n", fileName, errno);
        fclose(f);
        return 1;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, " *** ERROR: failed to seek to start of file '%s', errno %d\n", fileName, errno);
        fclose(f);
        return 1;
    }
    fileSize -= ftell(f);

    //printf("\nFile '%s' length is %ld bytes\n", fileName, fileSize);

    uint64_t previousPercentage = ~0;
    //uint8_t previousSegment = 0xFF;

    uint8_t rd_buf[128];
    uint8_t wr_buf[128 + 4/*writebytes*/ + 4 /*dummy cycles*/];
    memset(wr_buf, 0, sizeof(wr_buf));
    wr_buf[0] = COMMAND_QUAD_READ;

    while (1)
    {
        uint8_t fileReadBuffer[128];

        size_t numberOfBytesRead = fread(fileReadBuffer, 1, sizeof(fileReadBuffer), f);

        if (ferror(f) != 0)
        {
            fprintf(stderr, " *** ERROR: fread error file %s errno %d\n", fileName, errno);
            Status = 3;
            break;
        }
        //printf("i=%d\n",i);

        while (QSPI_FlashBusy(pFlashInfo));

        // 3-byte address mode:

        uint8_t segment = (uint8_t)(Addr / (16 * 1024 * 1024));
        QSPIReadRegWrite(pFlashInfo,0xc5,segment);

        wr_buf[1] = (uint8_t)(Addr >> 16);
        wr_buf[2] = (uint8_t)(Addr >> 8);
        wr_buf[3] = (uint8_t)Addr;

        while (QSPI_FlashBusy(pFlashInfo));

        Status = QSPI_Transfer(pFlashInfo, rd_buf,sizeof(rd_buf), wr_buf, sizeof(wr_buf));

        for(j = 0; j < numberOfBytesRead; ++j)
        {
            if (rd_buf[j] != (uint8_t)fileReadBuffer[j])
            {
                errno = -1;
                printf("\nnumberOfBytesRead=%ld Addr=0x%X Flash data = 0x%02x, File data = 0x%02x\n", numberOfBytesRead, Addr, rd_buf[j], fileReadBuffer[j]);
                printf("flash:\n");
                FL_HexDump(printf, rd_buf, numberOfBytesRead, 16, Addr - (ssize_t)rd_buf);
                printf("file:\n");
                FL_HexDump(printf, fileReadBuffer, numberOfBytesRead, 16, Addr - (ssize_t)fileReadBuffer);
                goto exit;
            }
        }

        Status = QSPI_ReadStatusReg(pFlashInfo);

        while (QSPI_FlashBusy(pFlashInfo));

        Status = QSPI_ReadStatusReg(pFlashInfo);
        //printf("there: Read status register = 0x%02x\n", Status);

        Addr += numberOfBytesRead;

        int eof = feof(f);

        uint64_t percentage = 100ULL * Addr;
        percentage /= fileSize;
        if (percentage != previousPercentage || eof != 0)
        {
            fprintf(stdout, "\rVerifying flash %.0f%%, %u (0x%X) of %ld bytes   ", (double)percentage, Addr, Addr, fileSize);
            fflush(stdout);
            previousPercentage = percentage;
        }

        if (eof != 0)
        {
            break;
        }

        //if (Addr >= 1024) break;
    }

exit:

    printf("\n");

    fclose(f);

    while (QSPI_FlashBusy(pFlashInfo));

    return Status;
}

int QSPI_ReadFlashToFile(const FL_FlashInfo * pFlashInfo, uint32_t Addr, const char * fileName)
{
    int Status = 0;
    int j;
    int const flash_size = SIZE * 1024 * 1024;
    FILE * f;
    //Addr = 16 * 1024 * 1024;

    uint8_t * fileBuffer = calloc(flash_size + 500, sizeof(uint8_t));
    memset(fileBuffer, 0xFF, flash_size + 500);

    uint64_t previousPercentage = ~0;
    //uint8_t previousSegment = 0xFF;

    uint8_t rd_buf[128];
    uint8_t wr_buf[128 + 4/*writebytes*/ + 4 /*dummy cycles*/];
    memset(wr_buf, 0, sizeof(wr_buf));
    wr_buf[0] = COMMAND_QUAD_READ;

    while (1)
    {
        while (QSPI_FlashBusy(pFlashInfo));

        // 3-byte address mode:

        uint8_t segment = (uint8_t)(Addr / (16 * 1024 * 1024));
        QSPIReadRegWrite(pFlashInfo, 0xc5, segment);

        wr_buf[1] = (uint8_t)(Addr >> 16);
        wr_buf[2] = (uint8_t)(Addr >> 8);
        wr_buf[3] = (uint8_t)Addr;

        while (QSPI_FlashBusy(pFlashInfo));
        //printf("numberofreadbytes 0x%02x\n", numberOfBytesRead);

//        wr64(baseAddr,AXIREG,CS_TRIG);
        Status = QSPI_Transfer(pFlashInfo, rd_buf, sizeof(rd_buf), wr_buf, sizeof(wr_buf));


        for (j = 0; j < sizeof(rd_buf); j++)
        {
            fileBuffer[Addr + j] = rd_buf[j];
        }

        Status = QSPI_ReadStatusReg(pFlashInfo);
        //         printf("there: Read status register = 0x%02x\n", Status);

        while (QSPI_FlashBusy(pFlashInfo));

        Status = QSPI_ReadStatusReg(pFlashInfo);
        //printf("there: Read status register = 0x%02x\n", Status);

        Addr += sizeof(rd_buf);

        uint64_t percentage = 100ULL * Addr;
        percentage /= flash_size;
        if (percentage != previousPercentage)
        {
            fprintf(stdout, "\rReading flash %.0f%%, %u (0x%X) of %d bytes   ", (double)percentage, Addr, Addr, flash_size);
            fflush(stdout);
            previousPercentage = percentage;
        }

        //usleep(1000);

        if (Addr >= flash_size) break;
    }

    printf("\n");

    f = fopen(fileName, "wb");
    if (f == NULL)
    {
        fprintf(stderr, " *** ERROR: failed to open file %s, errno %d\n", fileName, errno);
        exit(EXIT_FAILURE);
    }

    size_t numberOfItemsWritten = fwrite(fileBuffer, 1, flash_size, f);
    if (numberOfItemsWritten != flash_size)
    {
        fprintf(stderr, "*** ERROR: failed to write %u bytes to file %s, errno %d\n", flash_size, fileName, errno);
    }


    if (fclose(f) != 0)
    {
        fprintf(stderr, "*** ERROR: failed to close file %s, errno %d\n", fileName, errno);
    }

    free(fileBuffer);

    while (QSPI_FlashBusy(pFlashInfo));

    return Status;
}


int QSPI_EraseSector(const FL_FlashInfo * pFlashInfo, uint32_t Addr)
{
    int Status;
    int i;
    //int const flash_size=256  ;//00000;//0x1fff0;// 256;//31072;//*4;//262144;

    uint16_t wr_size = 256;
    uint16_t rd_size = 256;

    uint8_t *wr_buf = calloc(wr_size+10, sizeof(uint8_t));
    uint8_t *rd_buf = calloc(rd_size+10, sizeof(uint8_t));


    i=Addr;
    //for(i=0;i<flash_size;i=i+256){
    //printf("i=%d\n",i);
    SpiFlash4bytemodeEnable(pFlashInfo);
    printf("Read status register = 0x%02x \n",QSPI_ReadStatusReg(pFlashInfo));
    QSPIFlashGetFlagStatus(pFlashInfo);

    QSPIFlashWriteEnable(pFlashInfo);
    QSPIFlashGetFlagStatus(pFlashInfo);
    printf("Read status register = 0x%02x \n",QSPI_ReadStatusReg(pFlashInfo));

    wr_buf[0] = COMMAND_SECTOR_ERASE;//ReadCmd;
    //BYTE4
    wr_buf[1] = (uint8_t) (i >> 24);
    wr_buf[2] = (uint8_t) (i >> 16);
    wr_buf[3] = (uint8_t) (i >> 8);
    wr_buf[4] = (uint8_t) i;



    Status = QSPI_Transfer/*_write*/(pFlashInfo,NULL,0,wr_buf,5/*writebytes*/);// + 4 /*dummy cycles*/);
    printf("Read status register = 0x%02x \n",QSPI_ReadStatusReg(pFlashInfo));

    QSPIFlashGetFlagStatus(pFlashInfo);

    free(wr_buf);
    free(rd_buf);


    return Status;
}

int QSPI_BulkErase(const FL_FlashInfo * pFlashInfo)
{
    int Status;
    //int i,j;
    //int const flash_size=256  ;//00000;//0x1fff0;// 256;//31072;//*4;//262144;

    uint16_t wr_size = 256;

    uint8_t *wr_buf = calloc(wr_size+10, sizeof(uint8_t));
    printf("Read status register = 0x%02x \n",QSPI_ReadStatusReg(pFlashInfo));
    QSPIFlashGetFlagStatus(pFlashInfo);

    printf("Erasing flash ...");

    QSPIFlashWriteEnable(pFlashInfo);
    QSPIFlashGetFlagStatus(pFlashInfo);
    printf("Read status register = 0x%02x \n",QSPI_ReadStatusReg(pFlashInfo));

    wr_buf[0] = COMMAND_BULK_ERASE;

    Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,1/*writebytes*/);// + 4 /*dummy cycles*/);

    while(QSPI_FlashBusy(pFlashInfo)){
        usleep(1000000);//1 sec

    }
    printf("Read status register = 0x%02x \n",QSPI_ReadStatusReg(pFlashInfo));

    QSPIFlashGetFlagStatus(pFlashInfo);

    free(wr_buf);

    while (QSPI_FlashBusy(pFlashInfo));

    printf("\n");

    return Status;
}

// mt25q_qlkt_u_512_abb_0.pdf
// Table 3: Status Register
int QSPI_FlashBusy(const FL_FlashInfo * pFlashInfo)
{
    // Check the status register using the "READ STATUS REGISTER" command. return 0 for ready, 1 for busy.
    return QSPI_ReadStatusReg(pFlashInfo)&0x1;
}

uint8_t QSPI_ReadStatusReg(const FL_FlashInfo * pFlashInfo)
{
    return QSPIReadReg(pFlashInfo, COMMAND_STATUS_REG_READ);
}

static int _QSPIFlashGetFlagStatus(const FL_FlashInfo * pFlashInfo, bool output)
{
    uint8_t *rd_buf = calloc(1, sizeof(uint8_t));
    uint8_t *wr_buf = calloc(1, sizeof(uint8_t));

    wr_buf[0] = COMMAND_FSTATUS_REG_READ;
    //wr_buf[1] = 0;

    QSPI_Transfer(pFlashInfo,rd_buf,1,wr_buf,1);

    if (output) printf("FLag Status 0x%02x\n",rd_buf[0]);

    return rd_buf[0];
}

int QSPIReadReg(const FL_FlashInfo * pFlashInfo, uint8_t cmd)
{
    uint8_t *rd_buf = calloc(1, sizeof(uint8_t));
    uint8_t *wr_buf = calloc(2, sizeof(uint8_t));

    wr_buf[0] = cmd;

    QSPI_Transfer(pFlashInfo,rd_buf,1,wr_buf,2);

    return rd_buf[0];
}
int QSPIReadRegWrite(const FL_FlashInfo * pFlashInfo,uint8_t cmd,uint8_t data)
{
    QSPIFlashWriteEnable(pFlashInfo);

    uint8_t *wr_buf = calloc(2, sizeof(uint8_t));

    wr_buf[0] = cmd;
    wr_buf[1] = data;

    QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,2);

    return 0;//rd_buf[0];
}

int QSPIFlashGetFlagStatus(const FL_FlashInfo * pFlashInfo)
{
    return _QSPIFlashGetFlagStatus(pFlashInfo, true);
}

int QSPIFlashWriteEnable(const FL_FlashInfo * pFlashInfo)
{
    int Status;
    uint8_t wr_buf[1];

    wr_buf[0]= COMMAND_WRITE_ENABLE;
    Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,1);
    if (Status != 0)
    {
        fprintf(stderr, " *** ERROR: %s QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }

    return Status;
}

// RESET ENABLE and RESET MEMORY Command

int SpiFlashSWReset(const FL_FlashInfo * pFlashInfo)
{
    int Status;
    uint8_t *wr_buf = calloc(1, sizeof(uint8_t));

    wr_buf[0] = 0x66;//reset enable
    Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,1);
       if (Status != 0)
       {
           fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
           exit(EXIT_FAILURE);
       }
       wr_buf[0] = 0x99;//reset memory
       Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,1);
    if (Status != 0)
    {
        fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }

    return Status;
}

int SpiFlash4bytemodeEnableNVR(const FL_FlashInfo * pFlashInfo)
{
    int Status = 0;
    uint8_t *wr_buf = calloc(1, sizeof(uint8_t));
    //uint8_t *rd_buf = calloc(2, sizeof(uint8_t));

    // read out the Non volatile register.
    wr_buf[0] = COMMAND_RD_NONVLT_CFG_REG;
  //  Status = QSPI_Transfer_reg(baseAddr,rd_buf,2,wr_buf,1);
    if (Status != 0)
    {
        fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }

    //printf("RD NVR = 0X%02X 0X%02X \n",rd_buf[0],rd_buf[1]);
    QSPIFlashWriteEnable(pFlashInfo);
    //modify and write
    wr_buf[0] = COMMAND_WR_NONVLT_CFG_REG;
    wr_buf[1] = 0xFF;//rd_buf[0]&0xFF;
    wr_buf[2] = 0xFF;//rd_buf[1];
    //printf("")
    Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,3);

    if (Status != 0)
    {
        fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }
    wr_buf[0] = 0x66;//reset enable
    Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,1);
    if (Status != 0)
    {
        fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }
    wr_buf[0] = 0x99;//reset memory
    //wr64(baseAddr,AXIREG,CS_TRIG);

    Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,1);
    if (Status != 0)
    {
        fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }
    usleep(10000);

        SpiFlashSWReset(pFlashInfo);
        usleep(10000);
        QSPIFlashGetFlagStatus(pFlashInfo);

    return Status;
}




int SpiFlash4bytemodeEnable(const FL_FlashInfo * pFlashInfo)
{
    int Status;
    uint8_t *wr_buf = calloc(1, sizeof(uint8_t));

    /*Status =*/ QSPIFlashWriteEnable(pFlashInfo);

    /*
     * Prepare the COMMNAD_ENTER_4BYTE_ADDRESS_MODE.
     */
    wr_buf[0] = COMMAND_ENTER_4BYTE_ADDRESS_MODE;

    /*
     * Initiate the Transfer.
     */
    Status = QSPI_Transfer(pFlashInfo,NULL,0,wr_buf,1);
    if (Status != 0)
    {
        fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }

    return Status;
}

int SpiFlash3bytemodeEnable(const FL_FlashInfo * pFlashInfo)
{
    int status;
    uint8_t *wr_buf = calloc(1, sizeof(uint8_t));

    status = QSPIFlashWriteEnable(pFlashInfo);

    wr_buf[0] = 0xE9; // COMMAND_EXIT_4BYTE_ADDRESS_MODE;

    /*
     * Initiate the Transfer.
     */
    status = QSPI_Transfer(pFlashInfo, NULL, 0, wr_buf, 1);
    if (status != 0)
    {
        fprintf(stderr, " *** ERROR: %s: QSPI_Transfer failed\n", __func__);
        exit(EXIT_FAILURE);
    }

    return status;
}
#if 0
int QspiSetExtendedAddress(uint32_t * baseAddr, uint8_t segment)
{
    int status;
    uint8_t writeBuffer[2], readBuffer[2];

    //printf("\n%s segment %u\n", __func__, segment);

    status = QSPIFlashWriteEnable(baseAddr);

    SpiFlash3bytemodeEnable(baseAddr);

    memset(writeBuffer, 0, sizeof(writeBuffer));
    writeBuffer[0] = 0xC5; // WRITE EXTENDED ADDRESS REGISTER
    writeBuffer[1] = segment;

    status = QSPI_Transfer(baseAddr, NULL, 0, writeBuffer, sizeof(writeBuffer));
    if (status != 0)
    {
        fprintf(stderr, " *** ERROR: %s write extended address register failed\n", __func__);
        exit(EXIT_FAILURE);
    }

    memset(writeBuffer, 0, sizeof(writeBuffer));
    writeBuffer[0] = 0xC8; // READ EXTENDED ADDRESS REGISTER

    status = QSPI_Transfer(baseAddr, readBuffer, sizeof(readBuffer), writeBuffer, 1/*sizeof(writeBuffer)*/);
    if (status != 0)
    {
        fprintf(stderr, " *** ERROR: %s read extended address register failed\n", __func__);
        exit(EXIT_FAILURE);
    }

    if (readBuffer[0] != segment)
    {
        fprintf(stderr, " *** ERROR: read extended address register mismatch: expected 0x%02X != read 0x%02X", segment, readBuffer[0]);
        exit(EXIT_FAILURE);
    }

    return status;
}
#endif

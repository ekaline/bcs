/**
 *     This file contains SF2 specific code.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "FL_FlashLibrary.h"
#include "actel.h"
#include "actel_common.h"
#include "printlog.h"
#include "sf2commands.h"
#include "actel_sf2.h"

// Candidates for actel_common:
int ActelDumpInfo(const FL_FlashInfo * pFlashInfo);

#define EXAR_IMAGE_NO                     5 // Use it as implicit value for Mango2 case.
#define SODIMMA                        0x01
#define SODIMMB                        0x02

#define JEDEC_SODIMM_DDR4_ID           0x0c
#define JEDEC_SODIMM_KOS800_ID         0x80
#define JEDEC_SODIMM_KOS933_ID         0x81
#define JEDEC_SODIMM_QDR2_ID           0x90
#define JEDEC_SODIMM_QDR2_B_ID         0x91
#define JEDEC_DEBUG_TYPEB_ID           0x98


//Fill the SF2 read buffer
int SF2ReadFlash(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress)
{
    FL_Error errorCode = FL_AdminFillReadBuffer(pFlashInfo, flashAddress);
    if (errorCode != FL_SUCCESS)
    {
        fprintf(stderr, "AAAAAAAAAAAaaarRRRRRRRRRRRRRggggGGGGGGGGhHHHHHHH...\n");
    }

    return errorCode == FL_SUCCESS ? 0 : -1;
}

//Function which reboots SF2 with a new image
static void SF2RebootFPGA(const FL_FlashInfo * pFlashInfo, uint32_t image_addr, int image_no)
{
    int i, state;

    ActelWrite(pFlashInfo, 0xE, image_addr, image_no);

    i = 1000;
    while ( ((state = ActelFlashIsReady(pFlashInfo, image_addr)) > 0) && i){//Wait until authentication is done, 2 to 3 minutes?
        i--;
        usleep(10000);
//        prn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "WriteFlash cmd: Not ready @ 0x%08X!\n", image_addr);
    }
    if(!i){
        prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Timeout waiting for SF2 to be ready\n");
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 is being reprogrammed. Please wait 3 minutes before rebooting the server!\n");
    }else{
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 is being reprogrammed. Please wait 3 minutes before rebooting the server!\n");
    }
}


// Send an SF2 command request.
static int16_t ActelTelemetryCmd(const FL_FlashInfo * pFlashInfo, uint16_t reg)
{
    // Wait for ready
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(100);
    }

    ActelWrite(pFlashInfo, READ_TELEMETRY, 0, reg);
    // Test if command is known
    usleep(10);
    uint16_t RdData = ActelRead(pFlashInfo);
    if ((RdData & SF2_STATUS_UNKNOWN_CMD) != 0 && (RdData & SF2_STATUS_FLASH_BUSY) == 0) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "SF2 unknown command, result: %04X\n", RdData);
        return 1;
    }
    // Wait for result
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(100);
    }
    // Read back the resulting value
    int16_t val = ActelReadFlash(pFlashInfo, 0);
    return val;
}

static float conv_linear11_to_float(uint16_t ltc_val)
{
    float result = 0;
    int8_t exp = ltc_val>>11;
    int16_t mant = ltc_val & 0x7ff;

    if(exp > 0x0F) exp |= 0xE0;
    if(mant > 0x03FF) mant |= 0xF800;
    
    result = mant*pow(2,exp);
    return result;
}

static float conv_linear16_to_float(uint16_t ltc_val)
{
    float result = 0;
    uint16_t mant = ltc_val;
    int8_t exp = 0x13;

    if(exp > 0x0F) exp |= 0xE0;
  //  if(mant > 0x03FF) mant |= 0xF800;

    result = mant*pow(2,exp);
    return result;
}

//Print power supply information 
static void ActelReadPower(const FL_FlashInfo * pFlashInfo)
{
    printf("\nPower supply information\n");
    printf("=========================\n");
    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000089
    uint16_t ltc_val= ActelTelemetryCmd(pFlashInfo, 0x89);
    float cur_pmb = conv_linear11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCIe 12V current: %f A", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000135
    ltc_val= ActelTelemetryCmd(pFlashInfo, 0x135);
    float volt_pmb = conv_linear11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCIe 12V Power: %f W", volt_pmb*cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");


    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000099
    ltc_val = ActelTelemetryCmd(pFlashInfo, 0x99);
    cur_pmb = conv_linear11_to_float(ltc_val);
    if (cur_pmb < 0)
        cur_pmb = 0;
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "External 12V Current: %f A", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000137
    ltc_val= ActelTelemetryCmd(pFlashInfo, 0x137);
    volt_pmb = conv_linear16_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "External 12V Power: %f W", (2.415*volt_pmb) * (cur_pmb));
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000098
    ltc_val = ActelTelemetryCmd(pFlashInfo, 0x98); // Core Current - Channel0 IOUT
    cur_pmb = conv_linear11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "FPGA Core Current: %f A", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

}

// Print temperature information.
static void ActelReadTemp(const FL_FlashInfo * pFlashInfo)
{

    printf("Temperature information\n");
    printf("=======================\n");
    
    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000100
    int16_t tem_reg = ActelTelemetryCmd(pFlashInfo, 0x100); // PCB thermal

    if (((tem_reg>>5) & 0x400) == 1)
        printf("PCB Temperature is : - %f degree C\n", 0.125*(-(tem_reg & 0x400) + (tem_reg & ~0x400)));
    else
        printf("PCB Temperature is : + %f degree C\n", 0.125*(tem_reg>>5));

    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000132
    uint16_t ltc_val= ActelTelemetryCmd(pFlashInfo, 0x132);
    float cur_pmb = conv_linear11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "FPGA Core Supply Module Temperature: %f degree C", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

}

// Detect and read id of sodimm modules.
static void ActelReadSodimmsId(const FL_FlashInfo * pFlashInfo)
{
    uint8_t sodimma_id = 0, sodimmb_id = 0;
    // Equivalent to ./testfpga -c reg w 0x40 0x3800000000000140
    uint16_t id = ActelTelemetryCmd(pFlashInfo, 0x140);
    sodimma_id = id & 0xff;
    sodimmb_id = id >> 8;

    switch (sodimma_id) {
        case JEDEC_SODIMM_DDR4_ID:
            printf("SODIMM-A slot has DDR4 memory module \n");
            break;
        case JEDEC_SODIMM_KOS800_ID:
            printf("SODIMM-A slot has SQIVe@800-288 memory module \n");
            break;
        case JEDEC_SODIMM_KOS933_ID:
            printf("SODIMM-A slot has SQIVe@933-288 memory module \n");
            break;
        case JEDEC_SODIMM_QDR2_ID:
            printf("SODIMM-A slot has QDRII+@633-288-A memory module \n");
            break;
        case JEDEC_SODIMM_QDR2_B_ID: 
            printf("SODIMM-A slot has QDRII+@633-288-B memory module \n");
            break;
        case JEDEC_DEBUG_TYPEB_ID:
            printf("SODIMM-A slot has DEBUG TYPEB memory module \n");
            break;
        default:
            printf("SODIMM-A slot has no memory module (or unsupported Id: %x)\n", sodimma_id);
            break;
    }

    switch (sodimmb_id) {
        case JEDEC_SODIMM_DDR4_ID:
            printf("SODIMM-B slot has DDR4 memory module \n");
            break;
        case JEDEC_SODIMM_KOS800_ID:
            printf("SODIMM-B slot has SQIVe@800-288 memory module \n");
            break;
        case JEDEC_SODIMM_KOS933_ID:
            printf("SODIMM-B slot has SQIVe@933-288 memory module \n");
            break;
        case JEDEC_SODIMM_QDR2_ID:
            printf("SODIMM-B slot has QDRII+@633-288-A memory module \n");
            break;
        case JEDEC_SODIMM_QDR2_B_ID:
            printf("SODIMM-B slot has QDRII+@633-288-B memory module \n");
            break;
        case JEDEC_DEBUG_TYPEB_ID:
            printf("SODIMM-B slot has DEBUG TYPEB memory module \n");
            break;
        default:
            printf("SODIMM-B slot has no memory module (or unsupported Id: %x)\n", sodimmb_id);
            break;
    }
}


static int ActelReadPllDevRevision(const FL_FlashInfo * pFlashInfo)
{
    printf("pll version is: 0x%x\n", ActelTelemetryCmd(pFlashInfo, 0x05));
    return 0;
}

static int ActelReadLtcVersion(const FL_FlashInfo * pFlashInfo)
{
    printf("ltc version is: 0x%x\n", ActelTelemetryCmd(pFlashInfo, 0xB4));
    return 0;
}

//Read Exar reg and return value
static uint8_t ActelExarReadReg(const FL_FlashInfo * pFlashInfo, uint16_t reg, const char *exar_str)
{
    uint16_t exar_id = SF2_READ_EXAR1_REG;
    // Wait for ready
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(100);
    }

    if (exar_str == NULL) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Specify 'exar1' or 'exar2'\n");
        exit(-1);
    } else {
        if (strcmp(exar_str, "exar1") == 0)
            exar_id = SF2_READ_EXAR1_REG;
        else if (strcmp(exar_str, "exar2") == 0)
            exar_id = SF2_READ_EXAR2_REG;
        else {
            prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Specify 'exar1' or 'exar2'\n");
            exit(-1);
        }
    }

    // Issue sub command to read power
    ActelWrite(pFlashInfo, exar_id, 0, reg);
    // Test if command is known (because (ActelFlashIsReady writes
    usleep(10);
    uint16_t RdData = ActelRead(pFlashInfo);
    if ((RdData & SF2_STATUS_UNKNOWN_CMD) != 0 && (RdData & SF2_STATUS_FLASH_BUSY) == 0) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "SF2 unknown command, result: %04X\n", RdData);
        return 1;
    }
    // Wait for result
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(100);
    }
    // Read back the resulting value
    uint16_t val = ActelReadFlash(pFlashInfo, 0);
    return val & 0xff;
}

static int ActelReadExarVersion(const FL_FlashInfo * pFlashInfo, const char *exar) {
    printf("%s version is: 0x%x\n", exar, ActelExarReadReg(pFlashInfo, 0x8075, exar));
    return 0;
}

// Read LTC register
static int16_t ActelReadLtcReg(const FL_FlashInfo * pFlashInfo, uint16_t reg) {
    // Wait for ready
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(100);
    }

    ActelWrite(pFlashInfo, READ_TELEMETRY, 0, reg);
    // Test if command is known (because (ActelFlashIsReady writes
    usleep(10);
    uint16_t RdData = ActelRead(pFlashInfo);
    if ((RdData & SF2_STATUS_UNKNOWN_CMD) != 0 && (RdData & SF2_STATUS_FLASH_BUSY) == 0) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "SF2 unknown command, result: %04X\n", RdData);
        return 1;
    }
    // Wait for result
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(100);
    }
    // Read back the resulting value
    int16_t val = ActelReadFlash(pFlashInfo, 0);
    return val;
}

float conv_ltc_l11_to_float(uint16_t ltc_val)
{
    float result = 0;
    int8_t exp = ltc_val >> 11;
    int16_t mant = ltc_val & 0x7ff;

    if (exp > 0x0F) exp |= 0xE0;
    if (mant > 0x03FF) mant |= 0xF800;

    result = mant * pow(2, exp);
    return result;
}

float conv_ltc_l16_to_float(uint16_t ltc_val)
{
    float result = 0;
    uint16_t mant = ltc_val;
    int8_t exp = 0x13;

    if (exp > 0x0F) exp |= 0xE0;
    //  if(mant > 0x03FF) mant |= 0xF800;

    result = mant * pow(2, exp);
    return result;
}

//Print current values from LTC.
static void ActelReadLtc(const FL_FlashInfo * pFlashInfo)
{
    uint16_t ltc_val = ActelReadLtcReg(pFlashInfo, 0x89); // PCIe - IIN
    float cur_pmb = conv_ltc_l11_to_float(ltc_val);

    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "System Current(PCIe): %f A", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    ltc_val = ActelReadLtcReg(pFlashInfo, 0x98); // Core Current - Channel0 IOUT
    cur_pmb = conv_ltc_l11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Core Current: %f A", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    ltc_val = ActelReadLtcReg(pFlashInfo, 0x99); // External Current - Channel1 IOUT
    cur_pmb = conv_ltc_l11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "External Current: %f A", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");
}

static void ActelReadThermal(const FL_FlashInfo * pFlashInfo)
{
    // Use LTC address space. TBD Rename ActelReadLtcReg to ActelReadSF2Reg.
    int16_t tem_reg = ActelReadLtcReg(pFlashInfo, 0x100); // PCB thermal
    if (((tem_reg >> 5) & 0x400) == 1)
        printf("PCB temperature is : - %f degree C\n", 0.125*(-(tem_reg & 0x400) + (tem_reg & ~0x400)));
    else
        printf("PCB temperature is : + %f degree C\n", 0.125*(tem_reg >> 5));
}

float conv_adc(uint16_t adc_reg)
{
    int16_t val = 0, final;

    val = adc_reg >> 4;
    if ((val & 0x800) != 0) {
    }
    final = ((val) & ((1 << (11)) - 1));
    return (float)(final *(2.048 / 0x800));
}

static void ActelReadSodimmAdc(const FL_FlashInfo * pFlashInfo, uint8_t id)
{
    uint16_t adc_reg = 0;
    float final = 0;

    uint8_t sodimma_id = 0, sodimmb_id = 0;
    uint16_t sid = ActelReadLtcReg(pFlashInfo, 0x140);

    sodimma_id = sid & 0xff;
    sodimmb_id = sid >> 8;

    if (id == SODIMMA) {
        if (sodimma_id != 0 && sodimma_id == JEDEC_DEBUG_TYPEB_ID) {
            // Use LTC address space - read ADC1.
            adc_reg = ActelReadLtcReg(pFlashInfo, 0x110);
            printf("SODIMM-A ADC1 chan0 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.90) || (final > 1.965))
                printf("SODIMM ADC1 channel 0 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x112);
            printf("SODIMM-A ADC1 chan1 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.90) || (final > 1.965))
                printf("SODIMM ADC1 channel 1 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x114);
            printf("SODIMM-A ADC1 chan2 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.15) || (final >= 1.85))
                printf("SODIMM ADC1  channel 2 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x116);
            printf("SODIMM-A ADC1 chan3 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.15) || (final >= 1.85))
                printf("SODIMM ADC1 channel 3 voltage readout failed\n");

            // Use LTC address space - read ADC2.
            adc_reg = ActelReadLtcReg(pFlashInfo, 0x111);
            printf("SODIMM-A ADC2 chan0 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 0.55) || (final >= 0.95))
                printf("SODIMM ADC2 channel 0 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x113);
            printf("SODIMM-A ADC2 chan1 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 0.55) || (final >= 0.95))
                printf("SODIMM ADC2 channel 1 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x115);
            printf("SODIMM-A ADC2 chan2 voltage: %f\n", conv_adc(adc_reg));
            adc_reg = ActelReadLtcReg(pFlashInfo, 0x117);
            printf("SODIMM-A ADC2 chan3 voltage: %f\n", conv_adc(adc_reg));
        }
    }
    else if (id == SODIMMB) { // SODIMMB)
        if (sodimmb_id != 0 && sodimmb_id == JEDEC_DEBUG_TYPEB_ID) {
            // Use LTC address space - read ADC1.
            adc_reg = ActelReadLtcReg(pFlashInfo, 0x120);
            printf("SODIMM-B ADC1 chan0 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.90) || (final > 1.965))
                printf("SODIMM ADC1 channel 0 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x122);
            printf("SODIMM-B ADC1 chan1 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.90) || (final > 1.965))
                printf("SODIMM ADC1 channel 1 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x124);
            printf("SODIMM-B ADC1 chan2 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.15) || (final >= 1.85))
                printf("SODIMM ADC1  channel 2 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x126);
            printf("SODIMM-B ADC1 chan3 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 1.15) || (final >= 1.85))
                printf("SODIMM ADC1 channel 3 voltage readout failed\n");

            // Use LTC address space - read ADC2.
            adc_reg = ActelReadLtcReg(pFlashInfo, 0x121);
            printf("SODIMM-B ADC2 chan0 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 0.55) || (final >= 0.95))
                printf("SODIMM ADC2 channel 0 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x123);
            printf("SODIMM-B ADC2 chan1 voltage: %f\n", final = conv_adc(adc_reg));
            if ((final <= 0.55) || (final >= 0.95))
                printf("SODIMM ADC2 channel 1 voltage readout failed\n");

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x125);
            printf("SODIMM-B ADC2 chan2 voltage: %f\n", conv_adc(adc_reg));

            adc_reg = ActelReadLtcReg(pFlashInfo, 0x127);
            printf("SODIMM-B ADC2 chan3 voltage: %f\n", conv_adc(adc_reg));
        }
    }
}

static void ActelReadSodimmTemp(const FL_FlashInfo * pFlashInfo, uint8_t sodimm_id)
{
    uint16_t temp = 0, value;
    uint8_t sodimma_id = 0, sodimmb_id = 0;
    uint16_t id = ActelReadLtcReg(pFlashInfo, 0x140);

    sodimma_id = id & 0xff;
    sodimmb_id = id >> 8;

    if (sodimm_id == SODIMMA) {
        if (sodimma_id != 0 && sodimma_id == JEDEC_DEBUG_TYPEB_ID) {
            temp = ActelReadLtcReg(pFlashInfo, 0x130);
            value = ((temp) & ((1 << (12)) - 1));
            printf("SODIMM-A Temperature: %f degrees C\n", 0.25*(value >> 2));
        }
    }
    else {
        if (sodimmb_id != 0 && sodimmb_id == JEDEC_DEBUG_TYPEB_ID) {
            temp = ActelReadLtcReg(pFlashInfo, 0x131);
            value = ((temp) & ((1 << (12)) - 1));
            printf("SODIMM-B Temperature: %f degrees C\n", 0.25*(value >> 2));
        }
    }
}

static void ActelReadLtcTemp(const FL_FlashInfo * pFlashInfo)
{
    uint16_t ltc_val = ActelReadLtcReg(pFlashInfo, 0x132);
    float cur_pmb = conv_ltc_l11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Temperature (LTM Master): %f ", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    ltc_val = ActelReadLtcReg(pFlashInfo, 0x133);
    cur_pmb = conv_ltc_l11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Temperature (LTM Slave): %f ", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    ltc_val = ActelReadLtcReg(pFlashInfo, 0x134);
    cur_pmb = conv_ltc_l11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Die Temperature (LTC): %f ", cur_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");
}

static void ActelReadLtcVolt(const FL_FlashInfo * pFlashInfo)
{
    uint16_t ltc_val = ActelReadLtcReg(pFlashInfo, 0x135);
    float volt_pmb = conv_ltc_l11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "12V Input supply: %f ", volt_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");
    ltc_val = ActelReadLtcReg(pFlashInfo, 0x136);
    volt_pmb = conv_ltc_l16_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "VDD_CORE output: %f ", volt_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");
    ltc_val = ActelReadLtcReg(pFlashInfo, 0x137);
    volt_pmb = conv_ltc_l16_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "VDD_12V_LTM output: %f ", 2.415*volt_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");

    ltc_val = ActelReadLtcReg(pFlashInfo, 0x138);
    volt_pmb = conv_ltc_l16_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "VDD_1V2 output: %f ", volt_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");
}

static void ActelReadLtcCore(const FL_FlashInfo * pFlashInfo)
{
    uint16_t ltc_val = ActelReadLtcReg(pFlashInfo, 0x139);
    float volt_pmb = conv_ltc_l11_to_float(ltc_val);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Core current (LTC): %f ", volt_pmb);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "\n");
}

static void ActelLs(const FL_FlashInfo * pFlashInfo)
{
    uint32_t actel_revision;
    printf("\nCard information:\n");
    printf("-----------------\n\n");

    actel_revision = ActelRevision(pFlashInfo);
    prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 SW Revision %d.%d.%d.%d\n", (actel_revision >> 24), ((actel_revision >> 16) & 0xFF), ((actel_revision >> 8) & 0xFF), (actel_revision & 0xFF));
    ActelReadExarVersion(pFlashInfo, "exar1");
    ActelReadExarVersion(pFlashInfo, "exar2");
    ActelReadLtcVersion(pFlashInfo);
    ActelReadPllDevRevision(pFlashInfo);
    printf("\n");
    ActelReadSodimmsId(pFlashInfo);
    printf("\n");
    ActelReadLtc(pFlashInfo);
    printf("\n");
    ActelReadThermal(pFlashInfo);
    printf("\n");
    ActelReadSodimmAdc(pFlashInfo, SODIMMA);
    ActelReadSodimmAdc(pFlashInfo, SODIMMB);
    ActelReadSodimmTemp(pFlashInfo, SODIMMA);
    ActelReadSodimmTemp(pFlashInfo, SODIMMB);
    printf("\n");
    ActelReadLtcTemp(pFlashInfo);
    printf("\n");
    ActelReadLtcVolt(pFlashInfo);
    printf("\n");
    ActelReadLtcCore(pFlashInfo);
    printf("\n");
}

//Write flash readbuffer to stdout.
//Read until NULL char. Cannot be longer than flash buffer
static int ActelFlashBufToStdout(const FL_FlashInfo * pFlashInfo)
{
    uint16_t flashData;
    uint32_t flashAddr;
    // Read back the resulting string
    for (flashAddr = 0; flashAddr < pFlashInfo->FlashSize / 2; flashAddr++) {
        flashData = ActelReadFlash(pFlashInfo, flashAddr);
        if ((flashData & 0xFF) == 0 || (flashData>>8) == 0) {
            break;
        }
        //printf("%02x %02x", flashData&0xFF, flashData>>8);// Write data to stdout
        printf("%c%c", (char)(flashData&0xFF), (char)(flashData>>8));// Write data to stdout
    }
    printf("\n");
    return 0;
}

//Function which transfer exar image from SPI flash to exar nwm, note nwm only supports 20 writings
static void SF2copy2EXAR(const FL_FlashInfo * pFlashInfo, char *exar_id)
{
    uint16_t image_number = SF2_LOAD_EXAR1_IMAGE;

    if (exar_id == NULL) {
        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Specify 'exar1' or 'exar2' or 'ltc'\n");
        exit(-1);
    } else {
        if (strcmp(exar_id, "exar1") == 0) { 
            image_number = SF2_LOAD_EXAR1_IMAGE;
            prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 start copy exar1\n");
        } else if (strcmp(exar_id, "exar2") == 0) {
            image_number = SF2_LOAD_EXAR2_IMAGE;
            prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 start copy exar2\n");
        } else if (strcmp(exar_id, "ltc") == 0) {
            image_number = SF2_LOAD_LTC_IMAGE;
            prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 start copy LTC\n");
        } else {
            prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Specify 'exar1' or 'exar2' or 'ltc'\n");
            exit(-1);
        }
    }

    ActelWrite(pFlashInfo, 0xE, 0, image_number);

    // Wait for ready
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(10000);
        //prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 wait copy exar\n");
    }

    // Wait for ready
    while (ActelFlashIsReady(pFlashInfo, 0L) > 0) {
        usleep(100);
    }
    // Log the result
    ActelFlashBufToStdout(pFlashInfo);
}


// Parse input cmds
int cmdSF2(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    int argp = 0;//Parse counter
    uint32_t addr;
    uint32_t len;
    uint16_t data;
    uint32_t actel_revision;

    FL_ExitOnError(pLogContext, FL_AdminGetConfigurationInformation(pFlashInfo));// Read flash status

    while (argc > argp) {
        if (strcmp("reboot", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                if (strcmp("golden", argv[argp]) == 0) {
                    SF2RebootFPGA(pFlashInfo, 0, 0);
                } else if (strcmp("probe", argv[argp]) == 0) {
                    SF2RebootFPGA(pFlashInfo, 0, 1);
                } else if (strcmp("power", argv[argp]) == 0) {
                    argp++;
                    if (argc > argp) {
                        SF2copy2EXAR(pFlashInfo, argv[argp]);
                    } else
                        SF2copy2EXAR(pFlashInfo, NULL);
                }
            } else {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Missing image selector\n");
            }
        } else if (strcmp("status", argv[argp]) == 0)
        {
            uint16_t adminStatus;
            FL_ExitOnError(pLogContext, FL_AdminReadStatus(pFlashInfo, &adminStatus));
        }
        else if (strcmp("revision", argv[argp]) == 0)
        {
            actel_revision = ActelRevision(pFlashInfo);
            prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "SF2 SW Revision %d.%d.%d.%d\n", (actel_revision >> 24), ((actel_revision >> 16) & 0xFF), ((actel_revision >> 8) & 0xFF), (actel_revision & 0xFF));
        }
        else if (strcmp("read-power", argv[argp]) == 0) {
            ActelReadPower(pFlashInfo);
        } else if (strcmp("readcurrent", argv[argp]) == 0) {
            ActelReadLtc(pFlashInfo);
        } else if (strcmp("read-pcb-temp", argv[argp]) == 0) {
            ActelReadThermal(pFlashInfo);
        } else if (strcmp("read-sodimma-volt", argv[argp]) == 0) {
            ActelReadSodimmAdc(pFlashInfo, SODIMMA);
        } else if (strcmp("read-sodimmb-volt", argv[argp]) == 0) {
            ActelReadSodimmAdc(pFlashInfo, SODIMMB);
        } else if (strcmp("read-temperature", argv[argp]) == 0) {
            ActelReadTemp(pFlashInfo);
        } else if (strcmp("read-sodimms-id", argv[argp]) == 0) {
            ActelReadSodimmsId(pFlashInfo);
        } else if (strcmp("read-pll-version", argv[argp]) == 0) {
            ActelReadPllDevRevision(pFlashInfo);
        } else if (strcmp("read-ltc-version", argv[argp]) == 0) {
            ActelReadLtcVersion(pFlashInfo);
        } else if (strcmp("read-exar1-version", argv[argp]) == 0) {
            ActelReadExarVersion(pFlashInfo, "exar1");
        } else if (strcmp("read-exar2-version", argv[argp]) == 0) {
            ActelReadExarVersion(pFlashInfo, "exar2");
        } else if (strcmp("read-ltc-version", argv[argp]) == 0) {
            ActelReadLtcVersion(pFlashInfo);
        } else if (strcmp("read-sodimma-temp", argv[argp]) == 0) {
            ActelReadSodimmTemp(pFlashInfo, SODIMMA);
        } else if (strcmp("read-sodimmb-temp", argv[argp]) == 0) {
            ActelReadSodimmTemp(pFlashInfo, SODIMMB);
        } else if (strcmp("read-ltc-temp", argv[argp]) == 0) {
            ActelReadLtcTemp(pFlashInfo);
        } else if (strcmp("read-ltc-voltage", argv[argp]) == 0) {
            ActelReadLtcVolt(pFlashInfo);
        } else if (strcmp("read-ltc-corecurrent", argv[argp]) == 0) {
            ActelReadLtcCore(pFlashInfo);
        } else if (strcmp("ls", argv[argp]) == 0) {
            ActelLs(pFlashInfo);
        } else if (strcmp("exarreg", argv[argp]) == 0) {
            argp++;
            if (argc > argp) {
                uint16_t cmd = strtol(argv[argp], NULL, 0);
                uint8_t val = 0;
                argp++;
                if (argc > argp) {
                    val = ActelExarReadReg(pFlashInfo, cmd & 0xffff, argv[argp]);
                } else {
                    val = ActelExarReadReg(pFlashInfo, cmd & 0xffff, NULL);
                }
                prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Exar reg 0x%04x result: 0x%02x\n", cmd, val);
            } else {
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Missing Exar read register\n");
            }
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
                "reboot probe|power<devicename>|: Reprogram SF2\n"
                "revision                       : Read SF2 SW revision\n"
                "status                         : Read SF2 SW status\n"
                "flash2file <addr> <FILE> <len> : Write flash to file\n"
                "flashout <addr> <len>          : Flash to stdout\n"
                "file2flash <addr> <FILE>       : Write file to flash\n"
                "read-power                     : Read power supply info\n"
                "read-temperature               : Read temperature info\n"
                "read-sodimms-id                : Read id of mounted sodimm modules\n"
                "read-pll-version               : Read pll device version\n"
                "read-ltc-version               : Read LTC configuration version\n"
                "read-exar1-version             : Read exar1 configuration version\n"
                "read-exar2-version             : Read exar2 configuration version\n"
#if 0 // Do not show these commands to users
                "flashread                      : ???\n"
                "scratch                        : ???\n"
                "dumpinfo                       : ???\n"
                "read-sodimma-temp              : ???\n"
                "read-sodimmb-temp              : ???\n"
                "read-ltc-temp                  : ???\n"
                "read-ltc-voltage               : ???\n"
                "read-ltc-corecurrent           : ???\n"
                "ls                             : ???\n"
                "exarreg                        : ???\n"
#endif
                "");
            return 0;
        } else {
            prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Invalid argument: %s\nUse -h for help\n", argv[argp]);
            return -1;
        }
        argp++;
    }

    return 0;
}

#if 0
static void ActelWriteFlashSF2WritePageCmd(const FL_FlashInfo * pFlashInfo, uint32_t FlashAddr)
{
    ActelWrite(pFlashInfo, 0xA, FlashAddr, WRITE_PAGE_CMD);
}
#endif

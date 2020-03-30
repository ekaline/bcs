#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>

#include "fpgaregs.h"
/***************************************************
 * System monitor functions
 ***************************************************/
//Read FPGA system monitor

uint32_t sysmon_read(uint32_t *pRegistersBaseAddress, uint32_t addr) {
    uint32_t timeout;
    wr64(pRegistersBaseAddress, REG_SYSMON, (addr & 0x7f) << 16);
    timeout = 100;
    //wait for system monitor to be ready
    while (!(rd64(pRegistersBaseAddress, REG_SYSMON) & 0x10000) && --timeout)
        usleep(1000);
    return rd64(pRegistersBaseAddress, REG_SYSMON) & 0xFFFF;
}


double conv_temp(uint32_t int_val) {
    return int_val * 503.975 / 65536 - 273.15;
}

double conv_volt(uint32_t int_val) {
    return int_val / 65536.0 * 3.0;
}

//Read FPGA temperature in celcius

double sysmon_temp(uint32_t *pRegistersBaseAddress) {
    return conv_temp(sysmon_read(pRegistersBaseAddress, 0));
}
//Read FPGA minimum temperature in celcius

double sysmon_temp_min(uint32_t *pRegistersBaseAddress) {
    return conv_temp(sysmon_read(pRegistersBaseAddress, 0x24));
}
//Read FPGA maximum temperature in celcius

double sysmon_temp_max(uint32_t *pRegistersBaseAddress) {
    return conv_temp(sysmon_read(pRegistersBaseAddress, 0x20));
}

//Read FPGA VccInt voltage

double sysmon_VccInt(uint32_t *pRegistersBaseAddress) {
    return conv_volt(sysmon_read(pRegistersBaseAddress, 1));
}
//Read FPGA VccInt minimum voltage

double sysmon_VccInt_min(uint32_t *pRegistersBaseAddress) {
    return conv_volt(sysmon_read(pRegistersBaseAddress, 0x25));
}
//Read FPGA VccInt maximum voltage

double sysmon_VccInt_max(uint32_t *pRegistersBaseAddress) {
    return conv_volt(sysmon_read(pRegistersBaseAddress, 0x21));
}

//Read FPGA VccAux voltage

double sysmon_VccAux(uint32_t *pRegistersBaseAddress) {
    return conv_volt(sysmon_read(pRegistersBaseAddress, 2));
}
//Read FPGA VccAux minimum voltage

double sysmon_VccAux_min(uint32_t *pRegistersBaseAddress) {
    return conv_volt(sysmon_read(pRegistersBaseAddress, 0x26));
}
//Read FPGA VccAux maximum voltage

double sysmon_VccAux_max(uint32_t *pRegistersBaseAddress) {
    return conv_volt(sysmon_read(pRegistersBaseAddress, 0x22));
}


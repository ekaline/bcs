/* 
 * File:   sysmon.h
 * Author: rdj
 *
 * Created on September 26, 2010, 8:48 PM
 */

#ifndef _SYSMON_H
#define _SYSMON_H

double sysmon_temp(uint32_t *pRegistersBaseAddress);
double sysmon_temp_min(uint32_t *pRegistersBaseAddress);
double sysmon_temp_max(uint32_t *pRegistersBaseAddress);
double sysmon_VccInt(uint32_t *pRegistersBaseAddress);
double sysmon_VccInt_min(uint32_t *pRegistersBaseAddress);
double sysmon_VccInt_max(uint32_t *pRegistersBaseAddress);
double sysmon_VccAux(uint32_t *pRegistersBaseAddress);
double sysmon_VccAux_min(uint32_t *pRegistersBaseAddress);
double sysmon_VccAux_max(uint32_t *pRegistersBaseAddress);


#endif  /* _SYSMON_H */


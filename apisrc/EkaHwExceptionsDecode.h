#ifndef _EKA_HW_EXCEPTIONS_DECODE_H_
#define _EKA_HW_EXCEPTIONS_DECODE_H_

#include <string.h>
#include "EfcMsgs.h"

inline int ekaDecodeExceptions(char* dst,const EfcExceptionsReport* excpt) {
  auto d = dst;
  bool exceptionRaised = false;
  if (excpt->exceptionStatus.globalVector)
    exceptionRaised = true;;
  for(auto i = 0; i < 4; i++) { // 4 Cores
    if (excpt->exceptionStatus.portVector[i])
      exceptionRaised = true;;
  }
  if (exceptionRaised)
    d += sprintf(d,RED"\n\nFPGA internal exceptions:\n" RESET);
  else
    goto END;
  if ((excpt->exceptionStatus.globalVector>>0 )&0x1) d += sprintf(d,"Bit 0: SW->HW Write Corruption\n" );
  if ((excpt->exceptionStatus.globalVector>>1 )&0x1) d += sprintf(d,"Bit 1: HW->SW TCP feedback DMA data full\n" );
  if ((excpt->exceptionStatus.globalVector>>2 )&0x1) d += sprintf(d,"Bit 2: HW->SW TCP feedback DMA control full\n" );
  if ((excpt->exceptionStatus.globalVector>>3 )&0x1) d += sprintf(d,"Bit 3: HW->SW Report feedback DMA data full\n" );
  if ((excpt->exceptionStatus.globalVector>>4 )&0x1) d += sprintf(d,"Bit 4: HW->SW Report feedback DMA control full\n" );
  if ((excpt->exceptionStatus.globalVector>>5 )&0x1) d += sprintf(d,"Bit 5: Controller Watchdog Expired\n" );
  if ((excpt->exceptionStatus.globalVector>>6 )&0x1) d += sprintf(d,"Bit 6: SW->HW EPM Trigger control full\n" );
  if ((excpt->exceptionStatus.globalVector>>7 )&0x1) d += sprintf(d,"Bit 7: EPM Wrong Action Type\n" );
  if ((excpt->exceptionStatus.globalVector>>8 )&0x1) d += sprintf(d,"Bit 8: EPM Wrong Action Source\n" );
  if ((excpt->exceptionStatus.globalVector>>9 )&0x1) d += sprintf(d,"Bit 9: Strategy Double Fire Protection Triggered\n" );
  if ((excpt->exceptionStatus.globalVector>>10)&0x1) d += sprintf(d,"Bit 10: Strategy Context FIFO overrun\n" );
  if ((excpt->exceptionStatus.globalVector>>11)&0x1) d += sprintf(d,"Bit 11: Strategy MD vs Context Out of Sync\n" );
  if ((excpt->exceptionStatus.globalVector>>12)&0x1) d += sprintf(d,"Bit 12: Strategy Security ID mismatch\n" );
  if ((excpt->exceptionStatus.globalVector>>13)&0x1) d += sprintf(d,"Bit 13: SW->HW Strategy Update overrun\n" );

  for(auto i = 0; i < 4; i++) { // 4 Cores
    if (excpt->exceptionStatus.portVector[i]) {
      d += sprintf(d,"\nResolving exception for Core%d\n",i);
      if ((excpt->exceptionStatus.portVector[i]>>0)&0x1)  d += sprintf(d,"Bit 0: MD Parser Error\n");
      if ((excpt->exceptionStatus.portVector[i]>>1)&0x1)  d += sprintf(d,"Bit 1: TCP Sequence Management Corruption\n");
      if ((excpt->exceptionStatus.portVector[i]>>2)&0x1)  d += sprintf(d,"Bit 2: TCP Ack Management Corruption\n");
      if ((excpt->exceptionStatus.portVector[i]>>3)&0x1)  d += sprintf(d,"Bit 3: HW->SW LWIP data drop\n");
      if ((excpt->exceptionStatus.portVector[i]>>4)&0x1)  d += sprintf(d,"Bit 4: HW->SW LWIP control drop\n");
      if ((excpt->exceptionStatus.portVector[i]>>5)&0x1)  d += sprintf(d,"Bit 5: HW->SW Sniffer data drop\n");
      if ((excpt->exceptionStatus.portVector[i]>>6)&0x1)  d += sprintf(d,"Bit 6: HW->SW Sniffer control drop\n");
    }
  }
 END:  
  return d - dst;
}

#endif

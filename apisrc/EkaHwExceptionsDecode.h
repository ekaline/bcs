#ifndef _EKA_HW_EXCEPTIONS_DECODE_H_
#define _EKA_HW_EXCEPTIONS_DECODE_H_

#include <string.h>
#include "EfcMsgs.h"

inline int ekaDecodeExceptions(char* dst,const EfcExceptionsReport* excpt) {
  auto d = dst;

  if (excpt->globalExcpt)
    d += sprintf(d,RED"\n\nFPGA internal exceptions:\n" RESET);
  else
    goto END;
  if ((excpt->globalExcpt>>6) &0x1) d += sprintf(d,"Bit 6: Register access interface became full\n" );
  if ((excpt->globalExcpt>>10)&0x1) d += sprintf(d,"Bit 10: TCPRX[0] data drop\n" );
  if ((excpt->globalExcpt>>11)&0x1) d += sprintf(d,"Bit 11: TCPRX[0] descriptor drop\n" );
  if ((excpt->globalExcpt>>12)&0x1) d += sprintf(d,"Bit 12: TCPRX[1] data drop\n" );
  if ((excpt->globalExcpt>>13)&0x1) d += sprintf(d,"Bit 13: TCPRX[1] descriptor drop\n" );
  if ((excpt->globalExcpt>>14)&0x1) d += sprintf(d,"Bit 14: Sniffer[0] data drop\n" );
  if ((excpt->globalExcpt>>15)&0x1) d += sprintf(d,"Bit 15: Sniffer[0] descriptor drop\n" );
  if ((excpt->globalExcpt>>16)&0x1) d += sprintf(d,"Bit 16: Sniffer[1] data drop\n" );
  if ((excpt->globalExcpt>>17)&0x1) d += sprintf(d,"Bit 17: Sniffer[1] descriptor drop\n" );
  if ((excpt->globalExcpt>>18)&0x1) d += sprintf(d,"Bit 18: LWIP DMA data drop\n" );
  if ((excpt->globalExcpt>>19)&0x1) d += sprintf(d,"Bit 19: LWIP DMA descriptor drop\n" );
  if ((excpt->globalExcpt>>20)&0x1) d += sprintf(d,"Bit 20: Report DMA data drop\n" );
  if ((excpt->globalExcpt>>21)&0x1) d += sprintf(d,"Bit 21: Report DMA descriptor drop\n" );
  if ((excpt->globalExcpt>>26)&0x1) d += sprintf(d,"Bit 26: WD expired\n" );
  if ((excpt->globalExcpt>>27)&0x1) d += sprintf(d,"Bit 27: EPM desc fifo overrun\n" );
  if ((excpt->globalExcpt>>28)&0x1) d += sprintf(d,"Bit 28: EPM Wrong action type\n" );
  if ((excpt->globalExcpt>>29)&0x1) d += sprintf(d,"Bit 29: EPM Wrong action source\n" );
  if ((excpt->globalExcpt>>30)&0x1) d += sprintf(d,"Bit 30: Fire protection: no arming between two fires\n" );
  if ((excpt->globalExcpt>>32)&0x1) d += sprintf(d,"Bit 32: P4 Null session list\n" );
  if ((excpt->globalExcpt>>33)&0x1) d += sprintf(d,"Bit 33: P4 CTX reply overrun\n" );
  if ((excpt->globalExcpt>>34)&0x1) d += sprintf(d,"Bit 34: P4 MD out of sync\n" );
  if ((excpt->globalExcpt>>35)&0x1) d += sprintf(d,"Bit 35: P4 Wrong secid\n" );
  if ((excpt->globalExcpt>>36)&0x1) d += sprintf(d,"Bit 36: P4 CTX update overrun\n" );
  if ((excpt->globalExcpt>>0) &0x3f) d += sprintf(d,"\n--- Core Exceptions --\n");
  for(auto i = 0; i < 4; i++) { // 4 Cores
    if ((excpt->globalExcpt>>i)&0x1) {
      d += sprintf(d,"\nResolving exception for Core%d\n",i);
      if ((excpt->coreExcpt[i]>>0)&0x1)  d += sprintf(d,"Bit 0: RX port overrun, at least one packet was dropped\n");
      if ((excpt->coreExcpt[i]>>1)&0x1)  d += sprintf(d,"Bit 1: MD parser error, happens if the MD parser reached unknown state while parsing MD\n");
      if ((excpt->coreExcpt[i]>>2)&0x1)  d += sprintf(d,"Bit 2: Sequence number overflow - sequence number crossed 64bit value\n");
      if ((excpt->coreExcpt[i]>>3)&0x1)  d += sprintf(d,"Bit 3: Overrun in MD fifo, FATAL\n");
      if ((excpt->coreExcpt[i]>>4)&0x1)  d += sprintf(d,"Bit 4: RX CRC error was detected\n");
      if ((excpt->coreExcpt[i]>>7)&0x1)  d += sprintf(d,"Bit 7: Software DirectTCP remote ACK table table update overrun, happens if table is updated too fast\n");
      if ((excpt->coreExcpt[i]>>8)&0x1)  d += sprintf(d,"Bit 8: Software DirectTCP local SEQ table table update overrun, happens if table is updated too fast\n");
    }
  }
 END:  
  return d - dst;
}

#endif

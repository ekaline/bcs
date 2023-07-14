#ifndef _EKA_HW_EXCEPTIONS_DECODE_H_
#define _EKA_HW_EXCEPTIONS_DECODE_H_

#include "EfcMsgs.h"
#include "eka_macros.h"
#include <string.h>

inline int
ekaDecodeExceptions(char *dst,
                    const EfcExceptionsReport *excpt) {
  auto d = dst;
  bool exceptionRaised = false;
  if (excpt->exceptionStatus.globalVector)
    exceptionRaised = true;
  ;
  for (auto i = 0; i < 4; i++) { // 4 Cores
    if (excpt->exceptionStatus.portVector[i])
      exceptionRaised = true;
    ;
  }
  if (exceptionRaised)
    d += sprintf(d, RED
                 "\n\nFPGA internal exceptions:\n" RESET);
  else
    goto END;
  if ((excpt->exceptionStatus.globalVector >> 0) & 0x1)
    d += sprintf(
        d,
        "Bit 0: SW->HW Configuration Write Corruption\n");
  if ((excpt->exceptionStatus.globalVector >> 1) & 0x1)
    d += sprintf(
        d, "Bit 1: HW->SW Dummy Packet DMA data full\n");
  if ((excpt->exceptionStatus.globalVector >> 2) & 0x1)
    d += sprintf(
        d, "Bit 2: HW->SW Dummy Packet DMA control full\n");
  if ((excpt->exceptionStatus.globalVector >> 3) & 0x1)
    d += sprintf(
        d, "Bit 3: HW->SW Report feedback DMA data full\n");
  if ((excpt->exceptionStatus.globalVector >> 4) & 0x1)
    d += sprintf(
        d,
        "Bit 4: HW->SW Report feedback DMA control full\n");
  if ((excpt->exceptionStatus.globalVector >> 5) & 0x1)
    d += sprintf(d,
                 "Bit 5: P4 Controller Watchdog Expired\n");
  if ((excpt->exceptionStatus.globalVector >> 6) & 0x1)
    d += sprintf(
        d, "Bit 6: SW->HW EPM Trigger control full\n");
  if ((excpt->exceptionStatus.globalVector >> 7) & 0x1)
    d += sprintf(d, "Bit 7: EPM Wrong Action Type\n");
  if ((excpt->exceptionStatus.globalVector >> 8) & 0x1)
    d += sprintf(d, "Bit 8: EPM Wrong Action Source\n");
  if ((excpt->exceptionStatus.globalVector >> 9) & 0x1)
    d += sprintf(d, "Bit 9: P4 Strategy Double Fire "
                    "Protection Triggered\n");
  if ((excpt->exceptionStatus.globalVector >> 10) & 0x1)
    d += sprintf(d,
                 "Bit 10: Strategy Context FIFO overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 11) & 0x1)
    d += sprintf(
        d, "Bit 11: Strategy MD vs Context Out of Sync\n");
  if ((excpt->exceptionStatus.globalVector >> 12) & 0x1)
    d += sprintf(d,
                 "Bit 12: Strategy Security ID mismatch\n");
  if ((excpt->exceptionStatus.globalVector >> 13) & 0x1)
    d += sprintf(
        d, "Bit 13: SW->HW Strategy Update overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 14) & 0x1)
    d += sprintf(d, "Bit 14: Write Combining: WC to EPM "
                    "fifo overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 15) & 0x1)
    d += sprintf(
        d, "Bit 15: Write Combining: CTRL 16B unaligned\n");
  if ((excpt->exceptionStatus.globalVector >> 16) & 0x1)
    d += sprintf(
        d, "Bit 16: Write Combining: CTRL fragmented\n");
  if ((excpt->exceptionStatus.globalVector >> 17) & 0x1)
    d += sprintf(d,
                 "Bit 17: Write Combining: Missing TLP\n");
  if ((excpt->exceptionStatus.globalVector >> 18) & 0x1)
    d += sprintf(
        d,
        "Bit 18: Write Combining: Unknown CTRL Opcode\n");
  if ((excpt->exceptionStatus.globalVector >> 19) & 0x1)
    d += sprintf(
        d,
        "Bit 19: Write Combining: Unaligned data in TLP\n");
  if ((excpt->exceptionStatus.globalVector >> 20) & 0x1)
    d += sprintf(d, "Bit 20: Write Combining: Unaligned "
                    "data in non-TLP\n");
  if ((excpt->exceptionStatus.globalVector >> 21) & 0x1)
    d += sprintf(d, "Bit 21: Write Combining: CTRL bitmap "
                    "ff override\n");
  if ((excpt->exceptionStatus.globalVector >> 22) & 0x1)
    d += sprintf(
        d, "Bit 22: Write Combining: CTRL LSB override\n");
  if ((excpt->exceptionStatus.globalVector >> 23) & 0x1)
    d += sprintf(
        d, "Bit 23: Write Combining: CTRL MSB override\n");
  if ((excpt->exceptionStatus.globalVector >> 24) & 0x1)
    d += sprintf(
        d, "Bit 24: Write Combining: Partial CTRL info\n");
  if ((excpt->exceptionStatus.globalVector >> 25) & 0x1)
    d += sprintf(
        d,
        "Bit 25: Write Combining: pre-WC fifo overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 26) & 0x1)
    d += sprintf(d, "Bit 26: Write Combining: Same heap "
                    "bank violation\n");
  if ((excpt->exceptionStatus.globalVector >> 27) & 0x1)
    d += sprintf(
        d, "Bit 27: NW Controller Watchdog Expired\n");
  if ((excpt->exceptionStatus.globalVector >> 28) & 0x1)
    d += sprintf(d, "Bit 28: NW Strategy Double Fire "
                    "Protection Triggered\n");

  for (auto i = 0; i < 4; i++) { // 4 Cores
    if (excpt->exceptionStatus.portVector[i]) {
      d += sprintf(d, "\nResolving exception for Core%d\n",
                   i);
      if ((excpt->exceptionStatus.portVector[i] >> 0) & 0x1)
        d += sprintf(d, "Bit 0: MD Parser Error\n");
      if ((excpt->exceptionStatus.portVector[i] >> 1) & 0x1)
        d += sprintf(
            d,
            "Bit 1: TCP Sequence Management Corruption\n");
      if ((excpt->exceptionStatus.portVector[i] >> 2) & 0x1)
        d += sprintf(
            d, "Bit 2: TCP Ack Management Corruption\n");
      if ((excpt->exceptionStatus.portVector[i] >> 3) & 0x1)
        d += sprintf(d, "Bit 3: HW->SW LWIP data drop\n");
      if ((excpt->exceptionStatus.portVector[i] >> 4) & 0x1)
        d +=
            sprintf(d, "Bit 4: HW->SW LWIP control drop\n");
      if ((excpt->exceptionStatus.portVector[i] >> 5) & 0x1)
        d +=
            sprintf(d, "Bit 5: HW->SW Sniffer data drop\n");
      if ((excpt->exceptionStatus.portVector[i] >> 6) & 0x1)
        d += sprintf(
            d, "Bit 6: HW->SW Sniffer control drop\n");
    }
  }
END:
  //  d += sprintf(d,"Arm=%d,
  //  Ver=%d\n",excpt->armStatus.armFlag,excpt->armStatus.expectedVersion);
  return d - dst;
}

#endif

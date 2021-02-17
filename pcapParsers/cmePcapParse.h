#ifndef _EKA_CME_PCAP_PARSE_H_
#define _EKA_CME_PCAP_PARSE_H_

#include "ekaNW.h"
#include "eka_macros.h"

typedef int64_t EfhRunCtx; // dummy
typedef int     EkaFhMode; // dummy

static void hexDump (const char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char*)addr;
  if (desc != NULL) printf ("%s:\n", desc);
  if (len == 0) { printf("  ZERO LENGTH\n"); return; }
  if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
  for (i = 0; i < len; i++) {
    if ((i % 16) == 0) {
      if (i != 0) printf ("  %s\n", buff);
      printf ("  %04x ", i);
    }
    printf (" %02x", pc[i]);
    if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
    else buff[i % 16] = pc[i];
    buff[(i % 16) + 1] = '\0';
  }
  while ((i % 16) != 0) { printf ("   "); i++; }
  printf ("  %s\n", buff);
}

static void hexDump (const char *desc, const void *addr, int len) {
  hexDump(desc,(uint8_t*)addr,len);
}


class EkaFhCmeGr {
 public:
  using SecurityIdT = int32_t;
  using PriceT      = int64_t;
  using SizeT       = int32_t;

  using SequenceT   = uint32_t;
  using PriceLevetT = uint8_t;

  bool processPkt(const EfhRunCtx* pEfhRunCtx,
		  const uint8_t*   pkt, 
		  int16_t          pktLen,
		  EkaFhMode        op);

  int processedDefinitionMessages = 0;
};
#endif

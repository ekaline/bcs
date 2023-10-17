#include <arpa/inet.h>
#include <assert.h>
#include <endian.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>

#include "EfhMsgs.h"
#include "efh_macros.h"
#include "eka_macros.h"

inline void printHdr(const EfhMsgHeader *h) {
  printf("%s,", EKA_EXCH_DECODE(h->group.source));
  printf("%u,", h->group.localId);
  printf("%u,", h->underlyingId);
  printf("%ju,", h->securityId);
}

inline void printCommonDef(const EfhSecurityDef *h) {
  printf("%c,", printEfhSecurityType(h->securityType));
  printf("%c,", (char)h->exchange);
  printf("%c,", printEfhSecurityType(h->underlyingType));
  printf("%s,",
         std::string(h->classSymbol, sizeof(h->classSymbol))
             .c_str());
  printf("%s,",
         std::string(h->underlying, sizeof(h->underlying))
             .c_str());
  printf("%s,", std::string(h->exchSecurityName,
                            sizeof(h->exchSecurityName))
                    .c_str());
  printf("%u,", h->expiryDate);
  printf("%u,", h->contractSize);
  printf("%ju,", h->opaqueAttrA);
  printf("%ju,", h->opaqueAttrB);
}

inline void
printOptionDefinition(const EfhOptionDefinitionMsg *msg) {
  printHdr(&msg->header);
  printCommonDef(&msg->commonDef);
  printf("%jd,", msg->strikePrice);
  printf("%c,", msg->optionType == EfhOptionType::kCall
                    ? 'C'
                    : 'P');
  printf("\n");
}

int main(int argc, char *argv[]) {
  FILE *definitionsFile = fopen(argv[1], "rb");
  if (!definitionsFile)
    on_error_stderr("cannot open %s file", argv[1]);

  EfhOptionDefinitionMsg msg{};
  while (fread(&msg, sizeof(msg), 1, definitionsFile) ==
         1) {
    printOptionDefinition(&msg);
  }
  fclose(definitionsFile);

  return 0;
}

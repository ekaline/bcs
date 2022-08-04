#ifndef _EKA_FH_PARSER_COMMON_H
#define _EKA_FH_PARSER_COMMON_H

#include <algorithm>
#include <string.h>
#include "EfhMsgs.h"

template <typename SymbolType, std::size_t N>
SymbolType &copySymbol(SymbolType &symbol, const char (&src)[N]) {
  char *s = stpncpy(symbol, src, std::min(sizeof symbol, N));
  if (static_cast<uintptr_t>(s - symbol) < sizeof symbol)
    *s = '\0';
  --s;
  while (*s == ' ')
    *s-- = '\0';
  return symbol;
}

template <typename SymbolType, std::size_t N>
SymbolType &copySymbolDot(SymbolType &symbol, const char (&src)[N]) {
  const char* s = src;
  char* d = symbol;
  for (int i = 0; i < std::min(sizeof symbol, N); i++) {
    switch (*s) {
    case ' ' :
      *d++ = '\0';
      s++;
      break;
    case '.' :
      s++;
      break;
    default:
      *d++ = *s++;
    }
  }
  return symbol;
}


#endif

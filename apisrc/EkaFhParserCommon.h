#ifndef _EKA_FH_PARSER_COMMON_H
#define _EKA_FH_PARSER_COMMON_H

#include <algorithm>
#include <string_view>
#include <charconv>
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

template<size_t N>
constexpr std::string_view viewOfNulTermBuffer(const char (&buf)[N]) {
  std::string_view view(buf, N);
  auto trim_pos = view.find('\0');
  if (trim_pos != view.npos) {
    view.remove_suffix(view.size() - trim_pos);
  }
  return view;
}

template<class T>
constexpr unsigned MaxStrLen();

template<> constexpr unsigned MaxStrLen<uint8_t>() { return 3; }
template<> constexpr unsigned MaxStrLen<int8_t>() { return 4; }
template<> constexpr unsigned MaxStrLen<uint16_t>() { return 5; }
template<> constexpr unsigned MaxStrLen<int16_t>() { return 6; }
template<> constexpr unsigned MaxStrLen<uint32_t>() { return 10; }
template<> constexpr unsigned MaxStrLen<int32_t>() { return 11; }
template<> constexpr unsigned MaxStrLen<uint64_t>() { return 20; }
template<> constexpr unsigned MaxStrLen<int64_t>() { return 20; }

template <typename NumType, std::size_t N>
constexpr void numToStrBuf(char (&buf)[N], const NumType num) {
  static_assert(N > MaxStrLen<NumType>());
  char* const start = &*buf;
  char* const end = start + N - 1;
  *std::to_chars(start, end, num).ptr = '\0';
}

#endif

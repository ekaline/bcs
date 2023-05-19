#ifndef _EKA_FH_STR_UTILITIES_H
#define _EKA_FH_STR_UTILITIES_H

#include <algorithm>
#include <charconv>
#include <cstring>
#include <string_view>

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

// No null terminator, returns length
template <typename NumType, std::size_t N>
constexpr size_t numToStrView(char (&buf)[N], const NumType num) {
  static_assert(N >= MaxStrLen<NumType>());
  char* const start = &*buf;
  char* const end = start + N;
  char* const newEnd = std::to_chars(start, end, num).ptr;
  return newEnd - start;
}

// No whitespace allowed. Returns true iff the full string was parsed.
// Otherwise, returns false and does not modify `out`.
template <typename NumType>
constexpr bool parseFullStrToNum(const char *const str, const std::size_t len, NumType *out, int base = 10) {
  if (!out) return false;
  const char *const end = str + len;
  auto [p, ec] = std::from_chars(str, end, *out, base);
  return p == end && !int(ec);
}

template <typename NumType>
constexpr bool parseFullStrToNum(const char *const cStr, NumType *out, int base = 10) {
  return parseFullStrToNum(cStr, std::strlen(cStr), out, base);
}

#endif //_EKA_FH_STR_UTILITIES_H

#ifndef _EKA_FH_STR_UTILITIES_H
#define _EKA_FH_STR_UTILITIES_H

#include <algorithm>
#include <charconv>
#include <cstring>
#include <string_view>

template<std::size_t N>
bool tryCopyCString(char (&buf)[N], const char* source) {
  static_assert(N != 0, "copyCStringNoPad cannot copy into 0-size buffer");
  const std::size_t charsToCopy = std::strlen(source) + 1;
  if (charsToCopy > N) return false;
  std::memcpy(buf, source, charsToCopy);
  return true;
}

template<std::size_t N>
bool tryCopyPaddedString(char (&buf)[N], const char* source, char padding = '\0') {
  const std::size_t charsToCopy = std::strlen(source);
  if (charsToCopy > N) return false;
  std::memcpy(buf, source, charsToCopy);
  std::memset(buf + charsToCopy, padding, N - charsToCopy);
  return true;
}

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

// Max num of type that can fit in unterminated buf of size
template <typename NumType, std::size_t N>
constexpr NumType maxNumToStrView() {
  static_assert(N > 0);
  if constexpr(MaxStrLen<NumType>() <= N) {
    return std::numeric_limits<NumType>::max();
  } else {
    NumType max = 0;
    for (std::size_t i = 0; i < N; i++) {
      max = (max * 10) + 9;
    }
    return max;
  }
}

// Max num of type that can fit in null-terminated buf of size
template <typename NumType, std::size_t N>
constexpr NumType maxNumToStrBuf() {
  static_assert(N > 1);
  return maxNumToStrView<NumType, N - 1>();
}

template <typename NumType, std::size_t N>
constexpr void numToStrBuf(char (&buf)[N], const NumType num) {
  static_assert(N > MaxStrLen<NumType>());
  char* const start = &*buf;
  char* const end = start + N - 1;
  *std::to_chars(start, end, num).ptr = '\0';
}

template <typename NumType, std::size_t N>
constexpr bool tryNumToStrBuf(char (&buf)[N], const NumType num) {
  constexpr NumType max = maxNumToStrBuf<NumType, N>();
  if (num > max) return false;
  char* const start = &*buf;
  char* const end = start + N - 1;
  *std::to_chars(start, end, num).ptr = '\0';
  return true;
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

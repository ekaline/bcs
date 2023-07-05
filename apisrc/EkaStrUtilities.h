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

template<typename T>
struct ToCharsLimits {
  static_assert(std::numeric_limits<T>::is_integer);

  static constexpr unsigned CharsToRepr(T value) {
    unsigned n = (value < 0) ? 1 : 0;
    do {
      value /= 10;
      n++;
    } while (value);
    return n;
  }

  static constexpr T MaxValueReprIn(std::size_t numChars) {
    if (numChars <= 0) return MinValue;

    if (numChars >= MaxValueChars) {
      return MaxValue;
    } else {
      T max = 0;
      for (std::size_t i = 0; i < numChars; i++) {
        max = (max * 10) + 9;
      }
      return max;
    }
  }

  static constexpr unsigned MinValueReprIn(std::size_t numChars) {
    if (numChars <= 0) return MaxValue;

    if (std::numeric_limits<T>::is_signed && numChars >= 2) {
      if (numChars >= MinValueChars) {
        return MinValue;
      } else {
        T max = 0;
        for (std::size_t i = 1; i < numChars; i++) {
          max = (max * 10) - 9;
        }
        return max;
      }
    } else {
      return 0;
    }
  }

  static constexpr T MaxValue = std::numeric_limits<T>::max();
  static constexpr T MinValue = std::numeric_limits<T>::min();
  static constexpr unsigned MaxValueChars = CharsToRepr(MaxValue);
  static constexpr unsigned MinValueChars = CharsToRepr(MinValue);
  static constexpr unsigned MaxChars = std::max(MaxValueChars, MinValueChars);
  static constexpr T MaxCharsValue = MaxValueChars >= MinValueChars ? MaxValue : MinValue;
};

template <typename NumType, std::size_t N>
constexpr void numToStrBuf(char (&buf)[N], const NumType num) {
  static_assert(N > ToCharsLimits<NumType>::MaxChars);
  char* const start = &*buf;
  char* const end = start + N - 1;
  *std::to_chars(start, end, num).ptr = '\0';
}

template <typename NumType, std::size_t N>
constexpr bool tryNumToStrBuf(char (&buf)[N], const NumType num) {
  static_assert(N > 0);
  constexpr NumType Min = ToCharsLimits<NumType>::MinValueReprIn(N - 1);
  constexpr NumType Max = ToCharsLimits<NumType>::MaxValueReprIn(N - 1);
  if (num < Min || num > Max) return false;
  char* const start = &*buf;
  char* const end = start + N - 1;
  *std::to_chars(start, end, num).ptr = '\0';
  return true;
}

// No null terminator, returns length
template <typename NumType, std::size_t N>
constexpr size_t numToStrView(char (&buf)[N], const NumType num) {
  static_assert(N >= ToCharsLimits<NumType>::MaxChars);
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

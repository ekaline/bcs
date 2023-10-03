#ifndef _EKA_FH_OSI_SYMBOL_H
#define _EKA_FH_OSI_SYMBOL_H

#include "ekaline/EfhMsgs.h"
#include "EkaStrUtilities.h"

struct EkaOsiSymbolData {
  static constexpr std::size_t SymbolLength = 21;

  static constexpr std::size_t RootStart = 0;
  static constexpr std::size_t RootSize = 6;

  static constexpr std::size_t ExpiryStart = 6;
  static constexpr std::size_t ExpirySize = 6;

  static constexpr std::size_t OptionTypePos = 12;

  static constexpr std::size_t StrikePriceStart = 13;
  static constexpr std::size_t StrikePriceSize = 8;

  static constexpr std::size_t StrikePriceDecimalPlaces = 3;

  uint8_t rootLength;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  EfhOptionType optionType;
  int64_t efhStrikePrice;

  constexpr uint32_t getYYYYMMDD() {
    return static_cast<uint32_t>(year) * 10000 + static_cast<uint32_t>(month) * 100 + static_cast<uint32_t>(day);
  }

  template<std::size_t N>
  constexpr void copyRoot(char (&dest)[N], const char (&symbol)[SymbolLength]) {
    static_assert(N > RootSize, "destination is too small to hold the largest root name (6 chars) + a terminator");
    std::memcpy(dest, &symbol[RootStart], rootLength);
    std::memset(dest + rootLength, '\0', N - rootLength);
  }

  constexpr bool parseFromSymbol(const char (&symbol)[SymbolLength]) {
    constexpr uint8_t MaxDaysInMonth[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    std::size_t foundRootLength;
    {
      std::size_t i = 0;
      for (; i < RootSize; i++) {
        const char c = symbol[RootStart + i];
        if (c == ' ') break;
        if (c < ' ' || c > '~') return false;
      }
      foundRootLength = i;
      for (i++; i < RootSize; i++) {
        const char c = symbol[RootStart + i];
        if (c != ' ') return false;
      }
    }
    if (foundRootLength < 1) return false;

    for (std::size_t i = ExpiryStart; i < ExpiryStart + ExpirySize; i++) {
      if (symbol[i] < '0' || symbol[i] > '9') return false;
    }
    auto expiryDigit = [&symbol](std::size_t i) {
      return symbol[ExpiryStart + i] - '0';
    };
    uint16_t y = expiryDigit(0) * 10 + expiryDigit(1) + 2000;
    uint8_t m = expiryDigit(2) * 10 + expiryDigit(3);
    uint8_t d = expiryDigit(4) * 10 + expiryDigit(5);
    if (m < 1 || m > 12 || d < 1 || d > MaxDaysInMonth[m - 1]) {
      return false;
    }

    if (symbol[OptionTypePos] != 'P' && symbol[OptionTypePos] != 'C') {
      return false;
    }

    int64_t osiStrikePrice;
    if (!parseFullStrToNum(&symbol[StrikePriceStart], StrikePriceSize, &osiStrikePrice)) {
      return false;
    }

    this->rootLength = foundRootLength;
    this->year = y;
    this->month = m;
    this->day = d;
    this->optionType = symbol[OptionTypePos] == 'C' ? EfhOptionType::kCall : EfhOptionType::kPut;
    this->efhStrikePrice = priceToEfhScale(osiStrikePrice, StrikePriceDecimalPlaces);

    return true;
  }
};

#endif //_EKA_FH_OSI_SYMBOL_H

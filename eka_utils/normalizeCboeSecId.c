#include <inttypes.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eka_macros.h"

#include "eka_hw_conf.h"

static uint64_t char2num(char c) {
  if (c >= '0' && c <= '9')
    return c - '0'; // 10
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 10; // 36
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 10 + 26; // 62
  on_error("Unexpected char \'%c\' (0x%x)", c, c);
}

static int normalizeId(uint64_t secId) {

  uint64_t res = 0;
  char c = '\0';
  uint64_t n = 0;

  c = (char)((secId >> (8 * 0)) & 0xFF);
  n = char2num(c) << (6 * 0);
  res |= n;

  c = (char)((secId >> (8 * 1)) & 0xFF);
  n = char2num(c) << (6 * 1);
  res |= n;

  c = (char)((secId >> (8 * 2)) & 0xFF);
  n = char2num(c) << (6 * 2);
  res |= n;

  c = (char)((secId >> (8 * 3)) & 0xFF);
  n = char2num(c) << (6 * 3);
  res |= n;

  c = (char)((secId >> (8 * 4)) & 0xFF);
  n = char2num(c) << (6 * 4);
  res |= n;

  return res;
}

static int getLineIdx(uint64_t normSecId) {
  return (int)normSecId & (EFC_SUBSCR_TABLE_ROWS - 1);
}

int main(int argc, char *argv[]) {
  if (argc != 2)
    on_error("No argc: %d", argc);
  auto secId = std::stoul(argv[1], 0, 0);
  auto normalized = normalizeId(secId);
  printf("secId = %ju 0x%016jx, "
         "normalized =  %d 0x%x, Line = %d\n",
         secId, secId, normalized, normalized,
         getLineIdx(normalized));
  return 0;
}

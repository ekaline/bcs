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

static char num2char(uint8_t a) {
  auto n = a;
  if (n < 10)
    return '0' + n;
  n -= 10;

  if (n < 26)
    return 'A' + n;
  n -= 26;

  if (n < 26)
    return 'a' + n;

  on_error("Invalid n = %u (a = %u)", n, a);
}

static std::string denormalize(uint64_t a) {

  std::string s = {};
  for (auto i = 0; i < 6; i++) {
    uint8_t n = (a >> (6 * i)) & 0x3f;
    s += num2char(n);
    //  TEST_LOG("n=%u (0x%x) == \'%c\'", n, n,
    //  num2char(n));
  }
  reverse(s.begin(), s.end());
  return s;
}

static uint64_t getBinSecId(std::string strId) {
  const char *secChar = strId.c_str();
  if (strlen(secChar) != 6)
    on_error("unexpected security \'%s\' len %ju != 6",
             secChar, strlen(secChar));
  char shiftedStr[8] = {};
  for (auto i = 0; i < 6; i++)
    shiftedStr[i + 2] = secChar[i];

  return be64toh(*(uint64_t *)shiftedStr);
}
static bool isAscii(char letter) {
  //  EKA_LOG("testing %d", letter);
  if ((letter >= '0' && letter <= '9') ||
      (letter >= 'a' && letter <= 'z') ||
      (letter >= 'A' && letter <= 'Z'))
    return true;
  return false;
}

static std::string cboeSecIdString(uint64_t n) {
  auto revN = be64toh(n);
  auto p = reinterpret_cast<const char *>(&revN);
  std::string res = {};
  for (auto i = 0; i < sizeof(n); i++) {
    if (isAscii(*(p + i)))
      res += *(p + i);
  }
  return res;
}

static void printUsage(const char *cmd) {
  printf(
      "USAGE: [opt] Normalize/Denormalize CBOE ScurityID "
      "-- dump HW Security Context\n"
      "\t-n <numeric representaion of SecurityID>\n"
      "\t-d <normalized representaion of SecurityID>\n"
      "\t-a <ASCII representaion of SecurityID>\n"
      "\t-h print this help\n"
      "(Pitch Only!)\n",
      cmd);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage(argv[0]);
    exit(1);
  }

  std::string asciiSecId = {};
  uint64_t binSecId = 0;
  uint64_t normSecId = 0;
  std::string denormSecId = {};

  int opt;
  while ((opt = getopt(argc, argv, ":a:d:n:h")) != -1) {
    switch (opt) {
    case 'a':
      printf("ASCII input %s\n", optarg);
      asciiSecId = std::string(optarg);
      binSecId = getBinSecId(asciiSecId);
      normSecId = normalizeId(binSecId);
      denormSecId = denormalize(normSecId);
      break;
    case 'n':
      printf("Bin input: %s\n", optarg);
      binSecId = std::stoul(optarg, 0, 0);
      normSecId = normalizeId(binSecId);
      denormSecId = denormalize(normSecId);
      asciiSecId = cboeSecIdString(binSecId);
      break;
    case 'd':
      printf("Normalized input %s\n", optarg);
      normSecId = std::stoul(optarg, 0, 0);
      denormSecId = denormalize(normSecId);
      binSecId = getBinSecId(denormSecId);
      asciiSecId = cboeSecIdString(binSecId);
      break;
    case 'h':
      printUsage(argv[0]);
      exit(1);
      break;
    case '?':
      printf("unknown option: %c\n", optopt);
      break;
    }
  }

  printf("asciiSecId = \'%s\', ", asciiSecId.c_str());
  printf("binSecId = 0x%016jx, ", binSecId);
  printf("normalized = 0x%x, ", normSecId);
  printf("denormalized = \'%s\', ", denormSecId.c_str());
  printf("line %d", getLineIdx(normSecId));
  printf("\n");

  if (asciiSecId != denormSecId)
    on_error("asciiSecId \'%s\' != denormSecId \'%s\'",
             asciiSecId.c_str(), denormSecId.c_str());
  return 0;
}

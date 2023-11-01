#ifndef _EKA_MACROS_H
#define _EKA_MACROS_H

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <errno.h>
#include <math.h>
#include <string>

#include "efh_macros.h"
#include "ekaNW.h"
#include "eka_hw_conf.h"

#include "Eka.h"

extern FILE *g_ekaLogFile;
extern EkaLogCallback g_ekaLogCB;

#ifdef _VERILOG_SIM
extern FILE *g_ekaVerilogSimFile;
#endif

class EkaDev;

#define SEC_TO_NANO 1e9
#define MILLI_TO_NANO 1e6
#define MICRO_TO_NANO 1e3

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define RESET "\x1B[0m"

#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#ifndef on_error
#ifdef EKA_LIB_RUN
#define on_error(...)                                      \
  do {                                                     \
    const int err = errno;                                 \
    fprintf(stderr,                                        \
            "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",     \
            __func__, __FILE__, __LINE__);                 \
    fprintf(stderr, __VA_ARGS__);                          \
    if (err)                                               \
      fprintf(stderr, ": %s (%d)", strerror(err), err);    \
    fprintf(stderr, "\n");                                 \
    fflush(stderr);                                        \
    g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,       \
               __LINE__, EKA_LOG_ERROR, __VA_ARGS__);      \
    std::quick_exit(1);                                    \
  } while (0)
#else
#define on_error(...)                                      \
  do {                                                     \
    const int err = errno;                                 \
    fprintf(stderr,                                        \
            "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",     \
            __func__, __FILE__, __LINE__);                 \
    fprintf(stderr, __VA_ARGS__);                          \
    if (err)                                               \
      fprintf(stderr, ": %s (%d)", strerror(err), err);    \
    fprintf(stderr, "\n");                                 \
    fflush(stderr);                                        \
    std::quick_exit(1);                                    \
  } while (0)

#define EKA_LOG(...)                                       \
  do {                                                     \
    const int err = errno;                                 \
    fprintf(stdout, "%s@%s:%d: ", __func__, __FILE__,      \
            __LINE__);                                     \
    fprintf(stdout, __VA_ARGS__);                          \
    fprintf(stdout, "\n");                                 \
  } while (0)

#define EKA_ERROR(...)                                     \
  do {                                                     \
    const int err = errno;                                 \
    fprintf(stdout, "ERROR: %s@%s:%d: ", __func__,         \
            __FILE__, __LINE__);                           \
    fprintf(stdout, __VA_ARGS__);                          \
    fprintf(stdout, "\n");                                 \
  } while (0)
#endif
#endif

#ifndef on_error_stderr
#define on_error_stderr(...)                               \
  do {                                                     \
    const int err = errno;                                 \
    fprintf(stderr,                                        \
            "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",     \
            __func__, __FILE__, __LINE__);                 \
    fprintf(stderr, __VA_ARGS__);                          \
    if (err)                                               \
      fprintf(stderr, ": %s (%d)", strerror(err), err);    \
    fprintf(stderr, "\n");                                 \
    fflush(stderr);                                        \
    std::quick_exit(1);                                    \
  } while (0)
#endif

#define test_error(...)                                    \
  do {                                                     \
    const int err = errno;                                 \
    fprintf(stderr,                                        \
            RED "TEST FATAL ERROR: %s@%s:%d: ", __func__,  \
            __FILE__, __LINE__);                           \
    fprintf(stderr, __VA_ARGS__);                          \
    if (err)                                               \
      fprintf(stderr, ": %s (%d)", strerror(err), err);    \
    fprintf(stderr, "\n" RESET);                           \
    fflush(stdout);                                        \
    fflush(stderr);                                        \
    return -1;                                             \
  } while (0)

#define on_warning(...)                                    \
  do {                                                     \
    const int err = errno;                                 \
    fprintf(stderr, "EKALINE API LIB WARNING: %s@%s:%d: ", \
            __func__, __FILE__, __LINE__);                 \
    fprintf(stderr, __VA_ARGS__);                          \
    if (err)                                               \
      fprintf(stderr, ": %s (%d)", strerror(err), err);    \
    fprintf(stderr, "\n");                                 \
    fflush(stdout);                                        \
    fflush(stderr);                                        \
  } while (0)

#define EKA_TEST(...)                                      \
  do {                                                     \
    fprintf(stderr, "%s@%s:%d: ", __func__, __FILE__,      \
            __LINE__);                                     \
    fprintf(stderr, __VA_ARGS__);                          \
    fprintf(stderr, "\n");                                 \
    fflush(stdout);                                        \
    fflush(stderr);                                        \
  } while (0)

#if !defined(LOG_TRACE)
#define EKA_LOG_ERROR 3
#define EKA_LOG_WARNING 4
#define EKA_LOG_NOTICE 5
#define EKA_LOG_INFO 6
#define EKA_LOG_DEBUG 7
#define EKA_LOG_TRACE 8
#endif

#ifdef EKA_LIB_RUN
#define EKA_ERROR(...)                                     \
  g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,         \
             __LINE__, EKA_LOG_ERROR, __VA_ARGS__)
#endif

#define EKA_WARN(...)                                      \
  g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,         \
             __LINE__, EKA_LOG_WARNING, __VA_ARGS__)

#define EKA_NOTICE(...)                                    \
  g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,         \
             __LINE__, EKA_LOG_NOTICE, __VA_ARGS__)

#define EKA_INFO(...)                                      \
  g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,         \
             __LINE__, EKA_LOG_INFO, __VA_ARGS__)

#ifdef EKA_LIB_RUN
#define EKA_LOG(...)                                       \
  g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,         \
             __LINE__, EKA_LOG_INFO, __VA_ARGS__)
#endif

#define EKA_DEBUG(...)                                     \
  g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,         \
             __LINE__, EKA_LOG_DEBUG, __VA_ARGS__)

#define EKA_TRACE(...)                                     \
  g_ekaLogCB(g_ekaLogFile, __FUNCTION__, __FILE__,         \
             __LINE__, EKA_LOG_TRACE, __VA_ARGS__)

#define TEST_LOG_STDERR(...)                               \
  {                                                        \
    fprintf(stderr, "%s@%s:%d: ", __func__, __FILE__,      \
            __LINE__);                                     \
    fprintf(stderr, __VA_ARGS__);                          \
    fprintf(stderr, "\n");                                 \
  }
#define TEST_LOG(...)                                      \
  {                                                        \
    printf("%s@%s:%d: ", __func__, __FILE__, __LINE__);    \
    printf(__VA_ARGS__);                                   \
    printf("\n");                                          \
  }

#define TEST_FAILED(...)                                   \
  {                                                        \
    printf(RED "");                                        \
    printf(__VA_ARGS__);                                   \
    printf(RESET "\n");                                    \
  }
#define TEST_PASSED(...)                                   \
  {                                                        \
    printf(GRN "");                                        \
    printf(__VA_ARGS__);                                   \
    printf(RESET "\n");                                    \
  }

// #define EKA_DEC_POINTS_10000(x) (((bool)(x % 10 != 0)) ?
// 4 : ((bool)(x % 100 != 0)) ? 3 : ((bool)(x % 1000 != 0))
// ? 2 : ((bool)(x % 10000 != 0)) ? 1 : 0)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define EKA_DEC_POINTS_10000(x)                            \
  MAX(1, (((bool)(x % 10 != 0))      ? 4                   \
          : ((bool)(x % 100 != 0))   ? 3                   \
          : ((bool)(x % 1000 != 0))  ? 2                   \
          : ((bool)(x % 10000 != 0)) ? 1                   \
                                     : 0))

#define EKA_IP2STR(x)                                      \
  ((std::to_string((x >> 0) & 0xFF) + '.' +                \
    std::to_string((x >> 8) & 0xFF) + '.' +                \
    std::to_string((x >> 16) & 0xFF) + '.' +               \
    std::to_string((x >> 24) & 0xFF))                      \
       .c_str())

#define EKA_FEED2STRING(x)                                 \
  x == 0    ? "Unconfigured"                               \
  : x == 1  ? "NOM"                                        \
  : x == 2  ? "Pitch P4"                                   \
  : x == 12 ? "QED"                                        \
  : x == 13 ? "Itch FSweep"                                \
  : x == 14 ? "News"                                       \
  : x == 15 ? "CME FCancel"                                \
            : "Unknown"

#define EKA_STRAT2STRING(x)                                \
  x == 0    ? "Unconfigured"                               \
  : x == 2  ? "P4"                                         \
  : x == 12 ? "QED"                                        \
  : x == 13 ? "Fast Sweep"                                 \
  : x == 14 ? "Network News"                               \
  : x == 15 ? "Fast Cancel"                                \
            : "Unknown"

#define EKA_NIBBLE2CHAR(x)                                 \
  x == 0    ? "0"                                          \
  : x == 1  ? "1"                                          \
  : x == 2  ? "2"                                          \
  : x == 3  ? "3"                                          \
  : x == 4  ? "4"                                          \
  : x == 5  ? "5"                                          \
  : x == 6  ? "6"                                          \
  : x == 7  ? "7"                                          \
  : x == 8  ? "8"                                          \
  : x == 9  ? "9"                                          \
  : x == 10 ? "A"                                          \
  : x == 11 ? "B"                                          \
  : x == 12 ? "C"                                          \
  : x == 13 ? "D"                                          \
  : x == 14 ? "E"                                          \
  : x == 15 ? "F"                                          \
            : "X"

#define EKA_BYTE2CHAR(x)                                   \
  (std::string(EKA_NIBBLE2CHAR((uint8_t)(x / 16))) +       \
   std::string(EKA_NIBBLE2CHAR((uint8_t)(x % 16))))

#define EKA_MAC2STR(x)                                     \
  ((EKA_BYTE2CHAR(((uint8_t *)x)[0]) + ':' +               \
    EKA_BYTE2CHAR(((uint8_t *)x)[1]) + ':' +               \
    EKA_BYTE2CHAR(((uint8_t *)x)[2]) + ':' +               \
    EKA_BYTE2CHAR(((uint8_t *)x)[3]) + ':' +               \
    EKA_BYTE2CHAR(((uint8_t *)x)[4]) + ':' +               \
    EKA_BYTE2CHAR(((uint8_t *)x)[5]))                      \
       .c_str())

#define EKA_NATIVE_MAC(x)                                  \
  (((uint8_t)x[0] == 0x00) && ((uint8_t)x[1] == 0x21) &&   \
   ((uint8_t)x[2] == 0xb2))

template <class T>
inline T roundUp(T numToRound, T multiple) {
  if (multiple == 0)
    return numToRound;
  T remainder = (T)(numToRound % multiple);
  if (remainder == 0)
    return numToRound;
  return numToRound + multiple - remainder;
}

template <class T> inline T roundUp64(T x) {
  return (x + 63) & ~63;
}

template <class T> inline T roundUp8(T x) {
  return (x + 7) & ~7;
}

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define RESET "\x1B[0m"

inline void hexDump(const char *desc, const void *addr,
                    int len, std::FILE *file = stdout) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char *)addr;
  if (desc != NULL)
    std::fprintf(file, "%s:\n", desc);
  if (len == 0) {
    std::fprintf(file, "  ZERO LENGTH\n");
    return;
  }
  if (len < 0) {
    std::fprintf(file, "  NEGATIVE LENGTH: %i\n", len);
    return;
  }
  for (i = 0; i < len; i++) {
    if ((i % 16) == 0) {
      if (i != 0)
        std::fprintf(file, "  %s\n", buff);
      std::fprintf(file, "  %04x ", i);
    }
    std::fprintf(file, " %02x", pc[i]);
    if ((pc[i] < 0x20) || (pc[i] > 0x7e))
      buff[i % 16] = '.';
    else
      buff[i % 16] = pc[i];
    buff[(i % 16) + 1] = '\0';
  }
  while ((i % 16) != 0) {
    std::fprintf(file, "   ");
    i++;
  }
  std::fprintf(file, "  %s\n", buff);
}
/* -------------------------------------------------------
 */
inline void hexDump2str(const char *desc, const void *addr,
                        int len, char *hexBufStr,
                        const size_t hexBufStrSize) {
  if (std::FILE *const hexBufFile =
          fmemopen(hexBufStr, hexBufStrSize, "w")) {
    hexDump(desc, addr, len, hexBufFile);
    (void)std::fwrite("\0", 1, 1, hexBufFile);
    (void)std::fclose(hexBufFile);
  } else {
    std::snprintf(hexBufStr, hexBufStrSize,
                  "fmemopen error: %s (%d)",
                  strerror(errno), errno);
  }
}

/* -------------------------------------------------------
 */

inline int decPoints(int64_t a, int scaleF) {
  if (a % 10 != 0)
    return (int)log10(scaleF);
  for (int i = 100; i <= (int)scaleF / 10; i *= 10) {
    if (a % i != 0)
      return (int)log10(i) - 1;
  }
  return 1;
}
/* -------------------------------------------------------
 */

inline std::string ts_ns2str(uint64_t ts) {
  char dst[32] = {};
  uint ns = ts % 1000;
  uint64_t res = (ts - ns) / 1000;
  uint us = res % 1000;
  res = (res - us) / 1000;
  uint ms = res % 1000;
  res = (res - ms) / 1000;
  uint s = res % 60;
  res = (res - s) / 60;
  uint m = res % 60;
  res = (res - m) / 60;
  uint h = res % 24;
  sprintf(dst, "%02d:%02d:%02d.%03d.%03d.%03d", h, m, s, ms,
          us, ns);
  return std::string(dst);
}

/* -------------------------------------------------------
 */

// inline bool isStaleData(uint64_t exchTS,
// 												uint64_t
// StaleDataNanosecThreshold) { 		uint64_t
// exchNs = exchTS % 1'000'000'000;

// 		auto now =
// std::chrono::high_resolution_clock::now();

// 		uint64_t sampleTime =
// std::chrono::duration_cast<
// std::chrono::nanoseconds>
// 			(now.time_since_epoch()).count();

// 		uint64_t sampleNs = sampleTime %
// 1'000'000'000;

// 		if (sampleNs < exchNs ||
// 				 sampleNs - exchNs >
// StaleDataNanosecThreshold) {

// 			EKA_WARN("%s:%u: Stale data: "
// 							 "exchNs=
// %s
// (%ju) sampleNs= %s (%ju)",
// EKA_EXCH_DECODE(exch),id,
// 							 ts_ns2str(exchTS).c_str(),exchNs,
// 							 ts_ns2str(sampleTime).c_str(),sampleNs);
// 			return true;
// 		}

// 		return false;
// 	}
/* -------------------------------------------------------
 */

#ifdef EKA_FPGA_FREQUENCY
#define _FREQUENCY EKA_FPGA_FREQUENCY
#else
#define _FREQUENCY (161.132828125)
#endif

inline uint64_t
getFpgaTimeCycles() { // ignores Application - PCIe - FPGA
                      // latency
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); //
  uint64_t current_time_ns =
      ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 +
       (uint64_t)(t.tv_nsec));
  uint64_t current_time_cycles =
      (current_time_ns * (_FREQUENCY / 1000.0));
  /* eka_write(dev,FPGA_RT_CNTR,current_time_cycles); */
  /* char t_str[64] = {}; */
  /* str_time_from_nano(current_time_ns,t_str); */
  /* EKA_LOG("setting HW time to %s",t_str); */
  return current_time_cycles;
}
/* -------------------------------------------------------
 */

inline auto getPktTimestampCycles(const uint8_t *pPayload) {
  auto p{pPayload - sizeof(EkaUdpHdr) - sizeof(EkaIpHdr) -
         sizeof(EkaEthHdr)};

  uint8_t t[8] = {};
  t[0] = p[6 + 1];
  t[1] = p[6 + 0];
  t[2] = p[6 + 5];
  t[3] = p[6 + 4];
  t[4] = p[6 + 3];
  t[5] = p[6 + 2];
  t[6] = 0; // p[19];
  t[7] = 0; // p[18];

  return *(uint64_t *)t;
}

/* -------------------------------------------------------
 */
inline void printFpgaTime(char *dst, size_t dstSize,
                          uint64_t timeStampCycles) {
  uint64_t epcoh_seconds =
      timeStampCycles * (1000.0 / _FREQUENCY) / 1e9;
  auto raw_time{static_cast<time_t>(epcoh_seconds)};
  struct tm *timeinfo = localtime(&raw_time);
  //  strftime(dst, dstSize, "%a, %d %b %Y, %X", timeinfo);
  strftime(dst, dstSize, "%X", timeinfo);
  return;
}
/* -------------------------------------------------------
 */

inline std::chrono::system_clock::time_point
systemClockAtMidnight() {
  auto now = std::chrono::system_clock::now();

  time_t tnow = std::chrono::system_clock::to_time_t(now);
  tm *date = std::localtime(&tnow);
  date->tm_hour = 0;
  date->tm_min = 0;
  date->tm_sec = 0;
  return std::chrono::system_clock::from_time_t(
      std::mktime(date));
}
/* -------------------------------------------------------
 */

inline uint64_t nsSinceMidnight() {
  auto now = std::chrono::system_clock::now();

  time_t tnow = std::chrono::system_clock::to_time_t(now);
  tm *date = std::localtime(&tnow);
  date->tm_hour = 0;
  date->tm_min = 0;
  date->tm_sec = 0;
  auto midnight = std::chrono::system_clock::from_time_t(
      std::mktime(date));

  return std::chrono::duration_cast<
             std::chrono::duration<uint64_t, std::nano>>(
             now - midnight)
      .count();
}
/* -------------------------------------------------------
 */
static inline bool isTradingHours(int startHour,
                                  int startMinute,
                                  int endHour,
                                  int endMinute) {
  time_t rawtime;
  time(&rawtime);
  struct tm *ct = localtime(&rawtime);
  if ((ct->tm_hour > startHour ||
       (ct->tm_hour == startHour &&
        ct->tm_min > startMinute)) &&
      (ct->tm_hour < endHour || (ct->tm_hour == endHour &&
                                 ct->tm_min < endMinute))) {
    return true;
  }
  return false;
}

#endif

#ifndef _EKA_MACROS_H
#define _EKA_MACROS_H

#include <string>
#include <algorithm>
#include <errno.h>

#include "ekaNW.h"
#include "efh_macros.h"

class EkaDev;

#define SEC_TO_NANO 1e9
#define MILLI_TO_NANO 1e6
#define MICRO_TO_NANO 1e3

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#ifndef on_error
#define on_error(...) do { const int err = errno; fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); if (err) fprintf(stderr, ": %s (%d)", strerror(err), err); fprintf(stderr, "\n"); fflush(stdout); fflush(stderr); exit(1); } while(0)
#endif

#define test_error(...) do { const int err = errno; fprintf(stderr,RED "TEST FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); if (err) fprintf(stderr, ": %s (%d)", strerror(err), err); fprintf(stderr, "\n" RESET); fflush(stdout); fflush(stderr); return -1; } while(0)


#define on_warning(...) do { const int err = errno; fprintf(stderr, "EKALINE API LIB WARNING: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); if (err) fprintf(stderr, ": %s (%d)", strerror(err), err); fprintf(stderr,"\n"); fflush(stdout); fflush(stderr); } while(0)

#if !defined(LOG_TRACE)
#define EKA_LOG_WARNING 4
#define EKA_LOG_NOTICE 5
#define EKA_LOG_INFO 6
#define EKA_LOG_DEBUG 7
#define EKA_LOG_TRACE 8
#endif

#define EKA_WARN(...)   dev->logCB(dev->logCtx, __FUNCTION__, __FILE__, __LINE__, EKA_LOG_WARNING, __VA_ARGS__)
#define EKA_NOTICE(...)   dev->logCB(dev->logCtx, __FUNCTION__, __FILE__, __LINE__, EKA_LOG_NOTICE, __VA_ARGS__)
#define EKA_INFO(...)   dev->logCB(dev->logCtx, __FUNCTION__, __FILE__, __LINE__, EKA_LOG_INFO, __VA_ARGS__)
#define EKA_LOG(...)    dev->logCB(dev->logCtx, __FUNCTION__, __FILE__, __LINE__, EKA_LOG_INFO, __VA_ARGS__)
#define EKA_DEBUG(...)  dev->logCB(dev->logCtx, __FUNCTION__, __FILE__, __LINE__, EKA_LOG_DEBUG, __VA_ARGS__)
#define EKA_TRACE(...)  dev->logCB(dev->logCtx, __FUNCTION__, __FILE__, __LINE__, EKA_LOG_TRACE, __VA_ARGS__)

#define TEST_LOG_STDERR(...) { fprintf(stderr, "%s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n"); }
#define TEST_LOG(...) { printf("%s@%s:%d: ",__func__,__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); }

#define TEST_FAILED(...) { printf(RED "");printf( __VA_ARGS__); printf(RESET "\n");}
#define TEST_PASSED(...) { printf(GRN "");printf( __VA_ARGS__); printf(RESET "\n");}

//#define EKA_DEC_POINTS_10000(x) (((bool)(x % 10 != 0)) ? 4 : ((bool)(x % 100 != 0)) ? 3 : ((bool)(x % 1000 != 0)) ? 2 : ((bool)(x % 10000 != 0)) ? 1 : 0) 
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define EKA_DEC_POINTS_10000(x) MAX(1,(((bool)(x % 10 != 0)) ? 4 : ((bool)(x % 100 != 0)) ? 3 : ((bool)(x % 1000 != 0)) ? 2 : ((bool)(x % 10000 != 0)) ? 1 : 0))

#define EKA_IP2STR(x)  ((std::to_string((x >> 0) & 0xFF) + '.' + std::to_string((x >> 8) & 0xFF) + '.' + std::to_string((x >> 16) & 0xFF) + '.' + std::to_string((x >> 24) & 0xFF)).c_str())


#define EKA_FEED2STRING(x) \
  x == 0 ? "NOM" : \
    x == 1 ? "MIAX" : \
    x == 2 ? "PHLX" : \
    x == 3 ? "ISE/GEMX/MRX" : \
    x == 4 ? "BZX/C2/EDGEX" : \
    "GENERAL FH/EPM"

#define EKA_NIBBLE2CHAR(x) \
  x == 0 ? "0" : \
  x == 1 ? "1" : \
  x == 2 ? "2" : \
  x == 3 ? "3" : \
  x == 4 ? "4" : \
  x == 5 ? "5" : \
  x == 6 ? "6" : \
  x == 7 ? "7" : \
  x == 8 ? "8" : \
  x == 9 ? "9" : \
  x == 10 ? "A" : \
  x == 11 ? "B" : \
  x == 12 ? "C" : \
  x == 13 ? "D" : \
  x == 14 ? "E" : \
  x == 15 ? "F" : \
  "X" 

#define EKA_BYTE2CHAR(x) (std::string(EKA_NIBBLE2CHAR((uint8_t)(x / 16))) + std::string(EKA_NIBBLE2CHAR((uint8_t)(x % 16))))

#define EKA_MAC2STR(x) (( EKA_BYTE2CHAR(((uint8_t*)x)[0]) + ':' +  EKA_BYTE2CHAR(((uint8_t*)x)[1]) + ':' +  EKA_BYTE2CHAR(((uint8_t*)x)[2]) + ':' +  EKA_BYTE2CHAR(((uint8_t*)x)[3]) + ':' +  EKA_BYTE2CHAR(((uint8_t*)x)[4]) + ':' +  EKA_BYTE2CHAR(((uint8_t*)x)[5])).c_str())

#define EKA_NATIVE_MAC(x) (((uint8_t)x[0] == 0x00) && ((uint8_t)x[1] == 0x21) && ((uint8_t)x[2] == 0xb2))

template <class T> inline T roundUp(T numToRound, T multiple) {
  if (multiple == 0) return numToRound;
  T remainder = (T) (numToRound % multiple);
  if (remainder == 0) return numToRound;
  return numToRound + multiple - remainder;
}

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"


#ifndef _hexDump_defined_
#define _hexDump_defined_
inline void hexDump (const char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char*)addr;
  if (desc != NULL) printf ("%s:\n", desc);
  if (len == 0) { printf("  ZERO LENGTH\n"); return; }
  if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
  for (i = 0; i < len; i++) {
    if ((i % 16) == 0) {
      if (i != 0) printf ("  %s\n", buff);
      printf ("  %04x ", i);
    }
    printf (" %02x", pc[i]);
    if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
    else buff[i % 16] = pc[i];
    buff[(i % 16) + 1] = '\0';
  }
  while ((i % 16) != 0) { printf ("   "); i++; }
  printf ("  %s\n", buff);
}

inline void hexDump (const char *desc, const void *addr, int len) {
  hexDump(desc,(uint8_t*)addr,len);
}
#endif

#endif

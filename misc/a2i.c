#include <time.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>
#include <x86intrin.h>

#define on_error(...) do { const int err = errno; fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); if (err) fprintf(stderr, ": %s (%d)", strerror(err), err); fprintf(stderr, "\n"); fflush(stdout); fflush(stderr); exit(1); } while(0)

#define _INLINE_ 0

static const uint64_t kStrLen = 8;

constexpr int32_t 
staticPow (uint32_t unUsedPowerBase, const uint64_t _kIdx) {
  switch (_kIdx) {
  case 0: return 1e0;
  case 1: return 1e1;
  case 2: return 1e2;
  case 3: return 1e3;
  case 4: return 1e4;
  case 5: return 1e5;
  case 6: return 1e6;
  case 7: return 1e7;
  default:
    return 0;
  }
}

/* */
template< uint64_t _kOffset, uint64_t _kNumUsed, bool _kInvertOrder = true >
constexpr
auto
genIntScaleVector( ) {
    constexpr uint64_t kNumElems = kStrLen;
    /* */
    auto fn0 =
        [ & ]< uint64_t _kIdx >( std::integral_constant< uint64_t, _kIdx > ) -> int32_t {
            if constexpr( _kIdx >= _kNumUsed ) {
                return 0;
            } else {
                return staticPow( 10, _kIdx );
            }
        };
    /* */
    auto invIdx =
        [ ]< uint64_t _kIdx >( std::integral_constant< uint64_t, _kIdx > ) {
            if constexpr( _kInvertOrder ) {
                return std::integral_constant< uint64_t, ( kNumElems - _kIdx - 1 + _kOffset ) >{ };
            } else {
                return std::integral_constant< uint64_t, ( _kIdx + _kOffset ) >{ };
            }
        };
    /* */
    auto fn =
        [ & ]< size_t... _kIdxs >( std::index_sequence< _kIdxs... > ) {
            return _mm256_set_epi32( fn0( invIdx( std::integral_constant< uint64_t, _kIdxs >{ } ) )... );
        };
    return fn( std::make_integer_sequence< uint64_t, kNumElems >{ } );
}
inline
int32_t
getNumField8Avx1( const char* s ) {
    __m128i chars        = _mm_loadu_si128( reinterpret_cast< const __m128i* >( s ) );
    __m128i charsAsBytes =
        [ & ]( ) {
            constexpr __m128i kChar0Splat = {
                0x3030303030303030,
                0x3030303030303030
            };
            return _mm_subs_epi8( chars, kChar0Splat );
        }( );
    __m256i bytesAsInts = _mm256_cvtepu8_epi32( charsAsBytes );
    /* It wont let me initialize with constexpr, but I can see that in O3, it will initialize at compile time. */
    __m256i kScaleVector = genIntScaleVector< 0, 8, false >( );
    __m256i scaledInts = _mm256_mullo_epi32( bytesAsInts, kScaleVector );
    __m256i sum0      = _mm256_hadd_epi32( scaledInts, scaledInts );
    __m128i sum0Hi128 = _mm256_extracti128_si256( sum0, 1 );
    __m256i sum0Hi    = _mm256_castsi128_si256( sum0Hi128 );
    __m256i sum1      = _mm256_add_epi32( sum0, sum0Hi );
    __m256i sum2      = _mm256_hadd_epi32( sum1, sum1 );
    int32_t result = _mm256_extract_epi32( sum2, 0 );
    return result;
}

#if _INLINE_
inline
#endif
int32_t getUnrolledLoop8(const char* s) {
  return 
    (s[0] - '0') * static_cast<int32_t>(1e7) +
    (s[1] - '0') * static_cast<int32_t>(1e6) +
    (s[2] - '0') * static_cast<int32_t>(1e5) +
    (s[3] - '0') * static_cast<int32_t>(1e4) +
    (s[4] - '0') * static_cast<int32_t>(1e3) +
    (s[5] - '0') * static_cast<int32_t>(1e2) +
    (s[6] - '0') * static_cast<int32_t>(10) +
    (s[7] - '0');
}

inline int32_t digit_0_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 10000000;
    case '2' : return 20000000;
    case '3' : return 30000000;
    case '4' : return 40000000;
    case '5' : return 50000000;
    case '6' : return 60000000;
    case '7' : return 70000000;
    case '8' : return 80000000;
    case '9' : return 90000000;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

inline int32_t digit_1_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 1000000;
    case '2' : return 2000000;
    case '3' : return 3000000;
    case '4' : return 4000000;
    case '5' : return 5000000;
    case '6' : return 6000000;
    case '7' : return 7000000;
    case '8' : return 8000000;
    case '9' : return 9000000;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

inline int32_t digit_2_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 100000;
    case '2' : return 200000;
    case '3' : return 300000;
    case '4' : return 400000;
    case '5' : return 500000;
    case '6' : return 600000;
    case '7' : return 700000;
    case '8' : return 800000;
    case '9' : return 900000;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

inline int32_t digit_3_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 10000;
    case '2' : return 20000;
    case '3' : return 30000;
    case '4' : return 40000;
    case '5' : return 50000;
    case '6' : return 60000;
    case '7' : return 70000;
    case '8' : return 80000;
    case '9' : return 90000;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

inline int32_t digit_4_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 1000;
    case '2' : return 2000;
    case '3' : return 3000;
    case '4' : return 4000;
    case '5' : return 5000;
    case '6' : return 6000;
    case '7' : return 7000;
    case '8' : return 8000;
    case '9' : return 9000;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

inline int32_t digit_5_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 100;
    case '2' : return 200;
    case '3' : return 300;
    case '4' : return 400;
    case '5' : return 500;
    case '6' : return 600;
    case '7' : return 700;
    case '8' : return 800;
    case '9' : return 900;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

inline int32_t digit_6_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 10;
    case '2' : return 20;
    case '3' : return 30;
    case '4' : return 40;
    case '5' : return 50;
    case '6' : return 60;
    case '7' : return 70;
    case '8' : return 80;
    case '9' : return 90;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

inline int32_t digit_7_string8(const char c) {
    switch (c) {
    case '0' : return 0;
    case '1' : return 1;
    case '2' : return 2;
    case '3' : return 3;
    case '4' : return 4;
    case '5' : return 5;
    case '6' : return 6;
    case '7' : return 7;
    case '8' : return 8;
    case '9' : return 9;
    default  : return 0;
    //    default: on_error("unexpected char \'%c\'",c);
    }
}

#if _INLINE_
inline
#endif
int32_t getUnrolledLoop8_predefined(const char* s) {
    return 
    digit_0_string8(s[0]) +
    digit_1_string8(s[1]) +
    digit_2_string8(s[2]) +
    digit_3_string8(s[3]) +
    digit_4_string8(s[4]) +
    digit_5_string8(s[5]) +
    digit_6_string8(s[6]) +
    digit_7_string8(s[7]);
}

#if _INLINE_
inline
#endif
int32_t getLoop8(const char* s) {
  int32_t acc = 0;
  for (auto i = 0; i < 8; i++) {
    int digit = s[i] - '0';
    acc = (10 * acc) + digit;
  }
  return acc;
}

typedef int32_t (*A2I_F) (const char* s);

template <const size_t TestIter, const size_t StringLen>
  class TestCtx {
 public:
  TestCtx(std::string name, A2I_F f) {
    m_name = name;
    m_f    = f;
  }

  void run() {
    for (auto i = 0; i < TestIter; i++) {
      char str[StringLen + 1]{};
	
      auto num = static_cast<const uint64_t>(rand()) % staticPow(10,StringLen-1);
      sprintf(str,"%0*ju",StringLen,num);
      auto start = std::chrono::high_resolution_clock::now();
      auto res = m_f(str);
      auto end = std::chrono::high_resolution_clock::now();

      if (res != num) on_error("num %ju (\'%s\')!= res %ju",num,str,res);
    	
      m_arr.at(i) = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>
					  (end - start).count());
    }

  }
  void printResults() {
    std::sort(m_arr.begin(),m_arr.end());
    printf("%s: min = %ju ns, med = %ju ns, max = %ju ns\n",m_name.c_str(),
	   m_arr.at(0),m_arr.at(TestIter/2),m_arr.at(TestIter - 1));
  }
  /* ------------------------------------ */
 private:
  std::string           m_name;
  A2I_F                 m_f;
  std::vector<int>      m_arr = std::vector<int>(TestIter);
};
  
int main(int argc, char *argv[]) {
    static const size_t StringLen = kStrLen; // = 8
    static const size_t TestIterations = static_cast<const size_t>(1e6);

    using Test = TestCtx<TestIterations,StringLen>;

    std::vector<Test> t;

    t.push_back(Test("AVX2_8",getNumField8Avx1));
    t.push_back(Test("Loop8",getLoop8));
    t.push_back(Test("UnrolledLoop8",getUnrolledLoop8));
    t.push_back(Test("UnrolledLoop8_predefined",getUnrolledLoop8_predefined));

    for (auto& tst : t) {
	tst.run();
	tst.printResults();
    }
    return 0;
}

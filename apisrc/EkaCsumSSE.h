#ifndef _EKA_CSUM_SSE_H_
#define _EKA_CSUM_SSE_H_


#include <immintrin.h>
#include <emmintrin.h>


static __m128i xmm_be_le_swap16a;
static __m128i xmm_be_le_swap16b;
static __m128i xmm_udp_mask;

#define DECLARE_ALIGNED(_declaration, _boundary)         \
        _declaration __attribute__((aligned(_boundary)))

DECLARE_ALIGNED(const uint8_t crc_xmm_shift_tab[48], 16) = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

inline
__m128i xmm_shift_right(__m128i reg, const unsigned int num)
{
        const __m128i *p = (const __m128i *)(crc_xmm_shift_tab + 16 + num);

        return _mm_shuffle_epi8(reg, _mm_loadu_si128(p));
}


inline
__m128i xmm_shift_left(__m128i reg, const unsigned int num) {
        const __m128i *p = (const __m128i *)(crc_xmm_shift_tab + 16 - num);

        return _mm_shuffle_epi8(reg, _mm_loadu_si128(p));
}

inline
void IPChecksumInit(void) {
  /**
   * swap16a mask converts least significant 8 bytes of
   * XMM register as follows:
   * - converts 16-bit words from big endian to little endian
   * - converts 16-bit words to 32-bit words
   */
  xmm_be_le_swap16a =
    _mm_setr_epi16(0x0001, 0xffff, 0x0203, 0xffff,
		   0x0405, 0xffff, 0x0607, 0xffff);
  /**
   * swap16b mask converts most significant 8 bytes of
   * XMM register as follows:
   * - converts 16-bit words from big endian to little endian
   * - converts 16-bit words to 32-bit words
   */
  xmm_be_le_swap16b =
    _mm_setr_epi16(0x0809, 0xffff, 0x0a0b, 0xffff,
		   0x0c0d, 0xffff, 0x0e0f, 0xffff);

  /**
   * UDP mask converts 16-bit words from big endian to little endian.
   * Duplicates length field
   * Extends 16-bit words to 32-bit ones
   */
  xmm_udp_mask =
    _mm_setr_epi8(0x01, 0x00, 0xff, 0xff,
		  0x03, 0x02, 0xff, 0xff,
		  0x05, 0x04, 0xff, 0xff,  /**< do length twice */
		  0x05, 0x04, 0xff, 0xff);
}

inline
uint32_t csum_oc16_sse(const uint8_t *data, uint32_t data_len,
                       __m128i sum32a, __m128i sum32b) {
  uint32_t n = 0;
  IPChecksumInit();
#define swap16a xmm_be_le_swap16a
#define swap16b xmm_be_le_swap16b

  /**
   * If payload is big enough try to process 64 bytes
   * in one iteration.
   */
  for (n = 0; (n+64) <= data_len; n += 64) {
    /**
     * Load first 32 bytes
     * - make use of SNB enhanced load
     */
    __m128i dblock1, dblock2;

    dblock1 = _mm_loadu_si128((__m128i *)&data[n]);
    dblock2 = _mm_loadu_si128((__m128i *)&data[n + 16]);

    sum32a = _mm_add_epi32(sum32a,
			   _mm_shuffle_epi8(dblock1, swap16a));
    sum32b = _mm_add_epi32(sum32b,
			   _mm_shuffle_epi8(dblock1, swap16b));
    sum32a = _mm_add_epi32(sum32a,
			   _mm_shuffle_epi8(dblock2, swap16a));
    sum32b = _mm_add_epi32(sum32b,
			   _mm_shuffle_epi8(dblock2, swap16b));

    /**
     * Load second 32 bytes
     * - make use of SNB enhanced load
     */
    dblock1 = _mm_loadu_si128((__m128i *)&data[n + 32]);
    dblock2 = _mm_loadu_si128((__m128i *)&data[n + 48]);

    sum32a = _mm_add_epi32(sum32a,
			   _mm_shuffle_epi8(dblock1, swap16a));
    sum32b = _mm_add_epi32(sum32b,
			   _mm_shuffle_epi8(dblock1, swap16b));
    sum32a = _mm_add_epi32(sum32a,
			   _mm_shuffle_epi8(dblock2, swap16a));
    sum32b = _mm_add_epi32(sum32b,
			   _mm_shuffle_epi8(dblock2, swap16b));
  }

  /**
   * If rest of payload smaller than 64 bytes process 16 bytes
   * in one iteration.
   */
  while ((n + 16) <= data_len) {
    __m128i dblock;

    dblock = _mm_loadu_si128((__m128i *)&data[n]);
    sum32a = _mm_add_epi32(sum32a,
			   _mm_shuffle_epi8(dblock, swap16a));
    sum32b = _mm_add_epi32(sum32b,
			   _mm_shuffle_epi8(dblock, swap16b));
    n += 16;
  }

  if (n != data_len) {
    /**
     * Process very likely case of having less than 15 bytes
     * left at the end of the payload.
     */
    __m128i dblock;

    dblock = _mm_loadu_si128((__m128i *)&data[n]);
    dblock = xmm_shift_left(dblock, 16 - (data_len & 15));
    dblock = xmm_shift_right(dblock, 16 - (data_len & 15));
    sum32a = _mm_add_epi32(sum32a,
			   _mm_shuffle_epi8(dblock, swap16a));
    sum32b = _mm_add_epi32(sum32b,
			   _mm_shuffle_epi8(dblock, swap16b));
  }

  /**
   * Aggregate two 4x32 sum registers into one
   */
  sum32a = _mm_add_epi32(sum32a, sum32b);

  /**
   * Use horizontal dword add to go from 4x32-bits to 1x32-bits
   */
  sum32a = _mm_hadd_epi32(sum32a, _mm_setzero_si128());
  sum32a = _mm_hadd_epi32(sum32a, _mm_setzero_si128());
  return _mm_extract_epi32(sum32a, 0);
}

inline
uint16_t csum_oc16_reduce(uint32_t sum) {
  /**
   * Final part looks the same as in original algorithm
   */
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return (uint16_t)(~sum);
}

inline
uint16_t IPChecksumSSE(const uint8_t *data, uint32_t data_len) {
  uint32_t sum;

  if (data == NULL)
    return 0xffff;

  if (data_len == 0)
    return 0xffff;

  sum = csum_oc16_sse(data, data_len,
		      _mm_setzero_si128(),
		      _mm_setzero_si128());

  return csum_oc16_reduce(sum);
}

inline
uint32_t ekaPseudoTcpCsum_intel(const EkaIpHdr* ipHdr,
			  const EkaTcpHdr* tcpHdr) {
  
  //  IPChecksumInit();
  uint32_t sum = 0;

  sum += be16toh(ipHdr->src & 0xFFFF);
  sum += be16toh((ipHdr->src>>16) & 0xFFFF);
  sum += be16toh(ipHdr->dest & 0xFFFF);
  sum += be16toh((ipHdr->dest>>16) & 0xFFFF);
  sum += (uint16_t)EKA_PROTO_TCP;
  sum += be16toh(ipHdr->_len) - sizeof(EkaIpHdr);

  sum += csum_oc16_sse((const uint8_t*)tcpHdr,
		       be16toh(ipHdr->_len) - sizeof(EkaIpHdr),
		       _mm_setzero_si128(),
		       _mm_setzero_si128());

  return sum;
}


static inline
uint32_t calculate_pseudo_tcp_checksum_old(const EkaIpHdr* ipHdr,
					   const EkaTcpHdr* tcpHdr) {
  uint32_t sum = 0;
  
  sum += ipHdr->src & 0xFFFF;
  sum += (ipHdr->src>>16) & 0xFFFF;
  sum += ipHdr->dest & 0xFFFF;
  sum += (ipHdr->dest>>16) & 0xFFFF;

  sum += be16toh((uint16_t)EKA_PROTO_TCP);
  sum += be16toh(be16toh(ipHdr->_len) - sizeof(EkaIpHdr));

  int nbytes = be16toh(ipHdr->_len) - sizeof(EkaIpHdr);
  unsigned short *ptr = (unsigned short *)tcpHdr;
  unsigned short oddbyte;
  while(nbytes>1) {
    //    TEST_LOG("sum = 0x%04x, d = 0x%04x",sum,*ptr);
    sum+=*ptr++;
    nbytes-=2;
  }
  if(nbytes==1) {
    oddbyte=0;
    *((u_char*)&oddbyte)=*(u_char*)ptr;
    sum+=oddbyte;
  } 
  return sum;
}


inline
uint32_t calc_pseudo_ipHdrPart(const EkaIpHdr* ipHdr) {
uint32_t sum = 0;

 #if 0
  sum += be16toh(ipHdr->src & 0xFFFF);
  sum += be16toh((ipHdr->src>>16) & 0xFFFF);
  sum += be16toh(ipHdr->dest & 0xFFFF);
  sum += be16toh((ipHdr->dest>>16) & 0xFFFF);
  sum += (uint16_t)EKA_PROTO_TCP;
  sum += be16toh(ipHdr->_len) - sizeof(EkaIpHdr);
#else
  //  TEST_LOG("sum = 0x%04x d=0x%04x",sum,ipHdr->src & 0xFFFF);
  sum += ipHdr->src & 0xFFFF;

  //  TEST_LOG("sum = 0x%04x d=0x%04x",sum,(ipHdr->src>>16) & 0xFFFF);
  sum += (ipHdr->src>>16) & 0xFFFF;

  //  TEST_LOG("sum = 0x%04x d=0x%04x",sum,ipHdr->dest & 0xFFFF);
  sum += ipHdr->dest & 0xFFFF;

  //  TEST_LOG("sum = 0x%04x d=0x%04x",sum,(ipHdr->dest>>16) & 0xFFFF);
  sum += (ipHdr->dest>>16) & 0xFFFF;

  //  TEST_LOG("sum = 0x%04x d=0x%04x",sum,be16toh((uint16_t)EKA_PROTO_TCP));
  sum += be16toh((uint16_t)EKA_PROTO_TCP);

  //  TEST_LOG("sum = 0x%04x d=0x%04x",sum,be16toh(be16toh(ipHdr->_len) - sizeof(EkaIpHdr)));
  sum += be16toh(be16toh(ipHdr->_len) - sizeof(EkaIpHdr));
#endif
  return sum;
}

template <const int Iter> inline
uint32_t calcPseudoBlock(const uint16_t *ptr) {
  uint32_t sum = 0;
  //#pragma GCC optimize ("unroll-loops")
  for (auto i = 0; i < Iter; i++) {
    //    TEST_LOG("sum = 0x%04x d=0x%04x",sum,*ptr);
    sum += *ptr++;
  }
  return sum;
}


inline
uint32_t pseudo_csum_unrolled(const uint16_t *ptr,int nbytes) {
  uint32_t sum = 0;
  int restBytes = nbytes;
  {
    const int iter = 512;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 256;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 128;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 64;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 32;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 16;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 8;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 4;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 2;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  {
    const int iter = 1;
    if (restBytes >= iter * 2) {
      sum += calcPseudoBlock<iter>(ptr);
      ptr += iter;
      restBytes -= 2 * iter;
    }
  }
  if (restBytes == 1) {
    uint16_t oddbyte = 0;
    *((u_char*)&oddbyte)=*(u_char*)ptr;
    //    TEST_LOG("sum = 0x%04x d=0x%04x",sum,oddbyte);
    sum += oddbyte;
  }
  return sum;
}

inline
uint16_t ekaCsum(const uint16_t *ptr,int nbytes) {
    long sum;
    unsigned short oddbyte;
    short answer;
 
    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }
    sum = (sum>>16)+(sum & 0xffff);
    sum = sum + (sum>>16);
    answer=(short)~sum;
     
    return(answer);
}

inline
uint32_t ekaPseudoTcpCsum(const EkaIpHdr* ipHdr,
			  const EkaTcpHdr* tcpHdr) {
  auto pseudo = calc_pseudo_ipHdrPart(ipHdr);
  pseudo += pseudo_csum_unrolled((const uint16_t*)tcpHdr,
				 be16toh(ipHdr->_len) - sizeof(EkaIpHdr));

  //  TEST_LOG("TCP LEN = %d",(int)(be16toh(ipHdr->_len) - sizeof(EkaIpHdr)));
  return pseudo;
}

inline 
uint16_t foldPseudoCsum (uint32_t pseudo) {
  uint32_t sum = pseudo;
  while (sum>>16)
    sum = (sum & 0xFFFF)+(sum >> 16);

  /* sum = (sum>>16)+(sum & 0xffff); */
  /* sum = sum + (sum>>16); */

  // one's complement the result
  sum = ~sum;
	
  return ((uint16_t) sum);
}

inline
uint16_t ekaTcpCsum(const EkaIpHdr* ipHdr,
		 const EkaTcpHdr* tcpHdr) {

  return foldPseudoCsum(ekaPseudoTcpCsum(ipHdr,tcpHdr));
}
#endif

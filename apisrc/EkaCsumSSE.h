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
uint32_t ekaPseudoTcpCsum(const EkaIpHdr* ipHdr,
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
uint16_t calculate_tcp_checksum_old(uint32_t pseudo) {
  uint32_t sum = pseudo;

  // Fold the sum into a 16-bit checksum
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  return static_cast<uint16_t>(~sum);
}

#endif

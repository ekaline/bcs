#ifndef _EKA_NW_PARSER_H_
#define _EKA_NW_PARSER_H_

#include <sys/types.h>

namespace EkaNwParser {
  class Eth {
    struct Hdr {
      uint8_t  dest[6];
      uint8_t  src[6];
      uint16_t type;
    } __attribute__((packed));

  public:
    static inline uint16_t getType(const void* p) {
      return be16toh(reinterpret_cast<const Hdr*>(p)->type);
    }
    static inline const uint8_t* getMacDa(const void* p) {
      return &(reinterpret_cast<const Hdr*>(p)->dest[0]);
    }
    static inline const uint8_t* getMacSa(const void* p) {
      return &(reinterpret_cast<const Hdr*>(p)->src[0]);
    }
    static inline size_t getHdrLen(const void* p = NULL) {
      return sizeof(Hdr);
    }
    static inline bool isIp(const void* p) {
      auto type = getType(p);
      return type == 0x0800;
    }
  };

    class Ip {
      struct Hdr {
	/* version / header length */
	uint8_t _v_hl;
	/* type of service */
	uint8_t _tos;
	/* total length */
	uint16_t _len;
	/* identification */
	uint16_t _id;
	/* fragment offset field */
	uint16_t _offset;
	/* #define IP_RF 0x8000U        /\* reserved fragment flag *\/ */
	/* #define IP_DF 0x4000U        /\* don't fragment flag *\/ */
	/* #define IP_MF 0x2000U        /\* more fragments flag *\/ */
#define EKA_IP_OFFMASK 0x1fffU   /* mask for fragmenting bits */
	/* time to live */
	uint8_t _ttl;
	/* protocol*/
	uint8_t _proto;
	/* checksum */
	uint16_t _chksum;
	/* source and destination IP addresses */
	uint32_t src;
	uint32_t dest;
      } __attribute__ ((aligned (sizeof(uint16_t))));

    public:
      static inline uint32_t getSrc(const void* p) {
	return reinterpret_cast<const Hdr*>(p)->src;
      }    
      static inline uint32_t getDst(const void* p) {
	return reinterpret_cast<const Hdr*>(p)->dest;
      }
      static inline uint16_t getPktLen(const void* p) {
	return be16toh(reinterpret_cast<const Hdr*>(p)->_len);
      }
      static inline size_t getHdrLen(const void* p) {
	uint8_t ihl = reinterpret_cast<const Hdr*>(p)->_v_hl & 0x0F;
	return 4 * ihl;
      }
      static inline uint8_t getProto(const void* p) {
	return reinterpret_cast<const Hdr*>(p)->_proto;
      }    
      static inline bool isTcp(const void* p) {
	auto proto = getProto(p);
	return proto == 0x06;
      }
      static inline bool isUdp(const void* p) {
	auto proto = getProto(p);
	return proto == 0x11;
      }    
    };

    class Tcp {
      struct Hdr {
	uint16_t src;
	uint16_t dest;
	uint32_t seqno;
	uint32_t ackno;
	uint16_t _hdrlen_rsvd_flags;
	uint16_t wnd;
	uint16_t chksum;
	uint16_t urgp;
      } __attribute__ ((aligned (sizeof(uint16_t))));

    public:
      static inline uint16_t getSrc(const void* p) {
	return be16toh(reinterpret_cast<const Hdr*>(p)->src);
      }         
      static inline uint16_t getDst(const void* p) {
	return be16toh(reinterpret_cast<const Hdr*>(p)->dest);
      }         
      static inline size_t getHdrLen(const void* p) {
	auto h = reinterpret_cast<const Hdr*>(p)->_hdrlen_rsvd_flags;
	auto hdrLen = (be16toh(h) >> 12) & 0x000F;
	return 4 * hdrLen;
      }      
    };
    
    inline bool isTcpPkt(const void* pkt) {
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      if (! Eth::isIp(p)) return false;
      p += Eth::getHdrLen();
      return Ip::isTcp(p);
    }

    inline const uint8_t* getTcpPayload(const void* pkt) {
      if (! isTcpPkt(pkt)) return NULL;
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      p += Eth::getHdrLen();
      p += Ip::getHdrLen(p);
      p += Tcp::getHdrLen(p);
      return p;
    }

    inline uint32_t getIpSrc(const void* pkt) {
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      p += Eth::getHdrLen();
      return Ip::getSrc(p);
    }

    inline uint32_t getIpDst(const void* pkt) {
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      p += Eth::getHdrLen();
      return Ip::getDst(p);
    }

    inline uint16_t getTcpSrc(const void* pkt) {
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      p += Eth::getHdrLen();
      p += Ip::getHdrLen(p);
      return Tcp::getSrc(p);
    }

    inline uint getIpPktLen(const void* pkt) {
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      p += Eth::getHdrLen();
      return Eth::getHdrLen() + Ip::getPktLen(p);
    }

    inline ssize_t getTcpPayloadLen(const void* pkt) {
      if (! isTcpPkt(pkt)) return -1;
	    
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      p += Eth::getHdrLen();
      auto ipHdrLen = Ip::getHdrLen(p);
      p += ipHdrLen;
      auto tcpHdrLen = Tcp::getHdrLen(p);
      return getIpPktLen(pkt) - Eth::getHdrLen() - ipHdrLen - tcpHdrLen;
    }
}

#endif

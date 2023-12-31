#ifndef _EKA_NW_PARSER_H_
#define _EKA_NW_PARSER_H_

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "eka_macros.h"

namespace EkaNwParser {
  typedef size_t (*PrintMsgCb)(const void* p, uint64_t seq, FILE* fd);

  
  class Eth {
  public:
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
  public:
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
  public:
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

  class Udp {
    struct Hdr {
      uint16_t src;
      uint16_t dst;
      uint16_t len;
      uint16_t chksum;
    } __attribute__ ((aligned (sizeof(uint16_t))));

  public:
    constexpr static inline size_t getHdrLen(const void* p = NULL) {
      return sizeof(Hdr);
    }

    static inline uint16_t getSrc(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};
      return be16toh(h->src);
    }
    
    static inline uint16_t getDst(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};
      return be16toh(h->dst);
    }
    
    static inline uint16_t getLen(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};
      return be16toh(h->len);
    }
    
    static inline uint16_t getChckSum(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};
      return be16toh(h->chksum);
    }
    
  };

  class SoupBinTcp {
  public:
    struct Hdr {
      uint16_t    length;
      char        type;
    } __attribute__((packed));

    constexpr static inline
    size_t getHdrLen(const void* p = NULL) {
      return sizeof(Hdr);
    }

    static inline size_t getPayloadLen(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};

      if (static_cast<size_t>(be16toh(h->length)) <= sizeof(h->type))
				on_error("h->length = %u",be16toh(h->length));
      return static_cast<size_t>(be16toh(h->length)) - sizeof(h->type);
    }

    static inline char getType(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};
      return h->type;
    }

  };
  
  class MoldUdp64 {
  private:
    struct Hdr {
      uint8_t	sessionId[10];
      uint64_t	sequence;
      uint16_t	messageCnt;
    } __attribute__((packed));
  public:
    static inline
		uint64_t getSequence(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};
      return be64toh(h->sequence);
    }

    static inline
		uint16_t getMsgCnt(const void* p) {
      auto h {reinterpret_cast<const Hdr*>(p)};
      return be16toh(h->messageCnt);
    }
    
    constexpr static inline
		size_t getHdrLen(const void* p = NULL) {
      return sizeof(Hdr);
    }

		static inline
		const uint8_t* getFirstMsg(const void* pkt) {
			if (getMsgCnt(pkt) == 0)
				return NULL;

			auto p {reinterpret_cast<const uint8_t*>(pkt)};
      p += getHdrLen();
			auto msgLenRaw = reinterpret_cast<const uint16_t*>(p);
			p += sizeof(*msgLenRaw);
			return p;
		}
		
    static inline
		size_t printPkt(FILE* fd, const void* pkt,
										PrintMsgCb printFunc) {
      auto p {reinterpret_cast<const uint8_t*>(pkt)};
      auto sequence = getSequence(pkt);
      auto msgCnt   = getMsgCnt(pkt);
      p += getHdrLen();
      for (uint16_t i = 0; i < msgCnt; i++) {
				auto msgLenRaw = reinterpret_cast<const uint16_t*>(p);
				auto msgLen = be16toh(*msgLenRaw);
				p += sizeof(*msgLenRaw);
				auto parsedMsgLen = printFunc(p,sequence,fd);
				if (parsedMsgLen != msgLen)
					on_error("parsedMsgLen %ju != msgLen %d",
									 parsedMsgLen,msgLen);
				sequence++;
				p += msgLen;
      }
      return p - reinterpret_cast<const uint8_t*>(pkt);
    }  
  };
  /* ================================================ */
  class PcapHandler {
  protected:
    PcapHandler(const char* fileName,
								uint32_t ip, uint16_t port) {
      strcpy(fileName_,fileName);
      fp_ = fopen(fileName_,"rb");
      if (!fp_)
				on_error("Cannot open %s",fileName_);
      
      pcap_file_hdr pcapFileHdr;
      if (fread(&pcapFileHdr,sizeof(pcapFileHdr),1,fp_) != 1) 
				on_error ("Failed to read pcap_file_hdr from the pcap file");

      ip_   = ip;
      port_ = port;

      TEST_LOG("%s: %s:%u",
							 fileName_,EKA_IP2STR(ip_),port_);
    }

  public:
    ~PcapHandler() {
      TEST_LOG("closing %s",fileName_);
      fclose(fp_);
    }

    bool isEOF() {
      return feof(fp_);
    }

    uint64_t getPktNum() {
      return nPkts_;
    }

  protected:
    FILE* fp_;
    char fileName_[256] = {};
    uint32_t ip_;
    uint16_t port_;
    bool     pktInProgress_ = false;
    size_t   nPktBytes_ = 0;
    uint64_t nPkts_ = 0;
  };
  
  class TcpPcapHandler : public PcapHandler {
  public:
    TcpPcapHandler(const char* fileName,
									 uint32_t srcIp, uint16_t srcPort) :
      PcapHandler(fileName,srcIp,srcPort) {}

    virtual ~TcpPcapHandler() {}


    ssize_t getData(void* dst, size_t nBytes) {
      size_t pendingBytes = nBytes;
      uint8_t* p = (uint8_t*)dst;
      while (pendingBytes != 0) {
				if (nPktBytes_ == 0)
					nPktBytes_ = skipToMyPayload();

				if (nPktBytes_ < 0) return nPktBytes_;
	
				size_t bytes2readNow = std::min(pendingBytes,nPktBytes_);
				if (bytes2readNow != 0 && fread(p,bytes2readNow,1,fp_) != 1) {
					if (feof(fp_)) return -2;
					on_error("Failed to read %jd bytes (nPktBytes_=%jd): \'%s\'",
									 bytes2readNow,nPktBytes_,strerror(errno));
				}
				nPktBytes_ -= bytes2readNow;
				p += bytes2readNow;
				pendingBytes -= bytes2readNow;
      } // while()
      return nBytes;
    }


  private:
    ssize_t skipToMyPayload() {
      Eth::Hdr ethHdr;
      Ip::Hdr  ipHdr;
      Tcp::Hdr tcpHdr;
      uint8_t  nullBuf[1536];
      ssize_t  extraBytes;
      size_t   nCurPktBytes;
      while (1) {
				pcap_rec_hdr pcapHdr;
				if (fread(&pcapHdr,sizeof(pcapHdr),1,fp_) != 1) {
					if (feof(fp_)) {
						TEST_LOG("EOF");
						return -2;
					} else {
						on_error("Failed to read pcapHdr after %ju pkts",nPkts_);
					}
				}
				nCurPktBytes = pcapHdr.len;
				nPkts_ ++;
				// TEST_LOG("nPkts_=%ju: nCurPktBytes=%ju",nPkts_,nCurPktBytes);
				// ------------------------
				if (fread(&ethHdr,sizeof(ethHdr),1,fp_) != 1)
					on_error("Failed to read ethHdr");

				nCurPktBytes -= sizeof(ethHdr);
				// TEST_LOG("nCurPktBytes=%ju",nCurPktBytes);
				if (! Eth::isIp(&ethHdr)) goto DISCARD_PKT;
				// ------------------------
				if (fread(&ipHdr,sizeof(ipHdr),1,fp_) != 1)
					on_error("Failed to read ipHdr");

				nCurPktBytes -= sizeof(ipHdr);

				if (! Ip::isTcp(&ipHdr)) goto DISCARD_PKT;
				extraBytes = Ip::getHdrLen(&ipHdr) - sizeof(ipHdr);
				if (extraBytes && fread(nullBuf,extraBytes,1,fp_) != 1)
					on_error("Failed to read %ju bytes of extra ipHdr",
									 extraBytes);

				nCurPktBytes -= extraBytes;
				// ------------------------
				if (fread(&tcpHdr,sizeof(tcpHdr),1,fp_) != 1)
					on_error("Failed to read tcpHdr");

				nCurPktBytes -= sizeof(tcpHdr);
				extraBytes = Tcp::getHdrLen(&tcpHdr) - sizeof(tcpHdr);
				if (extraBytes && fread(nullBuf,extraBytes,1,fp_) != 1)
					on_error("Failed to read %ju bytes of extra tcpHdr",
									 extraBytes);

				nCurPktBytes -= extraBytes;

				// ------------------------

				if (Ip::getSrc (&ipHdr)  == ip_ &&
						Tcp::getSrc(&tcpHdr) == port_) {
					ssize_t payloadLen = Ip::getPktLen(&ipHdr) -
						Ip::getHdrLen(&ipHdr) -
						Tcp::getHdrLen(&tcpHdr);
					if (payloadLen == 0) goto DISCARD_PKT;
					return nCurPktBytes; // My pkt
				} 
      DISCARD_PKT:
				if (nCurPktBytes && fread(nullBuf,nCurPktBytes,1,fp_) != 1)
					on_error("Failed to read %ju bytes of pkt to discard",
									 nCurPktBytes);

      } // while(1)

      return 0;
    }

  };

  class UdpPcapHandler : public PcapHandler {
  public:
    UdpPcapHandler(const char* fileName,
									 uint32_t ip, uint16_t port) :
      PcapHandler(fileName,ip,port) {}

    virtual ~UdpPcapHandler() {}

    // -------------------------------
    const uint8_t* getData(void* dst) {
      while (1) {
				pcap_rec_hdr pcapHdr;
				if (fread(&pcapHdr,sizeof(pcapHdr),1,fp_) != 1) {
					if (feof(fp_)) {
						TEST_LOG("EOF");
						return NULL;
					} else {
						on_error("Failed to read pcapHdr after %ju pkts",nPkts_);
					}
				}
				nPkts_ ++;

				if (fread(dst,pcapHdr.len,1,fp_) != 1) 
					on_error ("Failed to read %d packet bytes at pkt %ju",
										pcapHdr.len,nPkts_);

				const uint8_t* p = (const uint8_t*) dst;
				if (! Eth::isIp(p)) continue;
				p += Eth::getHdrLen();
	
				if (! Ip::isUdp(p)) continue;
				if (ip_ != Ip::getDst(p)) continue;
				p += Ip::getHdrLen(p);

				if (port_ != Udp::getDst(p)) continue;
				p += Udp::getHdrLen(p);
				return p;
      } 
    }
    // -------------------------------

  };

  
} // namespace EkaNwParser

#endif

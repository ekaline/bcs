#ifndef _EKA_NW_HEADERS_H
#define _EKA_NW_HEADERS_H

#include <sys/types.h>


/* struct pcap_file_hdr { */
/*         uint32_t magic_number;   /\* magic number *\/ */
/*          uint16_t version_major;  /\* major version number *\/ */
/*          uint16_t version_minor;  /\* minor version number *\/ */
/*          int32_t  thiszone;       /\* GMT to local correction *\/ */
/*          uint32_t sigfigs;        /\* accuracy of timestamps *\/ */
/*          uint32_t snaplen;        /\* max length of captured packets, in octets *\/ */
/*          uint32_t network;        /\* data link type *\/ */
/*  }; */
/*  struct pcap_rec_hdr { */
/*          uint32_t ts_sec;         /\* timestamp seconds *\/ */
/*          uint32_t ts_usec;        /\* timestamp microseconds *\/ */
/*          uint32_t cap_len;        /\* number of octets of packet saved in file *\/ */
/*          uint32_t len;            /\* actual length of packet *\/ */
/*  }; */

struct EkaEthHdr {
  uint8_t dest[6];
  uint8_t src[6];
  uint16_t type;
};

struct EkaIpHdr {
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
};

struct EkaTcpHdr {
  uint16_t src;
  uint16_t dest;
  uint32_t seqno;
  uint32_t ackno;
  uint16_t _hdrlen_rsvd_flags;
  uint16_t wnd;
  uint16_t chksum;
  uint16_t urgp;
};

struct EkaUdpHdr {
  uint16_t src;
  uint16_t dest;
  uint16_t len;
  uint16_t chksum;
};


/* typedef struct eth_hdr EkaEthHdr; */
/* typedef struct ip_hdr  EkaIpHdr; */
/* typedef struct tcp_hdr EkaTcpHdr; */


#define EKA_IPH(buf)  ((EkaIpHdr*)  (((uint8_t*)buf) + sizeof(EkaEthHdr)))
#define EKA_TCPH(buf) ((EkaTcpHdr*) (((uint8_t*)buf) + sizeof(EkaEthHdr) + sizeof(EkaIpHdr)))
#define EKA_UDPH(buf) ((EkaUdpHdr*) (((uint8_t*)buf) + sizeof(EkaEthHdr) + sizeof(EkaIpHdr)))

/* Macros to get struct eth_hdr fields: */
#define EKA_ETHTYPE_IP        0x0800
#define EKA_ETHTYPE_ARP       0x0806

#define EKA_PROTO_TCP         0x06
#define EKA_PROTO_UDP         0x11

#define EKA_ETH_TYPE(pkt)  ((uint16_t)be16toh((((EkaEthHdr*)pkt)->type)))
#define EKA_ETH_MACDA(pkt) ((uint8_t*)&(((EkaEthHdr*)pkt)->dest))
#define EKA_ETH_MACSA(pkt) ((uint8_t*)&(((EkaEthHdr*)pkt)->src))

/* Macros to get struct ip_hdr fields: */
#define EKA_IPH_V(hdr)  ((hdr)->_v_hl >> 4)
#define EKA_IPH_HL(hdr) ((hdr)->_v_hl & 0x0f)
#define EKA_IPH_HL_BYTES(hdr) ((uint8_t)(EKA_IPH_HL(hdr) * 4))
#define EKA_IPH_TOS(hdr) ((hdr)->_tos)
#define EKA_IPH_LEN(hdr) (be16toh((hdr)->_len))
#define EKA_IPH_ID(hdr) ((hdr)->_id)
#define EKA_IPH_OFFSET(hdr) ((hdr)->_offset)
#define EKA_IPH_OFFSET_BYTES(hdr) ((uint16_t)((be16toh(EKA_IPH_OFFSET(hdr)) & EKA_IP_OFFMASK) * 8U))
#define EKA_IPH_TTL(hdr) ((hdr)->_ttl)
#define EKA_IPH_PROTO(hdr) ((hdr)->_proto)
#define EKA_IPH_CHKSUM(hdr) (be16toh((EKA_IPH(hdr))->_chksum))
#define EKA_IPH_CHKSUM_BE(hdr) ((EKA_IPH(hdr))->_chksum)
//#define EKA_IPH_CHKSUM(hdr) (((EkaIpHdr*)hdr)->_chksum)

#define EKA_IPH_SRC(hdr) ((EKA_IPH(hdr))->src)
#define EKA_IPH_DST(hdr) ((EKA_IPH(hdr))->dest)

/* Macros to set struct ip_hdr fields: */
#define EKA_IPH_VHL_SET(hdr, v, hl) (hdr)->_v_hl = (uint8_t)((((v) << 4) | (hl)))
#define EKA_IPH_TOS_SET(hdr, tos) (hdr)->_tos = (tos)
#define EKA_IPH_ID_SET(hdr, id) (hdr)->_id = (id)
#define EKA_IPH_OFFSET_SET(hdr, off) (hdr)->_offset = (off)
#define EKA_IPH_TTL_SET(hdr, ttl) (hdr)->_ttl = (uint8_t)(ttl)
#define EKA_IPH_PROTO_SET(hdr, proto) (hdr)->_proto = (uint8_t)(proto)

#define EKA_IPH_LEN_SET(pkt, len)       (EKA_IPH(pkt))->_len    = (len)
#define EKA_IPH_CHKSUM_SET(pkt, chksum) (EKA_IPH(pkt))->_chksum = (chksum)

#define EKA_IPH_DST_SET(pkt, dst_ip)       (EKA_IPH(pkt))->dest   = (dst_ip)


/* Macros to get struct tcp_hdr fields: */
#define EKA_TCP_FLAGS_MASK 0x3F
#define EKA_TCPH_HDRLEN(phdr) ((uint16_t)(be16toh((phdr)->_hdrlen_rsvd_flags) >> 12))
#define EKA_TCPH_HDRLEN_BYTES(phdr) ((uint8_t)(EKA_TCPH_HDRLEN(phdr) << 2))
#define EKA_TCPH_FLAGS(pkt)  ((uint8_t)((be16toh((EKA_TCPH(pkt))->_hdrlen_rsvd_flags) & EKA_TCP_FLAGS_MASK)))

#define EKA_TCP_SYN(pkt) ((TCPH_FLAGS(EKA_TCPH(pkt)) & TCP_SYN) != 0)
#define EKA_TCP_FIN(pkt) ((TCPH_FLAGS(EKA_TCPH(pkt)) & TCP_FIN) != 0)
#define EKA_TCP_ACK(pkt) ((TCPH_FLAGS(EKA_TCPH(pkt)) & TCP_ACK) != 0)

#define EKA_TCPH_SEQNO(pkt) ((uint32_t)(be32toh((EKA_TCPH(pkt))->seqno)))
#define EKA_TCPH_ACKNO(pkt) ((uint32_t)(be32toh((EKA_TCPH(pkt))->ackno)))


#define EKA_TCP_SEQ_LT(a,b)     ((int)((uint32_t)(a) - (uint32_t)(b)) < 0)
#define EKA_TCP_SEQ_LEQ(a,b)    ((int)((uint32_t)(a) - (uint32_t)(b)) <= 0)
#define EKA_TCP_SEQ_GT(a,b)     ((int)((uint32_t)(a) - (uint32_t)(b)) > 0)
#define EKA_TCP_SEQ_GEQ(a,b)    ((int)((uint32_t)(a) - (uint32_t)(b)) >= 0)


#define EKA_TCPH_SRC(pkt) ((uint16_t)(be16toh((EKA_TCPH(pkt))->src)))
#define EKA_TCPH_DST(pkt) ((uint16_t)(be16toh((EKA_TCPH(pkt))->dest)))

#define EKA_TCPH_WND(pkt) ((uint16_t)(be16toh((EKA_TCPH(pkt))->wnd)))
#define EKA_TCPH_PAYLOAD(pkt) ((uint8_t*)((uint8_t*)EKA_TCPH(pkt)+EKA_TCPH_HDRLEN_BYTES(EKA_TCPH(pkt))))


//#define EKA_TCP_PAYLOAD_LEN(iph,tcph) ((uint16_t)(EKA_IPH_LEN(iph) - EKA_IPH_HL_BYTES(iph) - TCPH_HDRLEN_BYTES(tcph)))
#define EKA_TCP_PAYLOAD_LEN(pkt) ((uint16_t)(EKA_IPH_LEN(EKA_IPH(pkt)) - EKA_IPH_HL_BYTES(EKA_IPH(pkt)) - EKA_TCPH_HDRLEN_BYTES(EKA_TCPH(pkt))))

/* Macros to get struct tcp_hdr fields: */
#define EKA_TCPH_HDRLEN_SET(phdr, len) (phdr)->_hdrlen_rsvd_flags = lwip_htons(((len) << 12) | TCPH_FLAGS(phdr))
#define EKA_TCPH_FLAGS_SET(phdr, flags) (phdr)->_hdrlen_rsvd_flags = (((phdr)->_hdrlen_rsvd_flags & PP_HTONS(~TCP_FLAGS)) | lwip_htons(flags))
#define EKA_TCPH_HDRLEN_FLAGS_SET(phdr, len, flags) (phdr)->_hdrlen_rsvd_flags = (uint16_t)(lwip_htons((uint16_t)((len) << 12) | (flags)))

#define EKA_TCPH_SET_FLAG(phdr, flags ) (phdr)->_hdrlen_rsvd_flags = ((phdr)->_hdrlen_rsvd_flags | lwip_htons(flags))
#define EKA_TCPH_UNSET_FLAG(pkt, flags) (EKA_TCPH(pkt))->_hdrlen_rsvd_flags = ((EKA_TCPH(pkt))->_hdrlen_rsvd_flags & ~lwip_htons(flags))

#define EKA_TCPH_CHKSUM_SET(pkt, tcpchksum) (EKA_TCPH(pkt))->chksum = (be16toh(tcpchksum))

#define EKA_TCPH_ACKNO_SET(pkt, _ackno) ((EKA_TCPH(pkt))->ackno = be32toh(_ackno))
#define EKA_TCPH_SEQNO_SET(pkt, _seqno) ((EKA_TCPH(pkt))->seqno = be32toh(_seqno))

#define EKA_UDPH_SRC(pkt) ((uint16_t)(be16toh((EKA_UDPH(pkt))->src)))
#define EKA_UDPH_DST(pkt) ((uint16_t)(be16toh((EKA_UDPH(pkt))->dest)))

//#define EKA_UDPHDR_DST(hdr) (uint16_t)be16toh(((EkaUdpHdr*)hdr)->dest)
#define EKA_UDPHDR_DST(hdr) be16toh(((EkaUdpHdr*)((hdr)))->dest)

#define EKA_IS_IP4_PKT(pkt) (EKA_ETH_TYPE(pkt) == EKA_ETHTYPE_IP)
#define EKA_IS_ARP_PKT(pkt) (EKA_ETH_TYPE(pkt) == EKA_ETHTYPE_ARP)

#define EKA_IS_TCP_PKT(pkt) (EKA_IS_IP4_PKT(pkt) && (EKA_IPH_PROTO(EKA_IPH(pkt)) == EKA_PROTO_TCP))
#define EKA_IS_UDP_PKT(pkt) (EKA_IS_IP4_PKT(pkt) && (EKA_IPH_PROTO(EKA_IPH(pkt)) == EKA_PROTO_UDP))


struct EkaIgmpV2Hdr {
  uint8_t  type; // join ? 0x16 : 0x17;
  uint8_t  code; // = 0
  uint16_t cksum;
  uint32_t group;
};


// MoldUdp64
struct mold_hdr {
  uint8_t	session_id[10];
  uint64_t	sequence;
  uint16_t	message_cnt;
} __attribute__((packed));

#define EKA_MOLD_SEQUENCE(hdr) (be64toh(((struct mold_hdr *)hdr)->sequence))
#define EKA_MOLD_MSG_CNT(hdr) (be16toh(((struct mold_hdr *)hdr)->message_cnt))

// Mold for Phlx Orders (MolDUdp)
struct PhlxMoldHdr { 
  uint8_t	session_id[10];
  uint32_t	sequence;
  uint16_t	message_cnt;
} __attribute__((packed));
#define EKA_PHLX_MOLD_SEQUENCE(hdr) (((struct PhlxMoldHdr *)hdr)->sequence)
#define EKA_PHLX_MOLD_MSG_CNT(hdr) (((struct PhlxMoldHdr *)hdr)->message_cnt)


#endif //_EKA_NW_HEADERS_H


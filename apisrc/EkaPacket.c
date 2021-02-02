#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>

#include "ekaNW.h"
#include "eka_macros.h"

uint16_t pseudo_csum2csum (uint32_t pseudo) {
  uint32_t sum = pseudo;
  //  while (sum>>16) sum = (sum & 0xFFFF)+(sum >> 16);

  sum = (sum>>16)+(sum & 0xffff);
  sum = sum + (sum>>16);

  // one's complement the result
  sum = ~sum;
	
  return ((uint16_t) sum);
}

unsigned short csum(unsigned short *ptr,int nbytes) {
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

unsigned int pseudo_csum(unsigned short *ptr,int nbytes) {
    long sum;
    unsigned short oddbyte;
    unsigned int answer;
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
    answer = sum; 
    return(answer);
}

uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size) {  
  struct pseudo_header {
      uint32_t src_ip;
      uint32_t dst_ip;
      uint8_t placeholder;
      uint8_t proto;
      uint16_t tcp_length;
    };
  char pseudo_packet[2000];
  memset(pseudo_packet, 0, sizeof(struct pseudo_header) + sizeof(struct tcphdr) + payload_size);

  struct pseudo_header psh;
  psh.src_ip = ((struct iphdr*)ip_hdr)->saddr;
  psh.dst_ip = ((struct iphdr*)ip_hdr)->daddr;
  psh.placeholder = (uint8_t) 0;
  psh.proto = (uint8_t) IPPROTO_TCP;
  psh.tcp_length = be16toh(sizeof(struct tcphdr) + payload_size);

  memcpy(pseudo_packet, (char *) &psh, sizeof(struct pseudo_header));
  memcpy(pseudo_packet + sizeof(struct pseudo_header), tcp_hdr, sizeof(struct tcphdr));
  memcpy(pseudo_packet + sizeof(struct pseudo_header) + sizeof(struct tcphdr), payload, payload_size);
  uint32_t pcsum = pseudo_csum( (unsigned short*) pseudo_packet , sizeof(struct pseudo_header) + sizeof(struct tcphdr) + payload_size);
  return pcsum;
}

uint32_t calcEmptyPktPseudoCsum (EkaIpHdr* ipHdr, EkaTcpHdr* tcpHdr) {  
  struct PseudoHeader {
      uint32_t src_ip;
      uint32_t dst_ip;
      uint8_t  placeholder;
      uint8_t  proto;
      uint16_t tcp_length;
    };

  struct PseudoPacket {
    PseudoHeader pseudoHdr;
    EkaTcpHdr    tcpHdr;
  };
  
  PseudoHeader pseudoHdr = {
    .src_ip      = ipHdr->src,
    .dst_ip      = ipHdr->dest,
    .placeholder = 0,
    .proto       = EKA_PROTO_TCP,
    .tcp_length  = be16toh(sizeof(EkaTcpHdr))
  };

  PseudoPacket pseudoPkt = {};
  memcpy(&pseudoPkt.pseudoHdr,&pseudoHdr,sizeof(pseudoHdr));
  memcpy(&pseudoPkt.tcpHdr,    tcpHdr   ,sizeof(EkaTcpHdr));
  return pseudo_csum((unsigned short*)&pseudoPkt,sizeof(pseudoPkt));
}

int createIgmpPkt (char* dst, bool join, uint8_t* macsa, uint32_t ip_src, uint32_t ip_dst) {
  struct IgmpPkt {
    EkaEthHdr    ethHdr;
    EkaIpHdr     ipHdr;
    uint32_t     ip_options;
    EkaIgmpV2Hdr igmpHdr;
  } __attribute__((packed));

  //  TEST_LOG("Creating IGMP %s packet",join ? "JOIN" : "LEAVE");

  IgmpPkt* pkt = (IgmpPkt*) dst;

  uint8_t macda[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x00};
  macda[3] = ((uint8_t*) &ip_dst)[1] & 0x7F;
  macda[4] = ((uint8_t*) &ip_dst)[2];
  macda[5] = ((uint8_t*) &ip_dst)[3];
  
  memcpy (&(pkt->ethHdr.dest) , macda, 6);
  memcpy (&(pkt->ethHdr.src)  , macsa, 6);
  pkt->ethHdr.type = be16toh(0x0800);
  
  pkt->ipHdr._v_hl = 0x46;
  pkt->ipHdr._tos = 0xc0;
  pkt->ipHdr._len = be16toh(sizeof(pkt->ipHdr) + sizeof(pkt->igmpHdr) + 4);

  pkt->ipHdr._id = 0;
  pkt->ipHdr._offset = 0x0040;
  pkt->ipHdr._ttl = 1;
  pkt->ipHdr._proto = 0x2; //IPPROTO_IGMP;
  pkt->ipHdr._chksum = 0;

  pkt->ipHdr.src = ip_src;
  pkt->ipHdr.dest = ip_dst;

  pkt->ip_options = be32toh(0x94040000); 

  pkt->ipHdr._chksum = csum((unsigned short *)&pkt->ipHdr, sizeof(pkt->ipHdr) + 4);

  pkt->igmpHdr.type = join ? 0x16 : 0x17;
  pkt->igmpHdr.code = 0;
  pkt->igmpHdr.cksum = 0;
  pkt->igmpHdr.group = ip_dst;
  pkt->igmpHdr.cksum = csum((unsigned short *)&pkt->igmpHdr, sizeof(pkt->igmpHdr));

  return 14 + 24 + 8;
}

uint16_t udp_checksum(EkaUdpHdr* p_udp_header, size_t len, uint32_t src_addr, uint32_t dest_addr) {
  const uint16_t *buf = (const uint16_t*)p_udp_header;
  uint16_t *ip_src = (uint16_t*)&src_addr, *ip_dst = (uint16_t*)&dest_addr;
  uint32_t sum;
  size_t length = len;

  // Calculate the sum
  sum = 0;
  while (len > 1)  {
    sum += *buf++;
    if (sum & 0x80000000)
      sum = (sum & 0xFFFF) + (sum >> 16);
    len -= 2;
  }

  if (len & 1)
    // Add the padding if the packet lenght is odd
    sum += *((uint8_t*)buf);

  // Add the pseudo-header
  sum += *(ip_src++);
  sum += *ip_src;

  sum += *(ip_dst++);
  sum += *ip_dst;

  sum += htons(IPPROTO_UDP);
  sum += htons(length);

  // Add the carries
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  // Return the one's complement of sum
  return (uint16_t)~sum;
}

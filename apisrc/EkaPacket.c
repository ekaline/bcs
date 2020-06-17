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
  };

  IgmpPkt* pkt = (IgmpPkt*) dst;

  /* struct eth_hdr* ethheader = (struct eth_hdr*) dst; */
  /* struct ip_hdr* ipheader = (struct ip_hdr*) ((uint8_t*) ethheader + sizeof(struct eth_hdr)); */
  /* uint32_t* ip_options = (uint32_t*)((uint8_t*) ipheader + sizeof(struct ip_hdr)); */
  /* struct igmpV2* igmpheader = (struct igmpV2*) ((uint8_t*) ipheader + sizeof(struct ip_hdr) + 4); // 4 for Router Aler Options */

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
  /* memcpy(&pkt->ipHdr.src,&ip_src,4); */
  /* memcpy(&pkt->ipHdr.dest,&ip_dst,4); */

  pkt->ip_options = be32toh(0x94040000); 

  pkt->ipHdr._chksum = csum((unsigned short *)&pkt->ipHdr, sizeof(pkt->ipHdr) + 4);

  pkt->igmpHdr.type = join ? 0x16 : 0x17;
  pkt->igmpHdr.code = 0;
  pkt->igmpHdr.cksum = 0;
  pkt->igmpHdr.group = ip_dst;
  pkt->igmpHdr.cksum = csum((unsigned short *)&pkt->igmpHdr, sizeof(pkt->igmpHdr));

  return 14 + 24 + 8;
}

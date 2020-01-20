#include "eka_data_structs.h"
#include "eka_dev.h"

#include "lwip/sockets.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"

void hexDump (const char* desc, void *addr, int len);
void eka_write(EkaDev* dev, uint64_t addr, uint64_t val); // FPGA specific access to mmaped space

struct igmpV2 {
  uint8_t	igmp_type;	/* version & type of IGMP message  */
  uint8_t	igmp_code;	
  uint16_t	igmp_cksum;	/* IP-style checksum               */
  uint32_t	igmp_group;	/* group address being reported    */
  char          pad[14];
} __attribute__((packed));


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


uint16_t ip_sum_calc(uint16_t len_ip_header, uint16_t buff[]) {
  uint16_t word16;
  uint32_t sum=0;
    
  // make 16 bit words out of every two adjacent 8 bit words in the packet
  // and add them up
  for (uint i=0; i<len_ip_header; i=i+2) {
    word16 =((buff[i]<<8)&0xFF00)+(buff[i+1]&0xFF);
    sum = sum + (uint32_t) word16;	
  }
	
  // take only 16 bits out of the 32 bit sum and add up the carries
  while (sum>>16)
    sum = (sum & 0xFFFF)+(sum >> 16);

  // one's complement the result
  sum = ~sum;
	
  return ((uint16_t) sum);
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



uint32_t calc_pseudo_csum (struct ip_hdr* ip_hdr, struct tcp_hdr* tcp_hdr, uint8_t* payload, uint16_t payload_size) {  
  struct pseudo_header {
      uint32_t src_ip;
      uint32_t dst_ip;
      uint8_t placeholder;
      uint8_t proto;
      uint16_t tcp_length;
    };
  char pseudo_packet[2000];
  memset(pseudo_packet, 0, sizeof(struct pseudo_header) + sizeof(struct tcp_hdr) + payload_size);

  struct pseudo_header psh = {};
  psh.src_ip = (uint32_t) *(uint32_t*) &ip_hdr->src;
  psh.dst_ip = (uint32_t) *(uint32_t*) &ip_hdr->dest;
  psh.placeholder = (uint8_t) 0;
  psh.proto = (uint8_t) IPPROTO_TCP;
  psh.tcp_length = htons(sizeof(struct tcp_hdr) + payload_size);

  memcpy(pseudo_packet, (char *) &psh, sizeof(struct pseudo_header));
  memcpy(pseudo_packet + sizeof(struct pseudo_header), tcp_hdr, sizeof(struct tcp_hdr));
  if (payload_size > 0) memcpy(pseudo_packet + sizeof(struct pseudo_header) + sizeof(struct tcp_hdr), payload, payload_size);
  uint32_t pcsum = pseudo_csum( (unsigned short*) pseudo_packet , sizeof(struct pseudo_header) + sizeof(struct tcp_hdr) + payload_size);
  /* hexDump("calc_pseudo_csum tcp_hdr",tcp_hdr,sizeof(struct tcp_hdr)); */
  /* hexDump("pseudo_packet",pseudo_packet,sizeof(struct pseudo_header)+sizeof(struct tcp_hdr)+payload_size); */
  return pcsum;
}

uint32_t calc_empty_pseudo_csum (struct ip_hdr* ip_hdr, struct tcp_hdr* tcp_hdr) {  
  struct pseudo_header {
      uint32_t src_ip;
      uint32_t dst_ip;
      uint8_t placeholder;
      uint8_t proto;
      uint16_t tcp_length;
    };
  uint8_t pseudo_packet[32] = {};

  ((struct pseudo_header*)pseudo_packet)->src_ip = (uint32_t) *(uint32_t*) &ip_hdr->src;
  ((struct pseudo_header*)pseudo_packet)->dst_ip = (uint32_t) *(uint32_t*) &ip_hdr->dest;
  ((struct pseudo_header*)pseudo_packet)->placeholder = 0;
  ((struct pseudo_header*)pseudo_packet)->proto = IPPROTO_TCP;
  ((struct pseudo_header*)pseudo_packet)->tcp_length = htons(20);

  memcpy(pseudo_packet + sizeof(struct pseudo_header), tcp_hdr, sizeof(struct tcp_hdr));
  uint32_t pcsum = csum( (unsigned short*) pseudo_packet , 32);

  return pcsum;
}

uint16_t eka_create_packet (EkaDev* dev, uint8_t* pkt_buf, uint8_t core, uint8_t sess, uint8_t* data, size_t data_len) {
  struct eth_hdr* ethheader = (struct eth_hdr*) pkt_buf;
  struct ip_hdr* ipheader = (struct ip_hdr*) ((uint8_t*) ethheader + sizeof(struct eth_hdr));
  struct tcp_hdr* tcpheader = (struct tcp_hdr*) ((uint8_t*) ipheader + sizeof(struct ip_hdr));

  //  EKA_LOG("src_ip=%0x src_port=%x = %u",dev->core[core].tcp_sess[sess].src_ip,be16toh(dev->core[core].tcp_sess[sess].src_port),be16toh(dev->core[core].tcp_sess[sess].src_port));

  // EKA_LOG("core=%d, sess=%d, sport=0x%x, dport=0x%x (%d)",core,sess,dev->core[core].tcp_sess[sess].src.sin_port,dev->core[core].tcp_sess[sess].dst.sin_port,dev->core[core].tcp_sess[sess].dst.sin_port);
  memcpy(&(ethheader->dest),&(dev->core[core].macda),6);
  memcpy(&(ethheader->src),&(dev->core[core].macsa),6);
  //  EKA_LOG("ethheader->ether_shost = %02x %02x %02x %02x %02x %02x ",ethheader->ether_shost[0],ethheader->ether_shost[1],ethheader->ether_shost[2],ethheader->ether_shost[3],ethheader->ether_shost[4],ethheader->ether_shost[5]);
  ethheader->type = htons(0x0800);
  memcpy(pkt_buf, ethheader, sizeof(struct eth_hdr));

  /* ipheader->ihl = 5; */
  /* ipheader->version = 4; */
  ipheader->_v_hl = 0x45;
  ipheader->_tos = 0;
  ipheader->_len = be16toh( sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + data_len);
  //  EKA_LOG("ipheader->tot_len = %ju = (0x%x)",sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + data_len,be16toh( sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + data_len));
  ipheader->_id = 0;
  ipheader->_offset = 0x0040;
  ipheader->_ttl = 64;
  ipheader->_proto = IPPROTO_TCP;
  ipheader->_chksum = 0;
  * (uint32_t*) &ipheader->src  = dev->core[core].tcp_sess[sess].src_ip;
  * (uint32_t*) &ipheader->dest = dev->core[core].tcp_sess[sess].dst_ip;
  /* * (uint32_t*) &ipheader->src  = be32toh(dev->core[core].tcp_sess[sess].src_ip); */
  /* * (uint32_t*) &ipheader->dest = be32toh(dev->core[core].tcp_sess[sess].dst_ip); */
  ipheader->_chksum = csum((unsigned short *)ipheader, sizeof(struct ip_hdr));
  //  EKA_LOG("ipheader->_chksum = 0x%x",ipheader->_chksum);
  memcpy(pkt_buf + sizeof(struct eth_hdr), ipheader, sizeof(struct ip_hdr));
  tcpheader->src    = be16toh(dev->core[core].tcp_sess[sess].src_port);
  tcpheader->dest   = be16toh(dev->core[core].tcp_sess[sess].dst_port);
  tcpheader->seqno = 0;
  tcpheader->ackno = 0;
  tcpheader->_hdrlen_rsvd_flags = be16toh(0x5000 | TCP_PSH | TCP_ACK);
  /* tcpheader->doff = 5; */
  /* tcpheader->fin = 0; */
  /* tcpheader->syn = 0; */
  /* tcpheader->rst = 0; */
  /* tcpheader->psh = 1; */
  /* tcpheader->ack = 1; */
  /* tcpheader->urg = 0;  */
  tcpheader->wnd = be16toh(dev->core[core].tcp_sess[sess].tcp_window);
  //tcpheader->window = dev->core[core].tcp_sess[sess].tcp_window; //be16toh(eka_get_window_size(dev,core,sess)); // from the driver DB
  tcpheader->chksum = 0;
  tcpheader->urgp = 0;

  uint32_t pcsum = calc_pseudo_csum (ipheader, tcpheader, data, data_len);
  EKA_LOG("Old way pcsum on pseudo header = 0x%08x",calc_pseudo_csum (ipheader, tcpheader, NULL, 0) - be16toh(dev->core[core].tcp_sess[sess].tcp_window));
  tcpheader->chksum = htons((uint16_t) (pcsum>>16) & 0xFFFF);
  tcpheader->urgp = htons((uint16_t) pcsum & 0xFFFF);
  //  memcpy(pkt_buf + sizeof(struct eth_hdr) + sizeof(struct ip_hdr),tcpheader,sizeof(struct tcp_hdr));
  memcpy(pkt_buf + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr),data,data_len);

  /* char src_addr_str[32] = {}; */
  /* sprintf (src_addr_str,"%s",inet_ntoa(dev->core[core].tcp_sess[sess].src.sin_addr)); */
  /* char dst_addr_str[32] = {}; */
  /* sprintf (dst_addr_str,"%s",inet_ntoa(dev->core[core].tcp_sess[sess].dst.sin_addr)); */

  /* EKA_DEBUG("Fast Path: %s:%u --> %s:%u, length =%u", */
  /* 	    src_addr_str,be16toh(dev->core[core].tcp_sess[sess].src.sin_port), */
  /* 	    dst_addr_str,be16toh(dev->core[core].tcp_sess[sess].dst.sin_port), */
  /* 	    data_len); */

  /* EKA_LOG("CORRECT pcsum = 0x%08x",pcsum); */

  if (data_len == 2) {
    hexDump("Payload = 2 pkt",pkt_buf,(uint16_t)sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + data_len);
    printf("Payload = 2 pkt tcpheader->chksum = 0x%x tcpheader->urgp = 0x%x\n",tcpheader->chksum,tcpheader->urgp);
  }

  return (uint16_t)sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + data_len;
}

void eka_convert2empty_ack(uint8_t* pkt) {
  EKA_IPH_LEN_SET(pkt, be16toh(40)); // 
  EKA_IPH_CHKSUM_SET(pkt, 0);
  uint16_t ip_cs = csum((uint16_t*)EKA_IPH(pkt), 20);

  EKA_IPH_CHKSUM_SET(pkt, ip_cs);
  //  printf ("%s: ip_cs = 0x%04x, EKA_IPH_CHKSUM(pkt) = 0x%04x\n",__func__,ip_cs,EKA_IPH_CHKSUM(pkt));
  //  hexDump("eka_convert2empty_ack after ip_cs",pkt,14+20+20);

  EKA_TCPH_UNSET_FLAG(pkt,TCP_PSH);
  EKA_TCPH_CHKSUM_SET(pkt, 0);

  uint32_t tcp_cs = calc_empty_pseudo_csum((struct ip_hdr*)EKA_IPH(pkt),(struct tcp_hdr*)EKA_TCPH(pkt));
  //  printf ("%s: tcp_cs = 0x%04x, EKA_IPH_CHKSUM(pkt) = 0x%04x\n",__func__,tcp_cs,EKA_IPH_CHKSUM(pkt));
  //  printf ("%s: tcp_cs = 0x%08x, shifted tcp_cs  = 0x%04x\n",__func__,tcp_cs,htons((uint16_t) (tcp_cs>>16) & 0xFFFF));

  EKA_TCPH_CHKSUM_SET(pkt, be16toh(tcp_cs));
  //  hexDump("eka_convert2empty_ack after tcp_cs",pkt,14+20+20);
  return;
}


/////////////////////////////////////////////////////////////////

#define PROTO_TCP  6
#define PROTO_UDP 17

uint32_t net_checksum_add(int len, uint8_t *buf)
{
    uint32_t sum = 0;
    int i;

    for (i = 0; i < len; i++) {
	if (i & 1)
	    sum += (uint32_t)buf[i];
	else
	    sum += (uint32_t)buf[i] << 8;
    }
    return sum;
}

uint16_t net_checksum_finish(uint32_t sum)
{
    while (sum>>16)
	sum = (sum & 0xFFFF)+(sum >> 16);
    return ~sum;
}

uint16_t net_checksum_tcpudp(uint16_t length, uint16_t proto,
                             uint8_t *addrs, uint8_t *buf)
{
    uint32_t sum = 0;

    sum += net_checksum_add(length, buf);         // payload
    sum += net_checksum_add(8, addrs);            // src + dst address
    sum += proto + length;                        // protocol & length
    return net_checksum_finish(sum);
}

void net_checksum_calculate(uint8_t *data, int length)
{
    int hlen, plen, proto, csum_offset;
    uint16_t csum;

    if ((data[14] & 0xf0) != 0x40)
	return; /* not IPv4 */
    hlen  = (data[14] & 0x0f) * 4;
    plen  = (data[16] << 8 | data[17]) - hlen;
    proto = data[23];

    switch (proto) {
    case PROTO_TCP:
	csum_offset = 16;
	break;
    case PROTO_UDP:
	csum_offset = 6;
	break;
    default:
	return;
    }

    if (plen < csum_offset+2)
	return;

    data[14+hlen+csum_offset]   = 0;
    data[14+hlen+csum_offset+1] = 0;
    csum = net_checksum_tcpudp(plen, proto, data+14+12, data+14+hlen);
    data[14+hlen+csum_offset]   = csum >> 8;
    data[14+hlen+csum_offset+1] = csum & 0xff;

}

void eka_preload_tcp_tx_header(EkaDev* dev, uint8_t core, uint8_t sess) {
  //  uint64_t base_addr = TCP_FAST_SEND_SP_BASE + EKA_MAX_TCP_PACKET_SIZE * EKA_MAX_TCP_SESSIONS_PER_CORE * core + EKA_MAX_TCP_PACKET_SIZE * sess;

  //  uint8_t hdr[54] = {};
  //  struct eth_hdr* ethheader = (struct eth_hdr*) hdr;
  struct eth_hdr* ethheader = (struct eth_hdr*) dev->core[core].tcp_sess[sess].pktBuf;
  struct ip_hdr* ipheader = (struct ip_hdr*) ((uint8_t*) ethheader + sizeof(struct eth_hdr));
  struct tcp_hdr* tcpheader = (struct tcp_hdr*) ((uint8_t*) ipheader + sizeof(struct ip_hdr));

  memcpy(&(ethheader->dest),&(dev->core[core].macda),6);
  memcpy(&(ethheader->src),&(dev->core[core].macsa),6);
  ethheader->type = htons(0x0800);

  ipheader->_v_hl = 0x45;
  ipheader->_offset = 0x0040;
  ipheader->_ttl = 64;
  ipheader->_proto = IPPROTO_TCP;
  * (uint32_t*) &ipheader->src  = dev->core[core].tcp_sess[sess].src_ip;
  * (uint32_t*) &ipheader->dest = dev->core[core].tcp_sess[sess].dst_ip;

  tcpheader->src    = be16toh(dev->core[core].tcp_sess[sess].src_port);
  tcpheader->dest   = be16toh(dev->core[core].tcp_sess[sess].dst_port);
  tcpheader->_hdrlen_rsvd_flags = be16toh(0x5000 | TCP_PSH | TCP_ACK);

  /* hexDump("Preloaded TCP TX Pkt Hdr",(void*)hdr,sizeof(hdr)); */

  /* uint64_t *buff_ptr = (uint64_t *)hdr; */

  /* for (uint i = 0; i < 6; i++) eka_write(dev, base_addr + i * 8, buff_ptr[i]); */
  
  dev->core[core].tcp_sess[sess].ip_preliminary_pseudo_csum = pseudo_csum((unsigned short *)ipheader, sizeof(struct ip_hdr));
  EKA_LOG("Calculating tcp_preliminary_pseudo_csum");
  dev->core[core].tcp_sess[sess].tcp_preliminary_pseudo_csum = calc_pseudo_csum (ipheader, tcpheader, NULL, 0);
  return;
}

uint16_t pseudo_csum2csum (uint32_t pseudo) {
  uint32_t sum = pseudo;
  while (sum>>16) sum = (sum & 0xFFFF)+(sum >> 16);

  // one's complement the result
  sum = ~sum;
	
  return ((uint16_t) sum);
}

void create_igmp_pkt (char* dst, bool join, uint8_t* macsa, uint32_t ip_src, uint32_t ip_dst) {
  struct eth_hdr* ethheader = (struct eth_hdr*) dst;
  struct ip_hdr* ipheader = (struct ip_hdr*) ((uint8_t*) ethheader + sizeof(struct eth_hdr));
  uint32_t* ip_options = (uint32_t*)((uint8_t*) ipheader + sizeof(struct ip_hdr));
  struct igmpV2* igmpheader = (struct igmpV2*) ((uint8_t*) ipheader + sizeof(struct ip_hdr) + 4); // 4 for Router Aler Options

  uint8_t macda[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x00};
  macda[3] = ((uint8_t*) &ip_dst)[1] & 0x7F;
  macda[4] = ((uint8_t*) &ip_dst)[2];
  macda[5] = ((uint8_t*) &ip_dst)[3];
  
  memcpy (&(ethheader->dest) , macda, 6);
  memcpy (&(ethheader->src)  , macsa, 6);
  ethheader->type = htons(0x0800);
  
  ipheader->_v_hl = 0x46;
  ipheader->_tos = 0xc0;
  ipheader->_len = be16toh(sizeof(struct ip_hdr) + sizeof(struct igmpV2) + 4);

  ipheader->_id = 0;
  ipheader->_offset = 0x0040;
  ipheader->_ttl = 1;
  ipheader->_proto = 0x2; //IPPROTO_IGMP;
  ipheader->_chksum = 0;

  memcpy(&ipheader->src,&ip_src,4);
  memcpy(&ipheader->dest,&ip_dst,4);

  *ip_options = be32toh(0x94040000); 

  ipheader->_chksum = csum((unsigned short *)ipheader, sizeof(struct ip_hdr) + 4);

  igmpheader->igmp_type = join ? 0x16 : 0x17;
  igmpheader->igmp_code = 0;
  igmpheader->igmp_cksum = 0;
  igmpheader->igmp_group = ip_dst;
  igmpheader->igmp_cksum = csum((unsigned short *)igmpheader, sizeof(struct igmpV2));

  return;
}

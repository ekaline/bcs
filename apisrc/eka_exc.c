#include <stdlib.h>
#include <thread>
/* #include <arpa/inet.h> */
/* #include <netinet/if_ether.h> */
/* #include <netinet/ether.h> */
/* #include <netinet/ip.h> */
/* #include <netinet/tcp.h> */
/* #include <net/ethernet.h> */

#include <chrono>
#include <ctime>
#include <iostream>
#include <x86intrin.h>

#include "eka_macros.h"
#include "eka_sn_addr_space.h"
#include "eka_data_structs.h"
//#include "eka_user_channel.h"
#include "eka_exc.h"
#include "eka_dev.h"
//#include "eka_sn_dev.h"

#include "Efc.h"

//#include "eka_exc_debug_module.h"

void eka_write(EkaDev* dev, uint64_t addr, uint64_t val); // FPGA specific access to mmaped space
uint32_t calc_pseudo_csum (EkaIpHdr* ip_hdr, EkaTcpHdr* tcp_hdr, uint8_t* payload, uint16_t payload_size);
void hexDump (const char* desc, void *addr, int len);
uint16_t encode_session_id (uint8_t core, uint8_t sess);
uint8_t session2core (uint16_t id);
uint16_t socket2session (eka_dev_t* dev, int sock_fd);
uint8_t session2sess (uint16_t id);
int decode_session_id (uint16_t id, uint8_t* core, uint8_t* sess);

bool eka_get_tcp_pkt(EkaDev* dev, uint8_t** ptr, uint32_t* pLen);
void eka_next_tcp_pkt(EkaDev* dev);
uint16_t eka_create_packet (EkaDev* dev, uint8_t* pkt_buf, uint8_t core, uint8_t sess, uint8_t* data, size_t data_len);
void set_fast_session(EkaDev* dev, uint8_t core,uint8_t sess);
void efc_run_thread(struct EfcCtx* pEfcCtx, struct EfcRunCtx* pEfcRunCtx); // Used for FIRE REPORTS and FAST PATH 0xf0208 (HB counter) 0xe0218>>32 MASK 32(Fire counter) 
void eka_get_time (char* t);
void efc_run (EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx);
void eka_convert2empty_ack(uint8_t* pkt);
void net_checksum_calculate(uint8_t *data, int length);
unsigned short csum(unsigned short *ptr,int nbytes);
unsigned int pseudo_csum(unsigned short *ptr,int nbytes);

void eka_preload_tcp_tx_header(EkaDev* dev, uint8_t core, uint8_t sess);
uint16_t pseudo_csum2csum (uint32_t pseudo);


int processEpmTrigger(EkaDev* dev, uint8_t* buf, uint32_t len);


inline bool ethernet_broadcast(uint8_t* m) {
  for (uint i = 0; i < 6; i++) if (m[i] != 0xFF) return false;
  printf ("%s: %02x:%02x:%02x:%02x:%02x:%02x is broadcast MAC\n",__func__,m[0],m[1],m[2],m[3],m[4],m[5]);
  return true;
}

void eka_print_hdr(EkaDev* dev, const char* txt, uint8_t* pkt, uint8_t core, uint8_t sess) {
      EKA_LOG("%s: ether_type = 0x%x ip proto = 0x%x, SYN=%u, ACK=%u, SEQ=%u, ACK_SEQ=%u, data_len=%u, tcp_local_seq_num=%u, tcp_remote_seq_num=%u",
	      txt,
      	      EKA_ETH_TYPE(pkt),
      	      EKA_IS_IP4_PKT(pkt) ? EKA_IPH_PROTO(EKA_IPH(pkt)) : 0,
      	      EKA_IS_TCP_PKT(pkt) ? EKA_TCP_SYN(pkt) : 0,
      	      EKA_IS_TCP_PKT(pkt) ? EKA_TCP_ACK(pkt) : 0,
      	      EKA_IS_TCP_PKT(pkt) ? EKA_TCPH_SEQNO(pkt) : 0,
      	      EKA_IS_TCP_PKT(pkt) ? EKA_TCPH_ACKNO(pkt) : 0,
      	      EKA_IS_TCP_PKT(pkt) ? EKA_TCP_PAYLOAD_LEN(pkt) : 0,
      	      EKA_IS_TCP_PKT(pkt) ? dev->core[core].tcp_sess[sess].tcp_local_seq_num : 0,
      	      EKA_IS_TCP_PKT(pkt) ? dev->core[core].tcp_sess[sess].tcp_remote_seq_num : 0
      	      );
}

void defaultOnFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fire_report_buf, size_t size) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  EKA_LOG("%s: Doing nothing");
  return;	 
}


bool find_tcp_core_sess_tx (EkaDev* dev, uint8_t* pkt, uint8_t* core, uint8_t* sess) { // SEND PATH
  /* EKA_LOG  ("Testing : src_ip=0x%08x, dst_ip=0x%08x, dst_port=0x%04x",EKA_IPH_DST(EKA_IPH(pkt)),EKA_IPH_SRC(EKA_IPH(pkt)),EKA_TCPH_SRC(pkt)); */
  for (uint8_t c = 0; c<dev->hw.enabled_cores; c++) {
    //    if (memcmp(EKA_ETH_MACSA(pkt),&dev->core[c].pLwipNetIf->hwaddr,6)) continue;
    for (uint8_t s=0; s<dev->core[c].tcp_sessions; s++) {
     /* EKA_LOG("Checking: src_ip=0x%08x, dst_ip=0x%08x, dst_port=0x%04x",dev->core[c].tcp_sess[s].src_ip,dev->core[c].tcp_sess[s].dst_ip,dev->core[c].tcp_sess[s].dst_port); */
     if (dev->core[c].tcp_sess[s].src_ip   == EKA_IPH_SRC(pkt)  &&
	 dev->core[c].tcp_sess[s].dst_ip   == EKA_IPH_DST(pkt)  &&
	 dev->core[c].tcp_sess[s].src_port == EKA_TCPH_SRC(pkt) &&
	 dev->core[c].tcp_sess[s].dst_port == EKA_TCPH_DST(pkt)) {
	*core = c;
	*sess = s;
	return true;
      }
    }
  }
  hexDump("Unrecognized packet at find_tcp_core_sess_tx",pkt,14+20+20);
  on_error("Not found!: 0x%08x:%u -->  0x%08x:%u",EKA_IPH_SRC(pkt),EKA_TCPH_SRC(pkt),EKA_IPH_DST(pkt),EKA_TCPH_DST(pkt));
  return false;
}

bool find_tcp_core_sess_rx (EkaDev* dev, uint8_t* pkt, uint8_t* core, uint8_t* sess) { // RECV PATH
  /* EKA_LOG  ("Testing : src_ip=0x%08x, dst_ip=0x%08x, dst_port=0x%04x",EKA_IPH_DST(EKA_IPH(pkt)),EKA_IPH_SRC(EKA_IPH(pkt)),EKA_TCPH_SRC(pkt)); */
  //  if (EKA_IS_ARP_PKT(pkt)) {
  if (! EKA_IS_TCP_PKT(pkt)) {
    *core = 0;
    *sess = 0;
    return true;
  }

  for (uint8_t c=0;c<dev->hw.enabled_cores;c++) {
    for (uint8_t s = 0; s < dev->core[c].tcp_sessions; s++) {
      /* EKA_LOG("Checking: 0x%08x:%u --> 0x%08x:%u vs 0x%08x:%u --> 0x%08x:%u", */
      /* 	      dev->core[c].tcp_sess[s].src_ip, */
      /* 	      dev->core[c].tcp_sess[s].src_port, */
      /* 	      dev->core[c].tcp_sess[s].dst_ip, */
      /* 	      dev->core[c].tcp_sess[s].dst_port, */
      /* 	      EKA_IPH_DST(pkt), */
      /* 	      EKA_TCPH_DST(pkt), */
      /* 	      EKA_IPH_SRC(pkt), */
      /* 	      EKA_TCPH_SRC(pkt) */
      /* 	      ); */
      if (dev->core[c].tcp_sess[s].src_ip   == EKA_IPH_DST(pkt) &&
	  dev->core[c].tcp_sess[s].dst_ip   == EKA_IPH_SRC(pkt) &&
	  dev->core[c].tcp_sess[s].src_port == EKA_TCPH_DST(pkt) &&
	  dev->core[c].tcp_sess[s].dst_port == EKA_TCPH_SRC(pkt)) {
	*core = c;
	*sess = s;
	return true;
      }
    }
  }
  hexDump("Unmatched packet",pkt,64);
  //  on_error("NO MATCH!!!");
  return false;
}

inline bool is_my_mac_addr (EkaDev* dev, uint8_t* pkt, uint8_t c) {
  if (ethernet_broadcast(EKA_ETH_MACDA(pkt))) return true;
  if (memcmp(EKA_ETH_MACDA(pkt),&dev->core[c].pLwipNetIf->hwaddr,6) == 0) return true;
  return false;
}



/* void ekaLwipPollThread (EkaDev* dev) { */
/*   if (dev == NULL) on_error ("dev == NULL"); */
/*   dev->ekaLwipPollThreadIsUp = true; */
/*   EKA_LOG("ekaLwipPollThreadIsUp"); */
/*   while (dev->exc_active) { */
/*     uint8_t* buf; */
/*     uint32_t len; */
/*     if (! eka_get_tcp_pkt(dev, &buf, &len)) continue; */

/*     uint8_t core,sess; */
/*     if (! find_tcp_core_sess_rx (dev, buf, &core, &sess)) on_error("Session not found"); */

/*     dev->core[core].tcp_sess[sess].tcp_remote_seq_num = EKA_TCPH_SEQNO(buf) + EKA_TCP_PAYLOAD_LEN(buf); */

/*     /\* hexDump("eka_poll",buf,len); *\/ */
/*     uint delay_cnt = 0; */
/*     while (dev->exc_active && EKA_IS_TCP_PKT(buf) && (EKA_TCPH_ACKNO(buf) > dev->core[core].tcp_sess[sess].tcp_local_seq_num) && (! EKA_TCP_SYN(buf)) && (! EKA_TCP_FIN(buf))) { */
/*       usleep(1); */
/*       if (++delay_cnt % 5000 == 0) {	 */
/* 	EKA_LOG ("Sess %u: Out of order ACK: EKA_TCPH_ACKNO(buf) = 0x%x, tcp_local_seq_num = %x, EKA_TCP_SYN(buf) = %d, EKA_TCP_FIN(buf) = %d len = %u", */
/* 		 sess, EKA_TCPH_ACKNO(buf),dev->core[core].tcp_sess[sess].tcp_local_seq_num,EKA_TCP_SYN(buf),EKA_TCP_FIN(buf), len ); */
/* #ifdef EXC_LAB_TEST */
/* 	EKA_LOG("Sess %u: expected Dummy pkt.cnt = %ju",sess,dev->exc_debug->get_expected_dummy_pkt_cnt(core,sess)); */
/* #endif */
/* 	on_error */
/* 	  //EKA_LOG */
/* 	  ("WARNING: Sess %u: delaying for longer than 5ms",sess); */
/*       } */
/*     } */
/*     struct pbuf* p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM); */
/*     if (p == NULL) on_error ("failed to get new PBUF"); */
/*     memcpy(p->payload,buf,len); */
/*     /\* char t[100]; *\/ */
/*     /\* eka_get_time(t); *\/ */
/*     /\* EKA_LOG("%s: RCV",t); *\/ */
/*     /\* hexDump("eka_poll sending to LWIP stack",buf,len); *\/ */
/*     dev->core[core].pLwipNetIf->input(p,dev->core[core].pLwipNetIf); */

/*     eka_next_tcp_pkt(dev); */
/*   } */

/*   return; */
/* } */


void ekaProcesTcpRx (EkaDev* dev, uint8_t* buf, uint32_t len) {
    uint8_t core,sess;

    if (! find_tcp_core_sess_rx (dev, buf, &core, &sess)) on_error("Session not found");

    dev->core[core].tcp_sess[sess].tcp_remote_seq_num = EKA_TCPH_SEQNO(buf) + EKA_TCP_PAYLOAD_LEN(buf) + EKA_TCP_SYN(buf);

    /* hexDump("eka_poll",buf,len); */
    /* uint delay_cnt = 0; */
/*     while (dev->exc_active && EKA_IS_TCP_PKT(buf) && (EKA_TCPH_ACKNO(buf) > dev->core[core].tcp_sess[sess].tcp_local_seq_num) && (! EKA_TCP_SYN(buf)) && (! EKA_TCP_FIN(buf))) { */
/*       usleep(1); */
/*       if (++delay_cnt % 5000 == 0) {	 */
/* 	EKA_LOG ("Sess %u: Out of order ACK: EKA_TCPH_ACKNO(buf) = 0x%x, tcp_local_seq_num = %x, EKA_TCP_SYN(buf) = %d, EKA_TCP_FIN(buf) = %d len = %u", */
/* 		 sess, EKA_TCPH_ACKNO(buf),dev->core[core].tcp_sess[sess].tcp_local_seq_num,EKA_TCP_SYN(buf),EKA_TCP_FIN(buf), len ); */
/* #ifdef EXC_LAB_TEST */
/* 	EKA_LOG("Sess %u: expected Dummy pkt.cnt = %ju",sess,dev->exc_debug->get_expected_dummy_pkt_cnt(core,sess)); */
/* #endif */
/* 	on_error ("WARNING: Sess %u: delaying for longer than 5ms",sess); */
/*       } */
/*     } */
    struct pbuf* p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
    if (p == NULL) on_error ("failed to get new PBUF");
    memcpy(p->payload,buf,len);
    dev->core[core].pLwipNetIf->input(p,dev->core[core].pLwipNetIf);
    return;
}

err_t ekaLowLevelSend_full_pkt(EkaDev* dev, uint8_t core,  uint8_t sess, void *buf, int len) {
  //  hexDump("ekaLowLevelSend_full_pkt",buf,len);
  if (dev == NULL) return ERR_CLSD;

  if (len > EKA_MAX_TCP_PACKET_SIZE) on_error("Size of buffer to send (=%d) exceeds EKA_MAX_TCP_PACKET_SIZE (%d)",(int)len,EKA_MAX_TCP_PACKET_SIZE);

  union tcp_fast_send_desc td = {};
  td.tcpd.src_index = EKA_MAX_TCP_PACKET_SIZE * (sess + EKA_MAX_TCP_SESSIONS_PER_CORE);
  td.tcpd.src_index += EKA_MAX_TCP_PACKET_SIZE * EKA_MAX_TCP_SESSIONS_PER_CORE * core;
  td.tcpd.length = (uint8_t) len; // total packet len MGUST BE < 256 BYTES !!!
  td.tcpd.core = core;
  td.tcpd.session = sess;
  td.tcpd.send_attr = 0x40; // dont modify TCP sequences and checksum

  //#########################################################
  //#ifdef EKA_WC
#if 0
  uint8_t __attribute__ ((aligned(0x100)))  pktBuf[256] = {};
  memcpy(pktBuf,buf,len);
  volatile uint8_t* base_addr = dev->txPktBuf + td.tcpd.src_index;

  //---------------------------------------------------------
    int iter = len/64 + !!(len%64); // num of 64 Byte words of the packet 
    for (int i = 0; i < iter; i++) {
      //      memcpy((uint8_t*)(base_addr + i * 64), ((uint8_t*)buf) + i * 64, 64);
      //      EKA_LOG("%4u: len=%u, iter = %u, base_addr=%p, wr_ptr =%p",i,len, iter, (uint8_t*)(base_addr + i * 64),dev->txPktBuf);fflush(stderr);
      memcpy((uint8_t*)(base_addr + i * 64), ((uint8_t*)pktBuf) + i * 64, 64);
      _mm_sfence();
    }
  //---------------------------------------------------------
#else 
  uint64_t base_addr = TCP_FAST_SEND_SP_BASE + td.tcpd.src_index;
  int iter = len/8 + !!(len%8); // num of 8Byte words of the packet
  uint64_t *buff_ptr = (uint64_t *)buf;
  //  for (int z=0; z<iter; ++z) eka_write(dev, base_addr + z*8, htobe64(buff_ptr[z]));
  for (int z=0; z<iter; ++z) eka_write(dev, base_addr + z*8, buff_ptr[z]);
#endif
  //---------------------------------------------------------

  eka_write(dev, TCP_FAST_SEND_DESC, td.desc);
  /* EKA_LOG("%u Bytes Pkt is sent",len); */
  //  hexDump("ekaLowLevelSend_full_pkt sent",buf,len);

  return ERR_OK;
}

void eka_send_pkt(EkaDev* dev, uint8_t core,  uint8_t sess, void *buf, int len) {
  //  hexDump("eka_send_igmp_join",buf,len);
  ekaLowLevelSend_full_pkt(dev, core,  sess, buf, len);
}

err_t ekaLowLevelSend_payload(EkaDev* dev, uint8_t core,  uint8_t sess, void *buf, int len) {
  /* hexDump("ekaLowLevelSend_payload",buf,len); */
  if (dev == NULL) return ERR_CLSD;

  //  if (len > EKA_MAX_TCP_PACKET_SIZE - 1) on_error("Size of buffer to send (=%d) exceeds EKA_MAX_TCP_PACKET_SIZE (%d)",(int)len,EKA_MAX_TCP_PACKET_SIZE);
  if (len + 14 + 20 + 20 > 255) on_error("Size of buffer to send (=%d) exceeds EKA_MAX_TCP_PACKET_SIZE (%d)",(int)len,EKA_MAX_TCP_PACKET_SIZE);

  uint16_t ip_tot_len = sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + len;
  uint16_t ip_csum = pseudo_csum2csum(dev->core[core].tcp_sess[sess].ip_preliminary_pseudo_csum + be16toh(ip_tot_len));

  uint32_t tcp_pseudo_csum = 
    dev->core[core].tcp_sess[sess].tcp_preliminary_pseudo_csum + 
    be16toh(dev->core[core].tcp_sess[sess].tcp_window) -
    be16toh(sizeof(struct tcp_hdr)) +
    be16toh(sizeof(struct tcp_hdr) + len) +
    pseudo_csum((uint16_t*)buf,len);

#if 0
  EKA_LOG("psh_psum = 0x%08x, flags_psum = 0x%08x, tcp_window_psum = 0x%08x, payload_psum = 0x%08x, total = 0x%08x",
	  dev->core[core].tcp_sess[sess].tcp_preliminary_pseudo_csum,
	  /* be16toh(0x5000 | TCP_PSH | TCP_ACK), */
	  be16toh(dev->core[core].tcp_sess[sess].tcp_window),
	  pseudo_csum((uint16_t*)buf,len),
	  dev->core[core].tcp_sess[sess].tcp_preliminary_pseudo_csum+be16toh(0x5000 | TCP_PSH | TCP_ACK)+be16toh(dev->core[core].tcp_sess[sess].tcp_window)+pseudo_csum((uint16_t*)buf,len)
	  );
#endif


  union tcp_fast_send_desc td = {};
  td.tcpd.src_index = EKA_MAX_TCP_PACKET_SIZE * sess;
  td.tcpd.src_index += EKA_MAX_TCP_PACKET_SIZE * EKA_MAX_TCP_SESSIONS_PER_CORE * core;
  td.tcpd.length = (uint8_t) ip_tot_len + sizeof(struct eth_hdr); // total packet len MUST BE < 256 BYTES !!!
  td.tcpd.ip_checksum =  be16toh(ip_csum);
  td.tcpd.core = core;
  td.tcpd.session = sess;
  td.tcpd.send_attr = 0; // fix TCP sequences and checksum

  union word_0 {
    uint64_t w_0;
    struct word_line {
      uint16_t window;
      uint16_t tcp_csum;
      uint16_t tcp_urgp;
      uint16_t data;
    } __attribute__((packed)) wl;
  } __attribute__((packed));
  
  union word_0 w0 = {};
  w0.wl.window   = be16toh(dev->core[core].tcp_sess[sess].tcp_window);
  w0.wl.tcp_csum = be16toh((uint16_t) (tcp_pseudo_csum>>16) & 0xFFFF);
  w0.wl.tcp_urgp = be16toh((uint16_t) (tcp_pseudo_csum      & 0xFFFF));
  w0.wl.data     = * (uint16_t*) buf;

#ifdef EKA_WC
  volatile uint8_t* base_addr = dev->txPktBuf + td.tcpd.src_index; 

  uint64_t* pPktBuf = (uint64_t*)dev->core[core].tcp_sess[sess].pktBuf;
  pPktBuf[6] = w0.w_0;

  int words8B = 1;
  if (len > 2) {
    words8B += (len - 2)/8 + !!((len - 2) % 8); // num of 8Byte words of the packet
    uint64_t *buff_ptr = (uint64_t *)((uint8_t*)buf + 2);
    for (int w8 = 0; w8 < words8B - 1; w8 ++) pPktBuf[7 + w8] = buff_ptr[w8];
  }

  uint totalPktLen = 14 + 20 + 20 + len;
  uint words64 = totalPktLen / 64 + !! (totalPktLen % 64);
  if (words64 > 4) on_error("words64 = %u",words64);

  //  memcpy((uint8_t*)base_addr,pPktBuf,words64 * 64);

  for (uint w64 = 0; w64 < words64; w64++) {
     memcpy((uint8_t*)base_addr,pPktBuf,64);
     base_addr += 64;
     pPktBuf += 8;
     _mm_mfence();
  }


  //  EKA_LOG("len = %u, totalPktLen = %u, words64 = %u, copied %u Bytes",len, totalPktLen, words64, words64 * 64);

#else

  uint64_t base_addr = TCP_FAST_SEND_SP_BASE + td.tcpd.src_index;

  eka_write(dev, base_addr + 6*8, w0.w_0);
  
  len -= 2;
  if (len > 0) {
    int iter = len/8 + !!(len%8); // num of 8Byte words of the packet
    uint64_t *buff_ptr = (uint64_t *)((uint8_t*)buf + 2);
    for (int z=0; z<iter; ++z) eka_write(dev, base_addr + (z+7)*8, buff_ptr[z]);
  }
#endif

  eka_write(dev, TCP_FAST_SEND_DESC, td.desc);
  return ERR_OK;
}



err_t ekaLwipSend(struct netif *netif, struct pbuf *p) {
  /* hexDump("ekaLwipSend",p->payload,p->len); */
  EkaDev* dev = ((struct LwipNetifState*)netif->state)->pEkaDev;
  if (dev == NULL) return ERR_CLSD;

  if (p == NULL) on_error("struct pbuf *p == NULL");
  if (p->len != p->tot_len) on_error("p->len (%u) != p->tot_len (%u) --- IP fragmentation is not supported",p->len,p->tot_len);
  if (p->len < 1) on_error("p->len < 1");//return ERR_OK;

  uint8_t* pkt = (uint8_t*) p->payload;
  /* EKA_LOG("XYU SEQNO = %u, EKA_TCP_PAYLOAD_LEN(pkt)=%u",EKA_TCPH_SEQNO(pkt),EKA_TCP_PAYLOAD_LEN(pkt)); */
  uint8_t core = 0;
  uint8_t sess = 0;

  //  hexDump("ekaLwipSend RAW",(uint8_t*) p->payload,p->len);

  if (EKA_IS_ARP_PKT(pkt)) {
    // Send as is
    /* hexDump("ARP or UDP pkt",pkt,14+20); */
  } else if (EKA_IS_UDP_PKT(pkt)) {
    sess = EKA_UDPH_DST(pkt) % EKA_MAX_TCP_SESSIONS_PER_CORE;
    //    EKA_LOG("EKA_UDPH_DST(pkt) = %u, sess = %u",EKA_UDPH_DST(pkt), sess);
  } else if (EKA_IS_TCP_PKT(pkt)) {
    /* hexDump("ekaLwipSend TCP",(uint8_t*) p->payload,p->len); */
    if(! find_tcp_core_sess_tx(dev,pkt,&core,&sess)) on_error ("Unrecognized TCP pkt");
    /* eka_print_hdr(dev,"TCP PKT",pkt,core,sess); */

    // -------------------------------------------------------------------
    // Updating HW for ACK sequence on every packet
    //    EKA_LOG("Updating HW for ACK_SEQ %u",EKA_TCPH_ACKNO(pkt));

    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sess;
    desc.td.target_idx = (uint32_t)sess;
    eka_write(dev,0x60000 + 0x1000*core + 8 * (sess*2),EKA_TCPH_ACKNO(pkt));
    eka_write(dev,0x6f000 + 0x100*core,desc.desc);  
    /* char t[100]; */
    /* eka_get_time(t); */
    //EKA_LOG("Sending ACK %u to FPGA core = %u, sess=%u, addr = 0x%jx",EKA_TCPH_ACKNO(pkt),core,sess,0x60000 + 0x1000*core + 8 * sess);

    dev->core[core].tcp_sess[sess].tcp_window =  EKA_TCPH_WND(pkt);
    // -------------------------------------------------------------------
    // Updating HW for Window and TX Sequence on SYN
    if (EKA_TCP_SYN(pkt)) {
      dev->core[core].tcp_sess[sess].tcp_local_seq_num = EKA_TCPH_SEQNO(pkt) + 1;
      EKA_LOG("Updating HW for core %u sess %u SEQUENCE %u",core,sess,dev->core[core].tcp_sess[sess].tcp_local_seq_num);
      eka_write(dev,0x30000 + 0x1000*core + 8 * (sess*2),dev->core[core].tcp_sess[sess].tcp_local_seq_num);
      eka_write(dev,0x3f000 + 0x100*core,desc.desc);

      eka_write(dev,0xe0318,EKA_TCPH_WND(pkt));
      //      dev->core[core].tcp_sess[sess].send_seq2hw = false;
    }

    // Dropping or converting to empty ACKs all Non-Retransmit (Dummy) packets
    if ((! EKA_TCP_SYN(pkt))                                                                                &&
	(! EKA_TCP_FIN(pkt))                                                                                &&
	(EKA_TCP_PAYLOAD_LEN(pkt) > 0)                                                                      && 
	(EKA_TCPH_SEQNO(pkt) >= dev->core[core].tcp_sess[sess].tcp_local_seq_num)    //not retransmit
	){
      /* eka_print_hdr(dev,"Dropped Dummy pkt",pkt,core,sess); */
      /* hexDump("Dropped Dummy pkt",pkt,p->len); */
      dev->core[core].tcp_sess[sess].tx_driver_bytes   += EKA_TCP_PAYLOAD_LEN(pkt);
      dev->core[core].tcp_sess[sess].tcp_local_seq_num = EKA_TCPH_SEQNO(pkt) + EKA_TCP_PAYLOAD_LEN(pkt);
      return ERR_OK;

      // Converting to empty ACK (To consider reducing the payload ...)
      //      dev->core[core].tcp_sess[sess].tcp_local_seq_num = EKA_TCPH_SEQNO(pkt) + EKA_TCP_PAYLOAD_LEN(pkt);
/*       eka_print_hdr(dev,"Dummy Pkt to be converted to Empty ACK or dropped",pkt,core,sess); */


/*       if (EKA_TCPH_ACKNO(pkt) >= dev->core[core].tcp_sess[sess].tcp_remote_seq_num) { */
/* 	// No need send Dup Ack */
/* 	/\* eka_print_hdr(dev,"Dropped Dummy pkt",pkt,core,sess); *\/ */
/* 	//	return ERR_OK; */
/*       } */

/*       EKA_TCPH_ACKNO_SET(pkt,dev->core[core].tcp_sess[sess].tcp_remote_seq_num); */
/*       //      EKA_TCPH_SEQNO_SET(pkt,dev->core[core].tcp_sess[sess].tcp_local_seq_num); // The bug is HERE */
/*       EKA_TCPH_SEQNO_SET(pkt,1234567); // The bug is HERE */
/* #if 0 */
/*       eka_convert2empty_ack(pkt); */
/*       p->len = 14 + 20 + 20; */
/* #else */
/*       return ERR_OK; */
/*       EKA_IPH_DST_SET(pkt,0x11223344); */
/* #endif //0 */
      /* eka_print_hdr(dev,"Empty ACK after conversion",pkt,core,sess); */

    } else {
      if (EKA_TCP_PAYLOAD_LEN(pkt) > 0) {
	/* hexDump("Passing to ekaLowLevelSend",pkt,p->len); */
	/* eka_print_hdr(dev,"WARNING: real re-transmit",pkt,core,sess); */
      }
    }
  } else {
    hexDump("Unrecognized Protocol pkt",pkt,14+20+20);
    on_error ("Unrecognized Protocol pkt");
  }
  /* EKA_LOG("Sending:"); */
  /* eka_print_hdr(dev,pkt,core,sess); */

  /* hexDump("at ekaLwipSend: passing to ekaLowLevelSend_full_pkt",pkt,p->len); */

  return ekaLowLevelSend_full_pkt(dev, core, sess, pkt, p->len);
}

//#################################################################################

err_t eka_netif_init (struct netif *netif) {
  EkaDev* dev = ((struct LwipNetifState*)netif->state)->pEkaDev;
  EKA_LOG("Initializing");

  return ERR_OK;
}

void ekaLwipInit (void * arg) {
  sys_sem_t *init_sem;
  LWIP_ASSERT("arg != NULL", arg != NULL);
  init_sem = (sys_sem_t*)arg;
  sys_sem_signal(init_sem);
  //  printf("%s: Im here\n",__func__);
}

void ekaExcInitLwip (EkaDev* dev) {
  EKA_LOG("Initializing LWIP");
  dev->hw.enabled_cores = 1;
  if (dev->lwip_inited) return;
  dev->lwip_inited = true;
  dev->exc_active = true;

  err_t err;
  err = sys_sem_new((sys_sem_t*)&dev->lwip_sem, 0);

  LWIP_ASSERT("failed to create init_sem", err == ERR_OK);

  LWIP_UNUSED_ARG(err);
  tcpip_init(ekaLwipInit,(sys_sem_t*)&dev->lwip_sem);
  /* we have to wait for initialization to finish before */
  /* calling update_adapter()! */
  sys_sem_wait((sys_sem_t*)&dev->lwip_sem);
  sys_sem_free((sys_sem_t*)&dev->lwip_sem);

  netconn_thread_init();
  //  default_netif_poll();

  for (uint8_t c=0;c<dev->hw.enabled_cores;c++) {
    if ((dev->core[c].pLwipNetIf = (struct netif*) calloc(1,sizeof(struct netif))) == NULL) on_error("cannot allocate pLwipNetIf");

    if ((dev->core[c].pLwipNetIf->state = calloc(1,sizeof(struct LwipNetifState))) == NULL) on_error("cannot allocate LwipNetifState");

    ((struct LwipNetifState*)dev->core[c].pLwipNetIf->state)->pEkaDev = dev;
    ((struct LwipNetifState*)dev->core[c].pLwipNetIf->state)->lane = c;


    for (int i = 0; i < 6; ++i) dev->core[c].pLwipNetIf->hwaddr[i] = dev->core[c].macsa[i];

    dev->core[c].pLwipNetIf->input = tcpip_input;
    dev->core[c].pLwipNetIf->output = etharp_output;
    dev->core[c].pLwipNetIf->linkoutput = ekaLwipSend;
    dev->core[c].pLwipNetIf->mtu = 1500;
    dev->core[c].pLwipNetIf->hwaddr_len = 6;

    EKA_LOG("running netif_add with ip=%s",inet_ntoa(*((struct in_addr*) &dev->core[c].src_ip)));

    netif_add(dev->core[c].pLwipNetIf,
    	      (const ip4_addr_t *) & dev->core[c].src_ip,
    	      NULL, // netmask
    	      NULL, // gw
    	      dev->core[c].pLwipNetIf->state,
    	      eka_netif_init,
    	      tcpip_input);
    dev->core[c].pLwipNetIf->flags |= NETIF_FLAG_UP | NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;
    dev->core[c].pLwipNetIf->output = etharp_output;

    //    netif_set_default (dev->core[c].pLwipNetIf);

    EKA_LOG("After netif_add:",((struct LwipNetifState*)dev->core[c].pLwipNetIf->state)->lane);
    uint8_t* m = (uint8_t*) & dev->core[c].pLwipNetIf->hwaddr;
    EKA_LOG("\tMAC = %02x:%02x:%02x:%02x:%02x:%02x",m[0],m[1],m[2],m[3],m[4],m[5]);
    EKA_LOG("\tIP =%s",     inet_ntoa(dev->core[c].pLwipNetIf->ip_addr));
    EKA_LOG("\tNetmask =%s",inet_ntoa(dev->core[c].pLwipNetIf->netmask));
    EKA_LOG("\tGateway =%s",inet_ntoa(dev->core[c].pLwipNetIf->gw));

     //    memcpy(&dev->core[c].macsa,&dev->core[c].pLwipNetIf->hwaddr,6);
    netif_set_up(dev->core[c].pLwipNetIf);
    netif_set_link_up(dev->core[c].pLwipNetIf);
    EKA_LOG("NIC %u is %s, link is %s",c, netif_is_up(dev->core[c].pLwipNetIf) ? "UP" : "DOWN",netif_is_link_up(dev->core[c].pLwipNetIf)  ? "UP" : "DOWN");
  }

#ifdef EXC_LAB_TEST
  dev->exc_debug = new exc_debug_module(dev,"EXCPKT");
#endif

  return;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ssize_t eka_recv(EkaDev* dev, uint16_t sess_id, void *buffer, size_t size) {
  uint8_t c = session2core(sess_id);
  uint8_t s = session2sess(sess_id);
  //  EKA_LOG("sock_fd = %d",dev->core[c].tcp_sess[s].sock_fd);
  return lwip_recv(dev->core[c].tcp_sess[s].sock_fd, buffer, size, 0);
}

int eka_socket_close(EkaDev* dev, uint16_t sess_id) {
  //  return pico_socket_close(session2picosock(dev,sess_id));
  uint8_t c = session2core(sess_id);
  uint8_t s = session2sess(sess_id);
  /* EKA_LOG("Shutting down socket %d for core%u sess%u",dev->core[c].tcp_sess[s].sock_fd,c,s); */
  /* lwip_shutdown(dev->core[c].tcp_sess[s].sock_fd,SHUT_RDWR); */
  /* usleep (10000); */
  EKA_LOG("Closing socket %d for core%u sess%u",dev->core[c].tcp_sess[s].sock_fd,c,s);
  int rc = lwip_close(dev->core[c].tcp_sess[s].sock_fd);
  usleep (1000);
  return rc;
}

int send_dummy_pkt(EkaDev* dev,uint8_t core, uint8_t sess, uint8_t* pkt,uint size) {
  /* EKA_LOG("Inserting %u bytes",size); */
  /* hexDump("send_dummy_pkt",pkt,size); */
  /* fflush(stderr); */
  /* fflush(stdout); */

#ifdef EXC_LAB_TEST
  //  dev->exc_debug->check_dummy_pkt(pkt,core,sess,size);
#endif // EXC_LAB_TEST

  /* uint8_t xyu[2000] = {}; */
  /* for (uint i = 0; i < size; i++) xyu[i] = 'X'; */
  //  int sent = lwip_write(dev->core[core].tcp_sess[sess].sock_fd,xyu,size);

  uint send_ptr = 0;
  while (dev->exc_active && (send_ptr < size - 1)) {
    int sent = lwip_write(dev->core[core].tcp_sess[sess].sock_fd,&pkt[send_ptr],size - send_ptr);
    //    int sent = lwip_write(dev->core[core].tcp_sess[sess].sock_fd,xyu,size - send_ptr);
    /* EKA_LOG("Tried %d, Sent %d Dummy bytes",size - send_ptr,sent); */
    if (sent <= 0) {
      usleep(1);
      continue;
    }
    send_ptr += sent;
  }
  return size;
}

ssize_t eka_send(EkaDev* dev, uint16_t sess_id, void *buf, size_t size) {
  if (size < 1) return size;

  uint8_t core = session2core(sess_id);
  uint8_t sess = session2sess(sess_id);

  if (dev->core[core].tcp_sess[sess].fast_path_bytes < dev->core[core].tcp_sess[sess].tx_driver_bytes) 
    on_error("fast_path_bytes (%ju) < tx_driver_bytes(%ju)",dev->core[core].tcp_sess[sess].fast_path_bytes,dev->core[core].tcp_sess[sess].tx_driver_bytes);

  if (dev->core[core].tcp_sess[sess].fast_path_bytes > dev->core[core].tcp_sess[sess].fast_path_dummy_bytes ||
      (dev->core[core].tcp_sess[sess].fast_path_dummy_bytes + dev->core[core].tcp_sess[sess].fire_dummy_bytes) > dev->core[core].tcp_sess[sess].tx_driver_bytes)
    return 0; // too high tx rate -- Back Pressure
  

  uint size2send = (size <= EKA_MAX_TCP_PAYLOAD_SIZE + 1) ? size : EKA_MAX_TCP_PAYLOAD_SIZE;
  dev->core[core].tcp_sess[sess].fast_path_bytes += size2send;

  if (size2send == 1) EKA_LOG("WARNING: 1Byte payload might be considered as Keep Alive");

  /* EKA_LOG("size = %u, size2send = %u, core = %u, sess = %u",size,size2send,core,sess); */
  /* hexDump("eka_send",buf,size); */

  ekaLowLevelSend_payload(dev, core, sess, buf, size2send);

  return size2send;
}


uint16_t eka_connect (EkaDev* dev, int sock, const struct sockaddr *d, socklen_t unused) {
  uint16_t sess_id = socket2session (dev, sock);
  uint8_t core = session2core(sess_id);
  uint8_t sess = session2sess(sess_id);

  struct sockaddr_in dst = {}; // rebuilding for LWIP compatability
  dst.sin_addr        = ((struct sockaddr_in *)d)->sin_addr;
  dst.sin_port        = ((struct sockaddr_in *)d)->sin_port;
  dst.sin_family      = AF_INET;

  //  dev->core[core].tcp_sess[sess].src_ip   = dev->core[core].src_ip;

  dev->core[core].tcp_sess[sess].dst_ip = *(uint32_t*) & ((struct sockaddr_in *)d)->sin_addr;
  dev->core[core].tcp_sess[sess].dst_port = be16toh  (dst.sin_port);

  char src_addr_str[20] = {};
  sprintf (src_addr_str,"%s",inet_ntoa(*(struct addr_in*) &dev->core[core].src_ip));
  EKA_LOG("core=%d: %s --> %s:%d, sock=%d",core,src_addr_str,inet_ntoa(dst.sin_addr),be16toh(dst.sin_port),sock);

  /* int rc; */
  /* if ((rc = lwip_connect(sock, (struct sockaddr *)&dst, sizeof(struct sockaddr_in))) < 0)  */
  /*   on_error("socket connect failed for core %u, sess %u with rc=%d",core,sess,rc); //BLOCKING SOCKET */
  /* EKA_LOG("TCP connected for core %u, sess %u",core,sess); */

  uint8_t* m; // pointer to get MAC_DA from the ARP table
  uint32_t* ip_ptr; // pointing to IP ADDR at the same ARP entry (if found)

  if (dev->core[core].macda_set_externally) {
    m = dev->core[core].macda;
    EKA_LOG("AddingStatic ARP entry %s --> %02x:%02x:%02x:%02x:%02x:%02x",inet_ntoa(dst.sin_addr),m[0],m[1],m[2],m[3],m[4],m[5]);

    etharp_add_static_entry((const ip4_addr_t*) &dst.sin_addr, (struct eth_addr *)m);
  } else {
    EKA_LOG("Sending ARP request to %s",inet_ntoa(dst.sin_addr));

    if (etharp_request(dev->core[core].pLwipNetIf, (const ip4_addr_t*)&dst.sin_addr) != ERR_OK) on_error("etharp_query failed");
  }


  bool arp_found = false;
  for (uint trials = 0; trials < 1000000; trials ++) {
    if (etharp_find_addr(dev->core[core].pLwipNetIf,(const ip4_addr_t*)&dst.sin_addr,(eth_addr**)&m,(const ip4_addr_t**)&ip_ptr) >= 0) {
      arp_found = true;
      break;
    }
    usleep(1);
  }
  if (! arp_found) on_error("%s is not in the ARP table",inet_ntoa(dst.sin_addr));
  
  EKA_LOG("ARP TABLE: %s --> %02x:%02x:%02x:%02x:%02x:%02x",inet_ntoa(*(struct in_addr*) ip_ptr),m[0],m[1],m[2],m[3],m[4],m[5]);
  for (uint i=0; i<6;i++) dev->core[core].macda[i] = m[i]; // taking MAC_DA for FPGA fires

  set_fast_session(dev,core,sess);

  dev->core[core].tcp_sess[sess].fast_path_bytes = 0;
  dev->core[core].tcp_sess[sess].tx_driver_bytes = 0;
  dev->core[core].tcp_sess[sess].fast_path_dummy_bytes = 0;
  dev->core[core].tcp_sess[sess].fire_dummy_bytes = 0;

  int rc;
  EKA_LOG("trying to connect to: %s:%u",inet_ntoa(dst.sin_addr),be16toh(dst.sin_port));
  fflush(stderr);
  if ((rc = lwip_connect(sock, (struct sockaddr *)&dst, sizeof(struct sockaddr_in))) < 0) 
    on_error("socket connect failed for core %u, sess %u with rc=%d",core,sess,rc); //BLOCKING SOCKET
  EKA_LOG("TCP connected for core %u, sess %u to %s:%u",core,sess,inet_ntoa(dst.sin_addr),be16toh(dst.sin_port));
  fflush(stderr);

  if (lwip_fcntl(dev->core[core].tcp_sess[sess].sock_fd, F_SETFL, lwip_fcntl(dev->core[core].tcp_sess[sess].sock_fd, F_GETFL, 0) | O_NONBLOCK) < 0) on_error ("Cant set O_NONBLOCK on sock %d",dev->core[core].tcp_sess[sess].sock_fd);
  //  EKA_LOG("Socket %d is set to O_NONBLOCK",sock);

  eka_preload_tcp_tx_header(dev,core,sess);
#ifndef EKA_WC
  
#endif
  return sess_id;
}

int eka_socket(EkaDev* dev, uint8_t core) { 
  if (dev == NULL) on_error ("dev = NULL");
  EKA_LOG("Openning a socket for core %u",core);
  fflush(stderr);

  if (!dev->exc_inited) {
    EKA_LOG("Launching efc_run_thread on 1st socket openning");
    fflush(stderr);

    struct EfcCtx efcCtx= {};
    efcCtx.dev = dev;
    EfcRunCtx runCtx = {};
    runCtx.onEfcFireReportCb = defaultOnFireReport;

    efc_run (&efcCtx, &runCtx);

    /* std::thread efc_run_main(efc_run_thread,&efcCtx,(EfcRunCtx*) &runCtx); */
    /* efc_run_main.detach(); */
  }
  //  if (pthread_mutex_lock(&(dev->tcp_socket_open_mutex))) on_error("failed on (pthread_mutex_lock(&(dev->tcp_socket_open_mutex))");
  EKA_LOG("Opening new socket for core %u, sess = %u",core,dev->core[core].tcp_sessions);
  fflush(stderr);

  if (dev->core[core].tcp_sessions >= EKA_MAX_TCP_SESSIONS_PER_CORE) 
    on_error("max count of TCP sessions per core %d is reached",dev->core[core].tcp_sessions);
  uint8_t sess = dev->core[core].tcp_sessions++;
  //  dev->core[core].tcp_sess[sess].send_seq2hw = true;
  int yes = 1;
  dev->core[core].tcp_sess[sess].sock_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
  //  dev->core[core].tcp_sess[dev->core[core].tcp_sessions].sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (dev->core[core].tcp_sess[sess].sock_fd < 0) on_error("error creating socket");

  struct sockaddr_in src = {}; // rebuilding for LWIP compatability
  src.sin_addr.s_addr = dev->core[core].src_ip;
  src.sin_port        = 0;
  src.sin_family      = AF_INET;

  socklen_t namelen = sizeof(struct sockaddr_in);

  if (lwip_bind(dev->core[core].tcp_sess[sess].sock_fd, (struct sockaddr *)&src, sizeof(struct sockaddr_in)) != 0) on_error("%s: error binding socket\n",__func__);
  if (lwip_getsockname(dev->core[core].tcp_sess[sess].sock_fd, (struct sockaddr *)&src, &namelen) != 0) on_error("%s: error on getsockname\n",__func__);
  EKA_LOG("Sock %d is binded to %s:%u",dev->core[core].tcp_sess[sess].sock_fd,inet_ntoa(src.sin_addr),be16toh(src.sin_port));

  if (dev->core[core].src_ip != (uint32_t) src.sin_addr.s_addr) on_error ("src_ip %x != src.sin_addr %x",dev->core[core].src_ip, src.sin_addr.s_addr);
  dev->core[core].tcp_sess[sess].src_port = be16toh(src.sin_port);
  dev->core[core].tcp_sess[sess].src_ip = dev->core[core].src_ip;

  if (lwip_setsockopt(dev->core[core].tcp_sess[sess].sock_fd,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int)) < 0) on_error("Cant set TCP_NODELAY");
  //  if (lwip_setsockopt(dev->core[core].tcp_sess[sess].sock_fd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int)) < 0) on_error("Cant set TCP_NODELAY");

  //  if (lwip_setsockopt(dev->core[core].tcp_sess[sess].sock_fd,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int)) < 0) on_error("Cant set TCP_NODELAY");

  struct linger linger = {
    //    true, // Linger ON
    false, // Linger OFF
    0     // Abort pending traffic on close()
  };

  if (lwip_setsockopt(dev->core[core].tcp_sess[sess].sock_fd,SOL_SOCKET, SO_LINGER,&linger,sizeof(struct linger)) < 0) on_error("Cant set SO_LINGER");
  EKA_LOG("SO_LINGER is set for sock_fd=%d for core %d, sess %d", dev->core[core].tcp_sess[sess].sock_fd,core,sess);

  EKA_LOG("opened sock_fd=%d for core %d, sess %d", dev->core[core].tcp_sess[sess].sock_fd,core,sess);
  fflush(stderr);
  //  pthread_mutex_unlock(&(dev->tcp_socket_open_mutex));
  return dev->core[core].tcp_sess[sess].sock_fd;
}


void eka_close_tcp ( EkaDev* dev) {
  if (dev->lwip_inited) {
    EKA_LOG("Closing LWIP");
    netconn_thread_cleanup();
    for (uint8_t c=0;c<dev->hw.enabled_cores;c++) {
      netif_set_down(dev->core[c].pLwipNetIf);
      netif_remove(dev->core[c].pLwipNetIf);
    }
  } else {
    EKA_LOG("LWIP was not initialized");
  }
  return; 
}

int excUdpSocket (EkaDev* dev) { 
  int sk = lwip_socket(AF_INET, SOCK_DGRAM, 0);
  int const_one = 1;
  if (lwip_setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &const_one, sizeof(int)) < 0) on_error("setsockopt(SO_REUSEADDR) failed");
  return sk;
}

int excSendTo (EkaDev* dev, int sock, const void *dataptr, size_t size, const struct sockaddr *to) {
  struct sockaddr_in dst = {}; // rebuilding for LWIP compatability
  dst.sin_addr.s_addr = ((struct sockaddr_in*)to)->sin_addr.s_addr;
  dst.sin_port        = ((struct sockaddr_in*)to)->sin_port;
  dst.sin_family      = AF_INET;
  
  return lwip_sendto(sock, dataptr, size, 0, (const struct sockaddr *)&dst, sizeof(struct sockaddr));
}

int excSendFastUdp (EkaDev* dev, uint8_t core, uint8_t sess, const void *dataptr, size_t size, const struct sockaddr *to) {
  uint32_t mc_ip = ((struct sockaddr_in*)to)->sin_addr.s_addr;
  uint size2send = sizeof(struct EkaEthHdr) + sizeof(struct EkaIpHdr) + sizeof(struct EkaUdpHdr) + size;

  if (size2send > EKA_MAX_TCP_PAYLOAD_SIZE) on_error("Size to send (%d) > EKA_MAX_TCP_PAYLOAD_SIZE (%d)",(int)size,EKA_MAX_TCP_PAYLOAD_SIZE);
  uint8_t buf[1536] = {};

  uint8_t macda[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x00};
  macda[3] = ((uint8_t*) &mc_ip)[1] & 0x7F;
  macda[4] = ((uint8_t*) &mc_ip)[2];
  macda[5] = ((uint8_t*) &mc_ip)[3];

  struct EkaEthHdr* ether_hdr = (struct EkaEthHdr*)buf;
  memcpy(ether_hdr->dest,macda,6);
  memcpy(ether_hdr->src,dev->core[core].macsa,6);
  ether_hdr->type = be16toh(EKA_ETHTYPE_IP);

  struct EkaIpHdr* ip_hdr = (struct EkaIpHdr*) (buf + sizeof(struct EkaEthHdr));
  ip_hdr->_v_hl = 0x45; 
  ip_hdr->_tos = 0;
  ip_hdr->_len = be16toh(sizeof(struct EkaIpHdr) + sizeof(struct EkaUdpHdr) + size);
  ip_hdr->_id = 0;
  ip_hdr->_offset = 0x0040; // dont fragment
  ip_hdr->_ttl = 64;
  ip_hdr->_proto = IPPROTO_UDP;
  ip_hdr->_chksum = 0;
  ip_hdr->src = dev->core[core].src_ip;
  ip_hdr->dest = mc_ip;

  struct EkaUdpHdr* udp_hdr = (struct EkaUdpHdr*) (buf + sizeof(struct EkaEthHdr) + sizeof(struct EkaIpHdr));
  udp_hdr->src = 0;
  udp_hdr->dest = ((struct sockaddr_in*)to)->sin_port;
  udp_hdr->len = be16toh(sizeof(struct EkaUdpHdr) + size);
  udp_hdr->chksum = 0;

  uint8_t* payload = buf + sizeof(struct EkaEthHdr) + sizeof(struct EkaIpHdr) + sizeof(struct EkaUdpHdr);
  memcpy(payload,dataptr,size);

  uint16_t ip_cs = csum((uint16_t*)EKA_IPH(buf), 20);
  EKA_IPH_CHKSUM_SET(buf, ip_cs);

  /* hexDump("Constructed UDP pkt BEFORE calculated checksum",buf,size2send); */
  net_checksum_calculate(buf,size2send);
  /* hexDump("Constructed UDP pkt AFTER calculated checksum",buf,size2send); */

  ekaLowLevelSend_full_pkt(dev, core, sess, (void*)buf, size2send);
  return size2send;
}

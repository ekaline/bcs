#include "lwip/sockets.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/api.h"


#include "EkaTcpSess.h"
#include "eka_macros.h"
#include "eka_hw_conf.h"
#include "EkaDev.h"
#include "EkaCore.h"
#include "EkaTcp.h"

//#include "EkaHwInternalStructs.h"

class EkaCore;

uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);
uint32_t calcEmptyPktPseudoCsum (EkaIpHdr* ipHdr, EkaTcpHdr* tcpHdr);

unsigned int pseudo_csum(unsigned short *ptr,int nbytes);
uint16_t pseudo_csum2csum (uint32_t pseudo);
unsigned short csum(unsigned short *ptr,int nbytes);

static void hexDump (const char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    if (desc != NULL) printf ("%s:\n", desc);
    if (len == 0) { printf("  ZERO LENGTH\n"); return; }
    if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) printf ("  %s\n", buff);
            printf ("  %04x ", i);
        }
        printf (" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
        else buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) { printf ("   "); i++; }
    printf ("  %s\n", buff);
}

inline bool eka_is_all_zeros (void* buf, ssize_t size) {
  uint8_t* b = (uint8_t*) buf;
  for (int i=0; i<size; i++) if (b[i] != 0) return false;
  return true;
}

/* ---------------------------------------------------------------- */
EkaTcpSess::EkaTcpSess(EkaDev* pEkaDev, EkaCore* _parent, uint8_t _coreId, uint8_t _sessId, 
		       uint32_t _srcIp, uint32_t _dstIp,uint16_t _dstPort, 
		       uint8_t* _macSa, uint8_t* _macDa, const uint MAX_SESS_PER_CORE) {
  dev     = pEkaDev;
  coreId  = _coreId;
  sessId  = _sessId;
  parent  = _parent;

  srcIp   = _srcIp;
  srcPort =  0;

  dstIp   = _dstIp;
  dstPort = _dstPort;

  tcpLocalSeqNum     = 0;
  tcpRemoteSeqNum    = 0;

  fastPathBytes      = 0;
  txDriverBytes      = 0;
  dummyBytes         = 0;

  fastBytesFromUserChannel = 0;

  connectionEstablished = false;

  memcpy(macSa,_macSa,6);
  memcpy(macDa,_macDa,6);

  if ((sock = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    on_error ("error creating TCP socket");

  if (sessId == MAX_SESS_PER_CORE) {
    EKA_LOG("Established TCP Session %u for Control Traffic",sessId);
  } else {
    EKA_LOG("sock=%d for: %s:%u --> %s:%u, %s -> %s",sock,
	    EKA_IP2STR(srcIp),srcPort,
	    EKA_IP2STR(dstIp),dstPort,
	    EKA_MAC2STR(macSa),EKA_MAC2STR(macDa));
  }
  payloadFastSendSlot = (MAX_SESS_PER_CORE + 1) * coreId + sessId;

  payloadFastSendDescr.tcpd.src_index      = MAX_PKT_SIZE * payloadFastSendSlot;
  payloadFastSendDescr.tcpd.length         = 0;
  payloadFastSendDescr.tcpd.session        = sessId;
  payloadFastSendDescr.tcpd.ip_checksum    = 0;
  payloadFastSendDescr.tcpd.core_send_attr = coreId; // modify TCP sequences and checksum

  fullPktFastSendSlot = (MAX_SESS_PER_CORE + 1)* coreId + MAX_SESS_PER_CORE + 1 + sessId;

  fullPktFastSendDescr.tcpd.src_index      = MAX_PKT_SIZE * fullPktFastSendSlot;
  fullPktFastSendDescr.tcpd.length         = 0;
  fullPktFastSendDescr.tcpd.session        = sessId;
  fullPktFastSendDescr.tcpd.ip_checksum    = 0;
  fullPktFastSendDescr.tcpd.core_send_attr = 0x40 | coreId; // dont modify TCP sequences and checksum



}

/* ---------------------------------------------------------------- */

ssize_t EkaTcpSess::recv(void *buffer, size_t size) {
  //  dev->mtx.lock();
  ssize_t res = lwip_recv(sock, buffer, size, 0);
  //  dev->mtx.unlock();
  return res;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::bind() {
  int flag = 1;
  if (lwip_setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char *) &flag, sizeof(int)) < 0) 
    on_error("Cant set TCP_NODELAY");
    
  struct linger linger = {
    false, // Linger OFF
    0      // Abort pending traffic on close()
  };

  if (lwip_setsockopt(sock,SOL_SOCKET, SO_LINGER,&linger,sizeof(struct linger)) < 0) 
    on_error("Cant set TCP_NODELAY");    

  struct sockaddr_in src = {};
  src.sin_addr.s_addr = srcIp;
  src.sin_port        = 0; // dummy
  src.sin_family      = AF_INET;

  socklen_t addr_len = sizeof(struct sockaddr_in);

  if (lwip_bind(sock, (struct sockaddr *)&src, sizeof(struct sockaddr_in)) < 0)
    on_error("error binding socket");

  if (lwip_getsockname(sock, (struct sockaddr *)&src, &addr_len) < 0) 
    on_error("error on getsockname");

  srcPort = be16toh(src.sin_port);
  EKA_LOG("Sock %d is binded to %s:%u",sock,EKA_IP2STR(src.sin_addr.s_addr),be16toh(src.sin_port));

  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::connect() {

  struct sockaddr_in dst = {};
  dst.sin_addr.s_addr = dstIp;
  dst.sin_port        = be16toh(dstPort);
  dst.sin_family      = AF_INET;

  struct netif* pLwipNetIf = (struct netif*) parent->pLwipNetIf;

  EKA_LOG("Establishing TCP for %u:%u : %s:%u --> %s:%u",
	  coreId,sessId,
	  EKA_IP2STR(srcIp),srcPort,EKA_IP2STR(dstIp),dstPort);


  if (! eka_is_all_zeros(macDa,6)) {
    EKA_LOG("Adding Static ARP entry %s --> %s for NIC %d",
	    EKA_IP2STR(dst.sin_addr.s_addr),
	    EKA_MAC2STR(macDa),
	    ((LwipNetifState*)(pLwipNetIf)->state)->lane);
    /* err_t err = etharp_add_static_entry((const ip4_addr_t*) &dst.sin_addr, (struct eth_addr *)macDa); */
    /* if (err == ERR_RTE) on_error("etharp_add_static_entry returned ERR_RTE"); */
#define ETHARP_FLAG_TRY_HARD     1
#define ETHARP_FLAG_STATIC_ENTRY 4
    etharp_update_arp_entry(pLwipNetIf, (const ip4_addr_t*) &dst.sin_addr, (struct eth_addr *)macDa, (u8_t)(ETHARP_FLAG_TRY_HARD | ETHARP_FLAG_STATIC_ENTRY));

  } else {
    EKA_LOG("Sending ARP request to %s",EKA_IP2STR(dst.sin_addr.s_addr));
    if (etharp_request(pLwipNetIf, (const ip4_addr_t*)&dst.sin_addr) != ERR_OK) 
      on_error("etharp_query failed: no route to %s",EKA_IP2STR(dst.sin_addr.s_addr));
  }

  uint32_t* ipPtr = NULL;
  bool arpFound = false;
  uint8_t* macDa_ptr = NULL;
  for (int trials = 0; trials < 10000; trials ++) {
    int arpEntry = etharp_find_addr(pLwipNetIf,
				    (const ip4_addr_t*)&dst.sin_addr,
				    (eth_addr**)&macDa_ptr,
				    (const ip4_addr_t**)&ipPtr);
    if (arpEntry >= 0) {
      arpFound = true;
      if (macDa_ptr == NULL) on_error("macDa_ptr == NULL");
      memcpy(macDa,macDa_ptr,6);
      EKA_LOG("Found Arp Entry %d with %s %s",arpEntry,EKA_IP2STR(*ipPtr),EKA_MAC2STR(macDa));
      break;
    }
    if (! dev->servThreadActive) break;
    usleep(1);
  }
  if (! arpFound) on_error("%s is not in the ARP table",EKA_IP2STR(dst.sin_addr.s_addr));
  
  /* EKA_LOG("ARP TABLE: %s --> %s", */
  /* 	  EKA_IP2STR(*ipPtr),EKA_MAC2STR(macDa)); */

  //  for (uint i=0; i<6;i++) parent->macda[i] = m[i]; // taking MAC_DA for FPGA fires

  if (lwip_connect(sock,(const sockaddr*) &dst, sizeof(struct sockaddr_in)) < 0) 
    on_error("socket connect failed");
  EKA_LOG("TCP connected %u:%u : %s:%u --> %s:%u",
	  coreId,sessId,EKA_IP2STR(srcIp),srcPort,EKA_IP2STR(dstIp),dstPort);


  connectionEstablished = true;

  return 0;
}
/* ---------------------------------------------------------------- */

EkaTcpSess::~EkaTcpSess() {
  EKA_LOG("Closing socket %d for core%u sess%u",sock,coreId,sessId);
  lwip_close(sock);
}
/* ---------------------------------------------------------------- */

int EkaTcpSess::preloadNwHeaders() {
  memset(&pktBuf,0,sizeof(pktBuf));
  EkaEthHdr* ethHdr = (EkaEthHdr*) pktBuf;
  EkaIpHdr*  ipHdr  = (EkaIpHdr* ) (((uint8_t*)ethHdr) + sizeof(EkaEthHdr));
  EkaTcpHdr* tcpHdr = (EkaTcpHdr*) (((uint8_t*)ipHdr ) + sizeof(EkaIpHdr));

  memcpy(&(ethHdr->dest),&macDa,6);
  memcpy(&(ethHdr->src), &macSa,6);
  ethHdr->type = be16toh(0x0800);

  ipHdr->_v_hl = 0x45;
  ipHdr->_offset = 0x0040;
  ipHdr->_ttl = 64;
  ipHdr->_proto = IPPROTO_TCP;
  ipHdr->_len =  be16toh(dev->hwRawFireSize+sizeof(EkaIpHdr)+sizeof(EkaTcpHdr));

  ipHdr->src  = srcIp;
  ipHdr->dest = dstIp;

  tcpHdr->src    = be16toh(srcPort);
  tcpHdr->dest   = be16toh(dstPort);
  tcpHdr->_hdrlen_rsvd_flags = be16toh(0x5000 | TCP_PSH | TCP_ACK);
  tcpHdr->urgp = 0;

/* ---------------------------------------------------------------- Fire Headers*/
  hw_session_nw_header_t hw_nw_header = {
    .ip_cs    = be16toh(csum((unsigned short *)ipHdr, sizeof(EkaIpHdr))),
    .dst_ip   = be32toh(dstIp),
    .src_port = srcPort,
    .dst_port = dstPort,
    .tcpcs    = 0
  };


  for (int i=0;i<((int)sizeof(hw_session_nw_header_t)/8) + !!(sizeof(hw_session_nw_header_t)%8);i++) {
    uint64_t data = ((uint64_t*)&hw_nw_header)[i];
    uint64_t addr = HW_SESSION_NETWORK_BASE+HW_SESSION_NETWORK_CORE_OFFSET*coreId+i*8;
    eka_write (dev,addr,data);
  }
  union large_table_desc hw_nw_desc = {};
  hw_nw_desc.ltd.src_bank = 0;
  hw_nw_desc.ltd.src_thread = 0;
  hw_nw_desc.ltd.target_idx = sessId;
  eka_write (dev,HW_SESSION_NETWORK_DESC_BASE + HW_SESSION_NETWORK_DESC_OFFSET*coreId,hw_nw_desc.lt_desc); //desc
/* ---------------------------------------------------------------- */

  uint64_t write_value;
  // macda
  memcpy ((char*)&write_value+2, macDa, 6);
  write_value = be64toh(write_value);
  eka_write (dev,CORE_CONFIG_BASE+CORE_CONFIG_DELTA*coreId+CORE_MACDA_OFFSET,write_value);

  EKA_LOG("Writing MacDA for core %u: %s",coreId,EKA_MAC2STR(macDa));

  // src ip
  eka_write (dev,CORE_CONFIG_BASE+CORE_CONFIG_DELTA*coreId+CORE_SRC_IP_OFFSET,be32toh(srcIp));
    
  // macsa
  memcpy ((char*)&write_value+2, macSa, 6);
  write_value = be64toh(write_value);
  eka_write (dev,CORE_CONFIG_BASE+CORE_CONFIG_DELTA*coreId+CORE_MACSA_OFFSET,write_value);
  
/* ---------------------------------------------------------------- FastPath Headers*/
  ipHdr->_len = 0; // for Fast Path. Recalculated every transaction
  ipPreliminaryPseudoCsum = pseudo_csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));
  tcpPreliminaryPseudoCsum = calcEmptyPktPseudoCsum(ipHdr, tcpHdr);


#ifdef EKA_WC
#else
  uint64_t base_addr = TCP_FAST_SEND_SP_BASE + MAX_PKT_SIZE * payloadFastSendSlot;

  uint64_t *buff_ptr = (uint64_t *)pktBuf;
  for (uint i = 0; i < 6; i++) eka_write(dev, base_addr + i * 8, buff_ptr[i]);
  /* hexDump("Preloaded TCP TX Pkt Hdr",(void*)ethHdr,48); */
#endif  


  EKA_LOG("%s:%u -- %s:%u is preloaded for core=%u, sess=%u",
	  EKA_IP2STR(*(uint32_t*)(&ipHdr->src)),be16toh(tcpHdr->src),
	  EKA_IP2STR(*(uint32_t*)(&ipHdr->dest)),be16toh(tcpHdr->dest),
	  coreId,sessId
	  );

  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::updateRx(const uint8_t* pkt, uint32_t len) {
  // THIS FUNCTION IS NOT USED
  if (tcpRemoteSeqNum != EKA_TCPH_SEQNO(pkt) + EKA_TCP_PAYLOAD_LEN(pkt)) {
    tcpRemoteSeqNum = EKA_TCPH_SEQNO(pkt) + EKA_TCP_PAYLOAD_LEN(pkt);

    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;
    eka_write(dev,0x60000 + 0x1000*coreId + 8 * (sessId*2),tcpRemoteSeqNum);
    eka_write(dev,0x6f000 + 0x100*coreId,desc.desc); 
  }

  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::sendStackPkt(void *pkt, int len) {
  if (EKA_TCP_SYN(pkt)) {
    tcpLocalSeqNum = EKA_TCPH_SEQNO(pkt) + 1;

    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;
    eka_write(dev,0x30000 + 0x1000*coreId + 8 * (sessId*2),tcpLocalSeqNum);
    eka_write(dev,0x3f000 + 0x100*coreId,desc.desc);

    tcpWindow = EKA_TCPH_WND(pkt);
    eka_write(dev,0xe0318,tcpWindow);
    sendFullPkt(pkt,len);
    return 0;
  } 
  /* -------------------------------------- */
  if (EKA_TCP_FIN(pkt)) {
    sendFullPkt(pkt,len);
    return 0;    
  }
  /* -------------------------------------- */

  if (tcpRemoteSeqNum != EKA_TCPH_ACKNO(pkt)) {
    tcpRemoteSeqNum = EKA_TCPH_ACKNO(pkt);

    tcpWindow = EKA_TCPH_WND(pkt);
    uint64_t tcpWindow_64bit = (uint64_t)tcpWindow;

    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;
    //    eka_write(dev,0x60000 + 0x1000*coreId + 8 * (sessId*2),tcpRemoteSeqNum | (tcpWindow << 32) );
    eka_write(dev,0x60000 + 0x1000*coreId + 8 * (sessId*2),tcpRemoteSeqNum | (tcpWindow_64bit << 32) );
    eka_write(dev,0x6f000 + 0x100*coreId,desc.desc); 

    if (connectionEstablished) {
      //EKA_LOG("\n=====================\n%u: Sending empty SEQ %ju, ACK %ju from HW   \n=====================\n",sessId,tcpLocalSeqNum,tcpRemoteSeqNum);
    } else {
      //EKA_LOG("\n=====================\n%u: Sending empty SEQ %ju, ACK %ju from Stack\n=====================\n",sessId,tcpLocalSeqNum,tcpRemoteSeqNum);
      sendFullPkt(pkt,len);
      return 0;
    }
  } else {
    //    EKA_LOG("Not sending HW ACK: ACKNO=%u, pktLen = %u",EKA_TCPH_ACKNO(pkt),EKA_TCP_PAYLOAD_LEN(pkt));
  }

  /* -------------------------------------- */
  if (EKA_TCPH_SEQNO(pkt) < tcpLocalSeqNum) { // retransmit
    EKA_LOG("Retransmit: Total Len = %u bytes, Seq = %u, tcpLocalSeqNum=%u", len, EKA_TCPH_SEQNO(pkt),tcpLocalSeqNum);
    hexDump("RetransmitPkt",pkt,len);

    sendFullPkt(pkt,len);
    return 0;
  }
  /* -------------------------------------- */
  // DUMMY PACKET -- DROPPED
  tcpLocalSeqNum = EKA_TCPH_SEQNO(pkt) + EKA_TCP_PAYLOAD_LEN(pkt);
  txDriverBytes += EKA_TCP_PAYLOAD_LEN(pkt);


  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::sendFullPkt(void *buf, int len) {
  //  hexDump("sendFullPkt",buf,len);
  if (! dev->exc_active) return 1;

  if ((uint)len > MAX_PKT_SIZE) 
    on_error("Size of buffer to send (=%d) exceeds MAX_PKT_SIZE (%d)",
	     (int)len,MAX_PKT_SIZE);

  fullPktFastSendDescr.tcpd.length = static_cast<decltype(fullPktFastSendDescr.tcpd.length)>(len);

  //#########################################################
  //#ifdef EKA_WC
#if 0
  uint8_t __attribute__ ((aligned(0x100)))  pktBuf[256] = {};
  memcpy(pktBuf,buf,len);
  volatile uint8_t* base_addr = dev->txPktBuf + fullPktFastSendDescr.tcpd.src_index;

  //---------------------------------------------------------
    int iter = len/64 + !!(len%64); // num of 64 Byte words of the packet 
    for (int i = 0; i < iter; i++) {
      //      memcpy((uint8_t*)(base_addr + i * 64), ((uint8_t*)buf) + i * 64, 64);
      memcpy((uint8_t*)(base_addr + i * 64), ((uint8_t*)pktBuf) + i * 64, 64);
      _mm_sfence();
    }
  //---------------------------------------------------------
#else 
  uint64_t base_addr = TCP_FAST_SEND_SP_BASE + fullPktFastSendDescr.tcpd.src_index;
  int iter = len/8 + !!(len%8); // num of 8Byte words of the packet
  uint64_t *buff_ptr = (uint64_t *)buf;
  for (int z=0; z<iter; ++z) {
    eka_write(dev, base_addr + z*8, buff_ptr[z]);
  }
#endif
  //---------------------------------------------------------

  eka_write(dev, TCP_FAST_SEND_DESC, fullPktFastSendDescr.desc);

  //  return ERR_OK;
  return 0;
}
/* ---------------------------------------------------------------- */

int EkaTcpSess::sendThruStack(void *buf, int len) {
  //    dev->mtx.lock();
    int res = lwip_write(sock,buf,len);
    //    dev->mtx.unlock();

    return res;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::sendDummyPkt(void *buf, int len) {
  fastBytesFromUserChannel += len;
  //  EKA_LOG("\n+++++++++++++++++++\nfastBytesFromUserChannel = %ju\n+++++++++++++++++++",fastBytesFromUserChannel);

  //  dev->mtx.lock();
  uint8_t* sendPtr = (uint8_t*)buf;
  int bytes2send = len;
  while (dev->exc_active && (bytes2send > 0)) {
    int sentBytes = lwip_write(sock,sendPtr,bytes2send);

    if (sentBytes <= 0) {
      //      on_error("Im dead!");
      EKA_LOG("Tried %d, Sent %d",bytes2send,sentBytes);
      //      usleep(10);
    } else {   
      sendPtr    += sentBytes;
      bytes2send -= sentBytes;
    }
  }
  dummyBytes += len;
  //  EKA_LOG("\n+++++++++++++++++++\ndummyBytes = %ju (%p)\n+++++++++++++++++++",dummyBytes,&dummyBytes);
  //  dev->mtx.unlock();
  return len;
}

/* ---------------------------------------------------------------- */
int EkaTcpSess::sendPayload(void *buf, int len) {
  if (! dev->exc_active) return 1;

  uint16_t tcpWindowSampled = tcpWindow;

  /* if (fastPathBytes < txDriverBytes) */
  /*   on_error("fastPathBytes %ju < txDriverBytes %ju",fastPathBytes,txDriverBytes); */

  if (fastPathBytes > txDriverBytes) {
    /* EKA_LOG("TX Backpressure: fastPathBytes=%ju, txDriverBytes=%ju", */
    /* 	    fastPathBytes,txDriverBytes); */
    //    printf(".");
    return 0; // too high tx rate -- Back Pressure
  }

  uint size2send = ((uint)len <= (MAX_PKT_SIZE + 2)) ? (uint)len : MAX_PKT_SIZE;
  fastPathBytes += size2send;
  if (size2send == 1) EKA_LOG("WARNING: 1Byte payload might be considered as Keep Alive");

  if (size2send <= 2) on_error("len = %d, size2send=%u,MAX_PKT_SIZE=%u",len,size2send,MAX_PKT_SIZE);

  uint16_t ip_tot_len = sizeof(EkaIpHdr) + sizeof(EkaTcpHdr) + size2send;
  uint16_t ip_csum    = pseudo_csum2csum(ipPreliminaryPseudoCsum + be16toh(ip_tot_len));

  uint32_t tcp_pseudo_csum = 
    tcpPreliminaryPseudoCsum               + 
    be16toh(tcpWindowSampled)                    -
    be16toh(sizeof(EkaTcpHdr))             +
    be16toh(sizeof(EkaTcpHdr) + size2send) +
    pseudo_csum((uint16_t*)buf,size2send);

  payloadFastSendDescr.tcpd.length      = ip_tot_len + sizeof(EkaEthHdr);
  payloadFastSendDescr.tcpd.ip_checksum = be16toh(ip_csum);

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
  w0.wl.window   = be16toh(tcpWindowSampled);
  w0.wl.tcp_csum = be16toh((uint16_t) (tcp_pseudo_csum>>16) & 0xFFFF);
  w0.wl.tcp_urgp = be16toh((uint16_t) (tcp_pseudo_csum      & 0xFFFF));
  w0.wl.data     = * (uint16_t*) buf;

#ifdef EKA_WC
  volatile uint8_t* base_addr = dev->txPktBuf + payloadFastSendDescr.tcpd.src_index; 

  uint64_t* pPktBuf = (uint64_t*)pktBuf;
  pPktBuf[6] = w0.w_0;

  int words8B = 1;
  if (size2send > 2) {
    words8B += (size2send - 2)/8 + !!((size2send - 2) % 8); // num of 8Byte words of the packet
    uint64_t *buff_ptr = (uint64_t *)((uint8_t*)buf + 2);
    for (int w8 = 0; w8 < words8B - 1; w8 ++) pPktBuf[7 + w8] = buff_ptr[w8];
  }

  uint totalPktLen = 14 + 20 + 20 + size2send;
  uint words64 = totalPktLen / 64 + !! (totalPktLen % 64);
  if (words64 > 4) on_error("words64 = %u",words64);

  //  memcpy((uint8_t*)base_addr,pPktBuf,words64 * 64);
  for (uint w64 = 0; w64 < words64; w64++) {
     memcpy((uint8_t*)base_addr,pPktBuf,64);
     base_addr += 64;
     pPktBuf += 8;
     _mm_mfence();
  }
  //  EKA_LOG("len = %u, totalPktLen = %u, words64 = %u, copied %u Bytes",size2send, totalPktLen, words64, words64 * 64);
#else
  uint64_t base_addr = TCP_FAST_SEND_SP_BASE + payloadFastSendDescr.tcpd.src_index;
  eka_write(dev, base_addr + 6*8, w0.w_0);
  
  int mainPayloadSize = size2send-2;
  if (mainPayloadSize > 0) {
    int iter = mainPayloadSize/8 + !!(mainPayloadSize%8); // num of 8Byte words of the packet
    uint64_t *buff_ptr = (uint64_t *)((uint8_t*)buf + 2);
    for (int z=0; z<iter; ++z) eka_write(dev, base_addr + (z+7)*8, buff_ptr[z]);
  }

#endif
  eka_write(dev, TCP_FAST_SEND_DESC, payloadFastSendDescr.desc);

  /* if (sessId == 0) { */
  /*   uint8_t pktBuf2check[2000] = {}; */

  /*   uint8_t* ipHdr   = pktBuf2check + sizeof(EkaEthHdr); */
  /*   uint8_t* tcpHdr  = ipHdr        + sizeof(EkaIpHdr); */
  /*   uint8_t* payload = tcpHdr       + sizeof(EkaTcpHdr); */

  /*   memcpy(pktBuf2check, pktBuf, 54); // Eth + IP + Tcp Hdr */
    
  /*   ((EkaTcpHdr*)tcpHdr)->seqno = be32toh(tcpLocalSeqNum); */
  /*   ((EkaTcpHdr*)tcpHdr)->ackno = be32toh(tcpRemoteSeqNum); */
  /*   ((EkaTcpHdr*)tcpHdr)->wnd   = be16toh(tcpWindowSampled); */
  /*   ((EkaTcpHdr*)tcpHdr)->urgp  = 0; */

  /*   memcpy(payload ,buf,size2send); // Payload */

  /*   uint32_t tcpCsum = calc_pseudo_csum((void*)ipHdr,(void*)tcpHdr,payload,size2send); */
    
  /*   ((EkaTcpHdr*)tcpHdr)->chksum = pseudo_csum2csum(tcpCsum); */

  /*   EKA_LOG("\n@@@@@@@@@@\nSess 0: tcpCsum = %04x\n@@@@@@@@@@\n",pseudo_csum2csum(tcpCsum)); */
  /*   hexDump("Sess 0 pktBuf2check",pktBuf2check,54 + size2send); */
  /* } */

  return size2send;
}

int EkaTcpSess::close() {
  return lwip_close(sock);
}

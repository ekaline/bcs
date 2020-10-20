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
#include "EkaEpm.h"
#include "EpmFastPathTemplate.h"
#include "EpmRawPktTemplate.h"
#include "EkaEpmAction.h"

class EkaCore;

uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);
uint32_t calcEmptyPktPseudoCsum (EkaIpHdr* ipHdr, EkaTcpHdr* tcpHdr);
uint16_t calc_tcp_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);

unsigned int pseudo_csum(unsigned short *ptr,int nbytes);
uint16_t pseudo_csum2csum (uint32_t pseudo);
unsigned short csum(unsigned short *ptr,int nbytes);
void hexDump (const char *desc, void *addr, int len);

inline bool eka_is_all_zeros (void* buf, ssize_t size) {
  uint8_t* b = (uint8_t*) buf;
  for (int i=0; i<size; i++) if (b[i] != 0) return false;
  return true;
}

/* ---------------------------------------------------------------- */
EkaTcpSess::EkaTcpSess(EkaDev* pEkaDev, EkaCore* _parent, uint8_t _coreId, uint8_t _sessId, 
		       uint32_t _srcIp, uint32_t _dstIp,uint16_t _dstPort, 
		       uint8_t* _macSa, uint8_t* _macDa) {
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

  if (sessId == CONTROL_SESS_ID) {
    EKA_LOG("Established TCP Session %u for Control Traffic, coreId=%u",sessId,coreId);
  } else {
    EKA_LOG("sock=%d for: %s:%u --> %s:%u, %s -> %s",sock,
	    EKA_IP2STR(srcIp),srcPort,
	    EKA_IP2STR(dstIp),dstPort,
	    EKA_MAC2STR(macSa),EKA_MAC2STR(macDa));
  }

/* -------------------------------------------- */
  fastPathAction = dev->epm->addAction(EkaEpm::ActionType::TcpFastPath,coreId,sessId,0);
  fullPktAction  = dev->epm->addAction(EkaEpm::ActionType::TcpFullPkt, coreId,sessId,0);
  emptyAckAction = dev->epm->addAction(EkaEpm::ActionType::TcpEptyAck, coreId,sessId,0);
}

/* ---------------------------------------------------------------- */

ssize_t EkaTcpSess::recv(void *buffer, size_t size) {
  ssize_t res = lwip_recv(sock, buffer, size, 0);
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
  dst.sin_family      = AF_INET;\

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
    LOCK_TCPIP_CORE();
    etharp_update_arp_entry(pLwipNetIf, (const ip4_addr_t*) &dst.sin_addr, (struct eth_addr *)macDa, (u8_t)(ETHARP_FLAG_TRY_HARD | ETHARP_FLAG_STATIC_ENTRY));
    UNLOCK_TCPIP_CORE();

  } else {
    EKA_LOG("Sending ARP request to %s from core %u",EKA_IP2STR(dst.sin_addr.s_addr),coreId);
    LOCK_TCPIP_CORE();
    if (etharp_request(pLwipNetIf, (const ip4_addr_t*)&dst.sin_addr) != ERR_OK) 
      on_error("etharp_query failed: no route to %s",EKA_IP2STR(dst.sin_addr.s_addr));
    UNLOCK_TCPIP_CORE();

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

  dev->lwipConnectMtx.lock();
  if (lwip_connect(sock,(const sockaddr*) &dst, sizeof(struct sockaddr_in)) < 0) 
    on_error("socket connect failed");
  dev->lwipConnectMtx.unlock();

  EKA_LOG("TCP connected %u:%u : %s:%u --> %s:%u",
	  coreId,sessId,EKA_IP2STR(srcIp),srcPort,EKA_IP2STR(dstIp),dstPort);


  connectionEstablished = true;


  uint32_t sockOpt = lwip_fcntl(sock, F_GETFL, 0);
  sockOpt |= O_NONBLOCK;
  int ret = lwip_fcntl(sock, F_SETFL, sockOpt);
  if (ret < 0) on_error("setting O_NONBLOCK is failed for sock %d: ret = %d",sock,ret);
  EKA_LOG("O_NONBLOCK is set for socket %d",sock);
  UNLOCK_TCPIP_CORE();

  return 0;
}
/* ---------------------------------------------------------------- */

EkaTcpSess::~EkaTcpSess() {
  EKA_LOG("Closing socket %d for core%u sess%u",sock,coreId,sessId);
  lwip_shutdown(sock,SHUT_RDWR);
  lwip_close(sock);
  EKA_LOG("Closed socket %d for core%u sess%u",sock,coreId,sessId);
}
/* ---------------------------------------------------------------- */

int EkaTcpSess::preloadNwHeaders() {
  /* memset(&pktBuf,0,sizeof(pktBuf)); */

  /* memcpy(&(ethHdr->dest),&macDa,6); */
  /* memcpy(&(ethHdr->src), &macSa,6); */
  /* ethHdr->type = be16toh(0x0800); */

  /* ipHdr->_v_hl = 0x45; */
  /* ipHdr->_offset = 0x0040; */
  /* ipHdr->_ttl = 64; */
  /* ipHdr->_proto = IPPROTO_TCP; */
  /* ipHdr->_chksum = 0; */
  /* ipHdr->_id  = EkaIpId; */
  /* ipHdr->_len = 0; */

  /* ipHdr->src  = srcIp; */
  /* ipHdr->dest = dstIp; */

  /* tcpHdr->src    = be16toh(srcPort); */
  /* tcpHdr->dest   = be16toh(dstPort); */
  /* tcpHdr->_hdrlen_rsvd_flags = be16toh(0x5000 | TCP_PSH | TCP_ACK); */
  /* tcpHdr->urgp = 0; */

  //---------------------------------------------------------
  // Preloading EmptyAck 
  /* ipHdr->_len = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)); */
  /* ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr)); */
  /* uint emptyAckLen = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr); */
  /* copyIndirectBuf2HeapHw_swap4(dev,emptyAckAction->heapAddr,(uint64_t*) pktBuf, MAX_CTX_THREADS - 1 /\* threadId *\/, emptyAckLen); */

  /* emptyAcktrigger.str.action_index = emptyAckAction->idx; */
  /* emptyAcktrigger.str.payload_size = emptyAckLen; */
  /* emptyAcktrigger.str.tcp_cs       = calc_pseudo_csum(ipHdr,tcpHdr,tcpPayload,0); */

  /* ipHdr->_len = 0; */
  /* ipHdr->_chksum = 0; */

  //---------------------------------------------------------

  //  EKA_LOG(" emptyAcktrigger.str.action_index = %u, emptyAcktrigger.str.payload_size=%u, emptyAckAction->heapAddr = 0x%jx",
  //  	  emptyAcktrigger.str.action_index, emptyAcktrigger.str.payload_size,emptyAckAction->heapAddr);

  /* ipHdr->_len = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)); */


  emptyAckAction->setNwHdrs(macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  fastPathAction->setNwHdrs(macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  fullPktAction->setNwHdrs (macDa,macSa,srcIp,dstIp,srcPort,dstPort);

  hw_session_nw_header_t hw_nw_header = {
    .ip_cs    = 0,//be16toh(csum((unsigned short *)ipHdr, sizeof(EkaIpHdr))),
    .dst_ip   = be32toh(dstIp),
    .src_port = srcPort,
    .dst_port = dstPort,
    .tcpcs    = 0
  };
  /* ipHdr->_len = 0; */

  copyBuf2Hw(dev,
	     HW_SESSION_NETWORK_BASE+HW_SESSION_NETWORK_CORE_OFFSET * coreId,
	     (uint64_t*)&hw_nw_header,sizeof(hw_nw_header));
  atomicIndirectBufWrite(dev,
			 HW_SESSION_NETWORK_DESC_BASE + HW_SESSION_NETWORK_DESC_OFFSET * coreId,
			 0,0,sessId,0);

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
  /* ipHdr->_len = 0; // for Fast Path. Recalculated every transaction */
  /* ipPreliminaryPseudoCsum = pseudo_csum((unsigned short *)ipHdr, sizeof(EkaIpHdr)); */
  /* tcpPreliminaryPseudoCsum = calcEmptyPktPseudoCsum(ipHdr, tcpHdr); */

  /* hexDump("Preloaded TCP TX Pkt Hdr",(void*)ethHdr,48); */

  copyIndirectBuf2HeapHw_swap4(dev, fastPathAction->heapAddr,(uint64_t *)pktBuf,0 /* threadId */, 48);

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
int EkaTcpSess::setRemoteSeqWnd2FPGA() {
    // Update FPGA with tcpRemoteSeqNum and tcpWindow
    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;

    uint64_t tcpWindow_64bit = (uint64_t)tcpWindow;
    eka_write(dev,0x60000 + 0x1000*coreId + 8 * (sessId*2),tcpRemoteSeqNum | (tcpWindow_64bit << 32) );
    eka_write(dev,0x6f000 + 0x100*coreId,desc.desc);
    // Update FPGA with tcpRemoteSeqNum and tcpWindow
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
  if (! connectionEstablished) {
    // 1st ACK on SYN-ACK
    sendFullPkt(pkt,len);
    return 0;
  }
  /* -------------------------------------- */
  if (EKA_TCPH_SEQNO(pkt) < tcpLocalSeqNum) { 
    // Retransmit
    //    EKA_LOG("Retransmit: Total Len = %u bytes, Seq = %u, tcpLocalSeqNum=%u", len, EKA_TCPH_SEQNO(pkt),tcpLocalSeqNum);
    //    hexDump("RetransmitPkt",pkt,len);
    sendFullPkt(pkt,len);
    return 0;
  }
  /* -------------------------------------- */
  // DUMMY PACKET -- DROPPED
  if (tcpRemoteSeqNum != EKA_TCPH_ACKNO(pkt)) {
    // This is first ACK on the tcpRemoteSeqNum
    // DUMMY PKT has "important" ACK (which was not sent)
    tcpRemoteSeqNum = EKA_TCPH_ACKNO(pkt);
    tcpWindow = EKA_TCPH_WND(pkt);
    setRemoteSeqWnd2FPGA();
    // Empty ACK is sent from FPGA
    emptyAckAction->send(); //
  }
  tcpLocalSeqNum = EKA_TCPH_SEQNO(pkt) + EKA_TCP_PAYLOAD_LEN(pkt);
  txDriverBytes += EKA_TCP_PAYLOAD_LEN(pkt);

  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::sendFullPkt(void *buf, int len) {
  if (! dev->exc_active) return 1;

  if ((uint)len > MAX_PKT_SIZE) 
    on_error("Size (=%d) > MAX_PKT_SIZE (%d)",(int)len,MAX_PKT_SIZE);

  /* hexDump("sendFullPkt",buf,len); */
  /* fullPktAction->print(); */

  fullPktAction->setFullPkt(MAX_CTX_THREADS - 1 /* threadId */,buf,(uint)len);
  fullPktAction->send();

  return 0;
}
/* ---------------------------------------------------------------- */

int EkaTcpSess::sendThruStack(void *buf, int len) {
  int res = lwip_write(sock,buf,len);
  return res;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::readyToSend() {
  struct pollfd fds[1] = {{sock, POLLOUT, 0}};

  int rc = lwip_poll(fds, 1, 0);

  if (rc < 0) on_error("Core %u TcpSess %u poll returned rc=%d",coreId,sessId,rc);

  if (fds[0].revents & POLLERR)
    on_error("Core %u TcpSess %u has POLLERR (broken pipe)",coreId,sessId);
  if (fds[0].revents & POLLNVAL)
    on_error("Core %u TcpSess %u has POLLNVAL (socket is not opened)",coreId,sessId);

  if (fds[0].revents & POLLOUT) return 1;

  //  EKA_WARN("core %u, sess %u, sock %d is not ready to write",coreId,sessId,sock);
  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::sendDummyPkt(void *buf, int len) {

  fastBytesFromUserChannel += len;
  uint8_t* sendPtr = (uint8_t*)buf;
  int bytes2send = len;
  while (dev->exc_active && (bytes2send > 0)) {
    txLwipBp = ! readyToSend();
    if (txLwipBp) {
      usleep(0);
      continue;
    }
    int sentBytes = lwip_write(sock,sendPtr,bytes2send);

    if (sentBytes == 0) {
      usleep(0);
    } else if (sentBytes < 0) {
      //      printf("-");
      perror("XYEBO MHE");
      usleep(0);

    } else {   
      sendPtr    += sentBytes;
      bytes2send -= sentBytes;
    }
  }
  dummyBytes += len;
  return len;
}

/* ---------------------------------------------------------------- */
int EkaTcpSess::sendPayload(uint thrId, void *buf, int len) {
  if (! dev->exc_active) return 1;

  if (txLwipBp                        || // lwip socket is unavauilable
      (fastPathBytes > txDriverBytes) || // previous TX pkt didn't arrive TX driver
      (fastPathBytes > dummyBytes)       // previous TX pkt wasn't sent to lwip as Dummy
      ) {
    usleep(0);
    return 0; // too high tx rate -- Back Pressure
  }

  uint payloadSize2send = ((uint)len <= (MAX_PAYLOAD_SIZE + 2)) ? (uint)len : MAX_PAYLOAD_SIZE;
  if (payloadSize2send <= 2) on_error("len = %d, payloadSize2send=%u,MAX_PKT_SIZE=%u",len,payloadSize2send,MAX_PAYLOAD_SIZE);
  fastPathBytes += payloadSize2send;

  fastPathAction->setPktPayload(thrId, buf, payloadSize2send);
  fastPathAction->send();

  /* uint totalSize2send = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr) + payloadSize2send; */

  /* memcpy(tcpPayload,buf,payloadSize2send); */

  /* ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)+payloadSize2send); */
  /* ipHdr->_chksum = 0; */
  /* ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr)); */

  /* copyIndirectBuf2HeapHw_swap4(dev,fastPathAction->heapAddr,(uint64_t*) pktBuf, thrId, totalSize2send); */
  /* //--------------------------------------------------------- */
  /* epm_trig_desc_t epm_trig_desc = {}; */
  /* epm_trig_desc.str.action_index = fastPathAction->idx; */
  /* epm_trig_desc.str.payload_size = totalSize2send; */
  /* epm_trig_desc.str.tcp_cs       = calc_pseudo_csum(ipHdr,tcpHdr,tcpPayload,payloadSize2send); */

  /* eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc); */

  return payloadSize2send;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::readyToRecv() {
  struct pollfd fds[1] = {{sock, POLLIN, 0}};

  int rc = lwip_poll(fds, 1, 0);

  if (rc < 0)
    EKA_WARN("Core %u TcpSess %u poll returned rc=%d",coreId,sessId,rc);

  if (fds[0].revents & POLLIN) return 1;

  if (fds[0].revents & POLLERR)
    EKA_WARN("Core %u TcpSess %u has POLLERR (broken pipe)",coreId,sessId);
  if (fds[0].revents & POLLNVAL)
    EKA_WARN("Core %u TcpSess %u has POLLNVAL (socket is not opened)",coreId,sessId);

  return 0;
}

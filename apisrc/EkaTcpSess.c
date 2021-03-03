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
  throttleCounter    = 0;
  maxThrottleCounter = 0;
  txDriverBytes      = 0;
  dummyBytes         = 0;

  fastBytesFromUserChannel = 0;

  connectionEstablished = false;

  memcpy(macSa,_macSa,6);
  memcpy(macDa,_macDa,6);

  if (dev->epmEnabled) {
    if ((sock = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) 
      on_error ("error creating TCP socket");

    if (sessId == CONTROL_SESS_ID) {
      EKA_LOG("Established TCP Session %u for Control Traffic, coreId=%u, EpmRegion = %u",sessId,coreId,EkaEpm::ServiceRegion);
    } else {
      EKA_LOG("sock=%d for: %s:%u --> %s:%u, %s -> %s",sock,
	      EKA_IP2STR(srcIp),srcPort,
	      EKA_IP2STR(dstIp),dstPort,
	      EKA_MAC2STR(macSa),EKA_MAC2STR(macDa));
    }
  } else {
    EKA_LOG("FPGA created IGMP-ONLY network channel");
  }

/* -------------------------------------------- */
  fastPathAction = dev->epm->addAction(EkaEpm::ActionType::TcpFastPath,EkaEpm::ServiceRegion,0,coreId,sessId,0);
  fullPktAction  = dev->epm->addAction(EkaEpm::ActionType::TcpFullPkt, EkaEpm::ServiceRegion,0,coreId,sessId,0);
  emptyAckAction = dev->epm->addAction(EkaEpm::ActionType::TcpEmptyAck,EkaEpm::ServiceRegion,0,coreId,sessId,0);
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

  dev->lwipConnectMtx.lock(); // Consider to move to beginning of connect()
  if (lwip_connect(sock,(const sockaddr*) &dst, sizeof(struct sockaddr_in)) < 0) 
    on_error("socket connect failed");
  //  dev->lwipConnectMtx.unlock();

  EKA_LOG("TCP connected %u:%u : %s:%u --> %s:%u",
	  coreId,sessId,EKA_IP2STR(srcIp),srcPort,EKA_IP2STR(dstIp),dstPort);


  connectionEstablished = true;

  //  LOCK_TCPIP_CORE();
  uint32_t sockOpt = lwip_fcntl(sock, F_GETFL, 0);
  sockOpt |= O_NONBLOCK;
  int ret = lwip_fcntl(sock, F_SETFL, sockOpt);
  if (ret < 0) on_error("setting O_NONBLOCK is failed for sock %d: ret = %d",sock,ret);
  EKA_LOG("O_NONBLOCK is set for socket %d",sock);
  //  UNLOCK_TCPIP_CORE();
  dev->lwipConnectMtx.unlock();

  return 0;
}
/* ---------------------------------------------------------------- */

EkaTcpSess::~EkaTcpSess() {
  EKA_LOG("Closing socket %d for core%u sess%u",sock,coreId,sessId);
  lwip_shutdown(sock,SHUT_RDWR);
  lwip_close(sock);
  EKA_LOG("Closed socket %d for core%u sess%u maxThrottle %uus",sock,coreId,sessId,maxThrottleCounter);
}
/* ---------------------------------------------------------------- */

int EkaTcpSess::preloadNwHeaders() {
  emptyAckAction->setNwHdrs(macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  fastPathAction->setNwHdrs(macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  fullPktAction->setNwHdrs (macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::updateRx(const uint8_t* pkt, uint32_t len) {
  tcpRemoteAckNum = EKA_TCPH_ACKNO(pkt);
  if ( 
      (tcpRemoteAckNum > tcpLocalSeqNum) && //doesntwork with wraparound
      (!(EKA_TCP_SYN(pkt)))
      ) {
    //   Bewlow warning is OK only for wraparound
    //    EKA_WARN(CYN "tcpRemoteAckNum %u > tcpLocalSeqNum %u, delta = %d" RESET,
    //  	     tcpRemoteAckNum, tcpLocalSeqNum, tcpRemoteAckNum - tcpLocalSeqNum);
    return 0; //allow wraparound
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

int EkaTcpSess::setLocalSeqWnd2FPGA() {
    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;
    eka_write(dev,0x30000 + 0x1000*coreId + 8 * (sessId*2),tcpLocalSeqNum);
    eka_write(dev,0x3f000 + 0x100*coreId,desc.desc);

    eka_write(dev,0xe0318,tcpWindow);
    return 0;
}

/* ---------------------------------------------------------------- */
int EkaTcpSess::sendStackPkt(void *pkt, int len) {
  if (EKA_TCP_SYN(pkt)) {
    tcpLocalSeqNum = EKA_TCPH_SEQNO(pkt) + 1;
    tcpLocalSeqNumBase = EKA_TCPH_SEQNO(pkt) + 1;
    tcpWindow = EKA_TCPH_WND(pkt);
    setLocalSeqWnd2FPGA();
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
    tcpRemoteSeqNum = EKA_TCPH_ACKNO(pkt);
    tcpWindow = EKA_TCPH_WND(pkt);
    setRemoteSeqWnd2FPGA();
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
    // This is first ACK on the tcpRemoteSeqNum, so
    // DUMMY PKT has "important" ACK (which was not sent),
    // so Empty ACK is sent from FPGA
    tcpRemoteSeqNum = EKA_TCPH_ACKNO(pkt);
    setRemoteSeqWnd2FPGA();
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
  /* fullPktAction->print("from sendFullPkt:"); */

  fullPktAction->setFullPkt(/* thrId, */buf,(uint)len);
  fullPktAction->send();

  return 0;
}
/* ---------------------------------------------------------------- */

int EkaTcpSess::sendThruStack(void *buf, int len) {
  int res = lwip_write(sock,buf,len);
  return res;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::sendDummyPkt(void *buf, int len) {
  if (tcpRemoteAckNum > dummyBytes + tcpLocalSeqNumBase)
    EKA_WARN(YEL "tcpRemoteAckNum %u > real dummyBytes %u, delta = %d" RESET,
	     tcpRemoteAckNum, dummyBytes + tcpLocalSeqNumBase, tcpRemoteAckNum - dummyBytes - tcpLocalSeqNumBase);

  int sentBytes = lwip_write(sock,buf,len);
  if (sentBytes <= 0) return 0;
  if (sentBytes != len) 
    on_error("Partial Dummy packet: sentBytes %d != len %d",sentBytes, len);
  dummyBytes += len;
  return len;
}

/* ---------------------------------------------------------------- */
int EkaTcpSess::sendPayload(uint thrId, void *buf, int len) {
  if (! dev->exc_active) return 1;

  static const uint TrafficMargin = 4*1024; // just a number

  /* if (/\* txLwipBp *\/       0                            || // lwip socket is unavauilable */
  /*     (fastPathBytes > (txDriverBytes + TrafficMargin)) || // previous TX pkt didn't arrive TX driver */
  /*     (fastPathBytes > (dummyBytes    + TrafficMargin))    // previous TX pkt wasn't sent to lwip as Dummy */
  /*     ) { */
  /*       usleep(0); */
  /*   return 0; // too high tx rate -- Back Pressure */
  /* } */

  if (
      ( (fastPathBytes + tcpLocalSeqNumBase - tcpRemoteAckNum) > TrafficMargin ) &&
      ( (fastPathBytes + tcpLocalSeqNumBase - tcpRemoteAckNum) < TrafficMargin*4 ) //and not a wraparound
      ) {
    throttleCounter++;
    if (throttleCounter > maxThrottleCounter) {
      //      EKA_WARN(YEL "max throttling updated %u" RESET, throttleCounter);
      maxThrottleCounter = throttleCounter;
    }

    //    EKA_WARN(YEL "throttling %u for %u us" RESET, fastPathBytes + tcpLocalSeqNumBase - tcpRemoteAckNum,throttleCounter);
    usleep(throttleCounter);
    return 0;
  }
  throttleCounter = 0;
  uint payloadSize2send = ((uint)len <= (MAX_PAYLOAD_SIZE + 2)) ? (uint)len : MAX_PAYLOAD_SIZE;
  //  if (payloadSize2send <= 2) on_error("len = %d, payloadSize2send=%u,MAX_PKT_SIZE=%u",len,payloadSize2send,MAX_PAYLOAD_SIZE);
  fastPathBytes += payloadSize2send;

  fastPathAction->fastSend(buf, payloadSize2send);

  return payloadSize2send;
}

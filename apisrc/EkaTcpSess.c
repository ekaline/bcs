#include "assert.h"

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
//void hexDump (const char *desc, void *addr, int len);
int ekaAddArpEntry(EkaDev* dev, EkaCoreId coreId, const uint32_t* protocolAddr,const uint8_t* hwAddr);

inline bool eka_is_all_zeros (void* buf, ssize_t size) {
  uint8_t* b = (uint8_t*) buf;
  for (int i=0; i<size; i++) if (b[i] != 0) return false;
  return true;
}

/* ---------------------------------------------------------------- */
EkaTcpSess::EkaTcpSess(EkaDev* pEkaDev, EkaCore* _parent,
		       uint8_t _coreId, uint8_t _sessId, 
		       uint32_t _srcIp, uint32_t _dstIp,
		       uint16_t _dstPort, uint8_t* _macSa) {
  dev     = pEkaDev;
  coreId  = _coreId;
  sessId  = _sessId;
  parent  = _parent;

  srcIp   = _srcIp;
  srcPort =  0;

  dstIp   = _dstIp;
  dstPort = _dstPort;

  connectionEstablished = false;

  memcpy(macSa,_macSa,6);

  if (dev->epmEnabled) {
    if ((sock = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) 
      on_error ("error creating TCP socket");

    if (sessId == CONTROL_SESS_ID) {
      EKA_LOG("Established TCP Session %u for Control Traffic, coreId=%u, EpmRegion = %u",sessId,coreId,EkaEpm::ServiceRegion);
    } else {
      EKA_LOG("sock=%d for: %s:%u --> %s:%u",sock,
	      EKA_IP2STR(srcIp),srcPort,
	      EKA_IP2STR(dstIp),dstPort);
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

ssize_t EkaTcpSess::recv(void *buffer, size_t size, int flags) {
  ssize_t res = lwip_recv(sock, buffer, size, flags);
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
    on_error("Cant set SO_LINGER");

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

  // lwIP will do routing of its own, but we need to do our own routing and ARP
  // resolution first, to set macDa. lwip_connect will begin issuing network
  // packets immediately (e.g., TCP SYNs), and if we don't set macDa now,
  // session lookup won't work when we start asynchronously hearing replies.
  // FIXME: confirm that that is how it really works...
  const bool dstIsOnOurNetwork = (dstIp & parent->netmask) == (srcIp & parent->netmask);
  const uint32_t nextHopIPv4 = dstIsOnOurNetwork ? dstIp : parent->gwIp;
  const char *const who = dstIsOnOurNetwork ? "network neighbor" : "gateway";

  // Try to look in the ARP cache before sending an ARP request on the network.
  // This might succeed even if we've never sent any ARP packets at all because
  // we sometimes place static entries in the ARP table, e.g., for the gateway.
  // FIXME: need LOCK_TCPIP_CORE here? What protects the table?
  uint32_t* ipPtr = NULL;
  uint8_t* macDa_ptr = NULL;
  char arpInCache = false;
  ssize_t arpEntry = etharp_find_addr(pLwipNetIf,
                                      (const ip4_addr_t*)&nextHopIPv4,
                                      (eth_addr**)&macDa_ptr,
                                      (const ip4_addr_t**)&ipPtr);

  if (arpEntry == -1) {
    if (! eka_is_all_zeros(&parent->macDa,6)) {
      EKA_LOG("Lane %u: Adding Dest IP %s with MAC %s to ARP table",
	      coreId,EKA_IP2STR(dstIp),EKA_MAC2STR(parent->macDa));
      if (ekaAddArpEntry(dev, coreId, &dstIp, parent->macDa) == -1)
	on_error("Failed to add static ARP entry for lane %u: %s --> %s",
		 coreId,EKA_IP2STR(dstIp),EKA_MAC2STR(parent->macDa));
      arpEntry = etharp_find_addr(pLwipNetIf,
				  (const ip4_addr_t*)&nextHopIPv4,
				  (eth_addr**)&macDa_ptr,
				  (const ip4_addr_t**)&ipPtr);
    } else {
      // ARP entry is not cached, send a request on the network.
      EKA_LOG("%s %s hw address not cached; sending ARP request from core %u",who,
	      EKA_IP2STR(nextHopIPv4),coreId);
      LOCK_TCPIP_CORE();
      const err_t arpReqErr = etharp_request(pLwipNetIf, (const ip4_addr_t*)&nextHopIPv4);
      if (arpReqErr != ERR_OK) {
	EKA_WARN("etharp_query failed for %s %s: %s (%d)",who,EKA_IP2STR(nextHopIPv4),
		 lwip_strerr(arpReqErr),int(arpReqErr));
	errno = err_to_errno(arpReqErr);
	return -1;
      }
      UNLOCK_TCPIP_CORE();

      // Keep looking until we find it, with microsecond sleeps in between, since
      // it may take several milliseconds for the host to respond to us.
      for (int trials = 0; arpEntry == -1 && dev->servThreadActive && trials < 100000;
	   (void)usleep(1), ++trials) {
	arpEntry = etharp_find_addr(pLwipNetIf,
				    (const ip4_addr_t*)&nextHopIPv4,
				    (eth_addr**)&macDa_ptr,
				    (const ip4_addr_t**)&ipPtr);
      }

      if (! dev->servThreadActive) {
	errno = ENETDOWN; // Network device is gone.
	return -1;
      }
    }
  } else {
    arpInCache = true;
  }

  if (arpEntry == -1) {
    // Still no ARP resolution after sending an explicit request; we don't
    // know how to reach the next hop.
    EKA_WARN("%s address %s is not in the ARP table",who,EKA_IP2STR(nextHopIPv4));
    errno = EHOSTUNREACH;
    return -1;
  }

  if (macDa_ptr == NULL) on_error("macDa_ptr == NULL");
  memcpy(macDa,macDa_ptr,6);
  const char *const arpHow = arpInCache ? "found cached" : "resolved";
  EKA_LOG("%s %s ARP entry %zd with %s %s",arpHow,who,arpEntry,EKA_IP2STR(*ipPtr),EKA_MAC2STR(macDa));

  // Now that macDa is set, preload the network headers.
  preloadNwHeaders();

  // Consider to move to beginning of connect()
  std::unique_lock<std::mutex> lck{dev->lwipConnectMtx};
  if (lwip_connect(sock,(const sockaddr*) &dst, sizeof(struct sockaddr_in)) < 0) 
    return -1;

  EKA_LOG("TCP connected %u:%u : %s:%u --> %s:%u",
	  coreId,sessId,EKA_IP2STR(srcIp),srcPort,EKA_IP2STR(dstIp),dstPort);

  connectionEstablished = true;
  return 0;
}
/* ---------------------------------------------------------------- */

EkaTcpSess::~EkaTcpSess() {
  EKA_LOG("Closing socket %d for core%u sess%u",sock,coreId,sessId);
  lwip_close(sock);
  EKA_LOG("Closed socket %d for core%u sess%u",
	  sock,coreId,sessId);
}
/* ---------------------------------------------------------------- */

int EkaTcpSess::preloadNwHeaders() {
  emptyAckAction->setNwHdrs(macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  fastPathAction->setNwHdrs(macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  fullPktAction->setNwHdrs (macDa,macSa,srcIp,dstIp,srcPort,dstPort);
  return 0;
}

/* ---------------------------------------------------------------- */

static uint64_t seq32to64(uint64_t prev64, uint32_t prev32,
			  uint32_t new32, uint32_t baseOffs) {

  uint64_t new64 = prev64;
  if (new32 < prev32) // wraparound
    new64 += (uint64_t)(1) << 32;

  new64 = (new64 & 0xFFFFFFFF00000000) | (new32 & 0xFFFFFFFF);
  return new64 - baseOffs;
}

int EkaTcpSess::updateRx(const uint8_t* pkt, uint32_t len) {

  uint32_t prevTcpRemoteAckNum = tcpRemoteAckNum.load();
  tcpRemoteAckNum.store(EKA_TCPH_ACKNO(pkt));
  tcpSndWnd.store(uint32_t(EKA_TCPH_WND(pkt)) << tcpSndWndShift);

  realTcpRemoteAckNum.store(seq32to64(realTcpRemoteAckNum,
				      prevTcpRemoteAckNum,
				      EKA_TCPH_ACKNO(pkt),
				      0 /* tcpLocalSeqNumBase */)
			    );
  

  /* // delay RX Ack if corresponding Dummy not pushed to LWIP */
  /* while (dev->exc_active && (realTcpRemoteAckNum.load() > realDummyBytes.load())) { */
  /*   std::this_thread::yield(); */
  /* } */

  // delay RX Ack if corresponding Dummy not pushed to LWIP
  while (dev->exc_active && (realTcpRemoteAckNum.load() > realTxDriverBytes.load())) {
    std::this_thread::yield();
  }
  
  if (EKA_TCP_SYN(pkt)) {
    // SYN/ACK part of the handshake contains the peer's TCP session options.
    assert(EKA_TCP_ACK(pkt));
    const EkaTcpHdr *const tcpHdr = EKA_TCPH(pkt);
    if (EKA_TCPH_HDRLEN_BYTES(tcpHdr) > 20) {
      // Header length > 20, we have TCP options; parse them.
      auto *opt = (const uint8_t *)(tcpHdr + 1);
      while (*opt) {
        switch (*opt) {
        case 1:  // No-op
          ++opt;
          break;
        case 3: // Send window scale.
          tcpSndWndShift = opt[2]; 
	  [[fallthrough]];
        default:
          opt += opt[1];
          break;
        }
      }
    }
  }

  if ( 
      (tcpRemoteAckNum.load() > tcpLocalSeqNum.load()) && //doesntwork with wraparound
      (!(EKA_TCP_SYN(pkt)))
       ) {
    //   Bewlow warning is OK only for wraparound
    //    EKA_WARN(CYN "tcpRemoteAckNum %u > tcpLocalSeqNum %u, delta = %d" RESET,
    //  	     tcpRemoteAckNum, tcpLocalSeqNum, tcpRemoteAckNum - tcpLocalSeqNum);
    return 0; //allow wraparound
  }
#if DEBUG_PRINTS
  TEST_LOG("realTcpRemoteAckNum entering LWIP RX %ju",realTcpRemoteAckNum.load());
#endif  
  return 0;
}

/* ---------------------------------------------------------------- */
int EkaTcpSess::setRemoteSeqWnd2FPGA() {
    // Update FPGA with tcpRemoteSeqNum and tcpRcvWnd
    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;

    uint64_t tcpRcvWnd_64bit = (uint64_t)tcpRcvWnd;
    eka_write(dev,0x60000 + 0x1000*coreId + 8 * (sessId*2),tcpRemoteSeqNum | (tcpRcvWnd_64bit << 32) );
    eka_write(dev,0x6f000 + 0x100*coreId,desc.desc);
    return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::setLocalSeqWnd2FPGA() {
    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;
    eka_write(dev,0x30000 + 0x1000*coreId + 8 * (sessId*2),(uint64_t)tcpLocalSeqNum.load());
    eka_write(dev,0x3f000 + 0x100*coreId,desc.desc);

    eka_write(dev,0xe0318,tcpRcvWnd);
    return 0;
}

int EkaTcpSess::setBlocking(bool b) {
  int flags = lwip_fcntl(sock, F_GETFL, 0);
  if (flags == -1)
    return -1;
  else if (b)
    flags &= ~O_NONBLOCK;
  else
    flags |= O_NONBLOCK;
  const int err = lwip_fcntl(sock, F_SETFL, flags);
  if (!err)
    blocking = b;
  return err;
}

/* ---------------------------------------------------------------- */
int EkaTcpSess::sendStackEthFrame(void *pkt, int len) {
  if (EKA_TCP_SYN(pkt)) {
    tcpLocalSeqNum.store(EKA_TCPH_SEQNO(pkt) + 1);
    //    tcpLocalSeqNumBase.store(EKA_TCPH_SEQNO(pkt) + 1);
    realTcpRemoteAckNum.store(EKA_TCPH_SEQNO(pkt) + 1);
    realFastPathBytes.store(EKA_TCPH_SEQNO(pkt) + 1);
    realTxDriverBytes.store(EKA_TCPH_SEQNO(pkt) + 1);
    realDummyBytes.store(EKA_TCPH_SEQNO(pkt) + 1);

    tcpRcvWnd = EKA_TCPH_WND(pkt);
    setLocalSeqWnd2FPGA();
    sendEthFrame(pkt,len);
    return 0;
  } 
  /* -------------------------------------- */
  if (EKA_TCP_FIN(pkt)) {
    sendEthFrame(pkt,len);
    return 0;    
  }
  /* -------------------------------------- */
  if (! connectionEstablished) {
    // 1st ACK on SYN-ACK
    tcpRemoteSeqNum = EKA_TCPH_ACKNO(pkt);
    tcpRcvWnd = EKA_TCPH_WND(pkt);
    setRemoteSeqWnd2FPGA();
    sendEthFrame(pkt,len);
    return 0;
  }
  /* -------------------------------------- */
  if (EKA_TCPH_SEQNO(pkt) < tcpLocalSeqNum) { 
    // Retransmit
    //    EKA_LOG("Retransmit: Total Len = %u bytes, Seq = %u, tcpLocalSeqNum=%u", len, EKA_TCPH_SEQNO(pkt),tcpLocalSeqNum);
    //    hexDump("RetransmitPkt",pkt,len);
    sendEthFrame(pkt,len);
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
    /* TEST_LOG("Sending Empty ACK %u",EKA_TCPH_ACKNO(pkt)); */
  } else {
    /* TEST_LOG("NOT Sending ACK %u",EKA_TCPH_ACKNO(pkt)); */
  }
  tcpLocalSeqNum.store(EKA_TCPH_SEQNO(pkt) + EKA_TCP_PAYLOAD_LEN(pkt));
#if DEBUG_PRINTS  
  TEST_LOG("Dropping pkt %u bytes, seq = %u",
	   EKA_TCP_PAYLOAD_LEN(pkt),EKA_TCPH_SEQNO(pkt)-tcpLocalSeqNumBase);
#endif  
  if (EKA_IS_TCP_PKT(pkt)) {
    realTxDriverBytes.fetch_add(EKA_TCP_PAYLOAD_LEN(pkt));
  }
  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::sendEthFrame(void *buf, int len) {
  if (! dev->exc_active) {
    errno = ENETDOWN;
    return -1;
  }

  if ((uint)len > MAX_ETH_FRAME_SIZE)
    on_error("Size %d > MAX_ETH_FRAME_SIZE (%d)",len,MAX_ETH_FRAME_SIZE);

  /* hexDump("sendEthFrame",buf,len); */
  /* fullPktAction->print("from sendEthFrame:"); */

  fullPktAction->setEthFrame(/* thrId, */buf,(uint)len);
  fullPktAction->send();

  return 0;
}

/* ---------------------------------------------------------------- */

int EkaTcpSess::lwipDummyWrite(void *buf, int len, uint8_t originatedFromHw) {
  auto p = (const uint8_t*)buf;
  int sentSize = 0;

  if (originatedFromHw)
    realFastPathBytes.fetch_add(len);
  
  do {
    int sentBytes = lwip_write(sock,p,len-sentSize);
    lwip_errno = errno;

    if (sentBytes <= 0 && lwip_errno != EAGAIN && lwip_errno != EWOULDBLOCK) {
      EKA_ERROR("lwip_write(): rc = %d, errno=%d \'%s\'",
		sentBytes,lwip_errno,strerror(lwip_errno));
      return sentBytes;
    }

    if (sentBytes > 0) {
      p += sentBytes;
      sentSize += sentBytes;
    }

    if (sentSize != len) {
      txLwipBp.store(true);
      std::this_thread::yield();
    }
    
  } while (dev->exc_active && sentSize != len);

  txLwipBp.store(false);
  realDummyBytes.fetch_add(len);
  
  return len;
}

/* ---------------------------------------------------------------- */
int EkaTcpSess::sendPayload(uint thrId, void *buf, int len, int flags) {
  if (! dev->exc_active) {
    errno = ENETDOWN;
    return -1;
  }
  if (len == 0) return 0;
  
  static const uint32_t WndMargin = 2*1024; // 1 MTU

  uint payloadSize2send = (uint)len < MAX_PAYLOAD_SIZE ? (uint)len : MAX_PAYLOAD_SIZE;
  int64_t unAckedBytes = realFastPathBytes.load() - realTcpRemoteAckNum.load();

  uint32_t allowedWnd = tcpSndWnd.load() < WndMargin ? 0 : tcpSndWnd.load() - WndMargin;
  if (txLwipBp.load() || unAckedBytes + payloadSize2send > allowedWnd) {
    payloadSize2send = 0;
    errno = EAGAIN;
#if DEBUG_PRINTS
    TEST_LOG("Throttling: txLwipBp=%d,unAckedBytes=%jd,tcpSndWnd=%u,realDummyBytes=%ju,realTxDriverBytes=%ju,realTcpRemoteAckNum=%ju,LwipTxBytes=%jd",
    	     (int)txLwipBp.load(), unAckedBytes,
	     tcpSndWnd.load(), (uint64_t)realDummyBytes.load(),
	     (uint64_t)realTxDriverBytes.load(), realTcpRemoteAckNum.load(),
    	     (int64_t)(realDummyBytes.load() - realTxDriverBytes.load())
	     );
#endif    
    return 0;
  }

  const bool isBlocking = this->blocking && !(flags & MSG_DONTWAIT);
  if (isBlocking && payloadSize2send != (uint)len) {
    // FIXME: do something about this
    EKA_WARN("full blocking emulation is not implemented yet!");
    errno = ENOSYS;
    return -1;
  } 

  realFastPathBytes.fetch_add(payloadSize2send);
  
  fastPathAction->fastSend(buf, payloadSize2send);
  return payloadSize2send;
}

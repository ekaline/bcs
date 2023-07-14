#include "assert.h"

#include "lwip/api.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "lwip/sockets.h"
#include "lwip/tcpip.h"

#include "EkaCore.h"
#include "EkaCsumSSE.h"
#include "EkaDev.h"
#include "EkaEpm.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaTcp.h"
#include "EkaTcpSess.h"
#include "EpmFastPathTemplate.h"
#include "EpmRawPktTemplate.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "ekaNW.h"

class EkaCore;

uint32_t calc_pseudo_csum(const void *ip_hdr,
                          const void *tcp_hdr,
                          const void *payload,
                          uint16_t payload_size);

uint16_t calc_tcp_csum(const void *ip_hdr,
                       const void *tcp_hdr,
                       const void *payload,
                       uint16_t payload_size);

unsigned short csum(const unsigned short *ptr, int nbytes);

int ekaAddArpEntry(EkaDev *dev, EkaCoreId coreId,
                   const uint32_t *protocolAddr,
                   const uint8_t *hwAddr);

void ekaPushLwipRxPkt(EkaDev *dev, EkaCoreId rxCoreId,
                      const void *pkt, uint32_t len);

static inline bool eka_is_all_zeros(void *buf,
                                    ssize_t size) {
  uint8_t *b = (uint8_t *)buf;
  for (int i = 0; i < size; i++)
    if (b[i] != 0)
      return false;
  return true;
}

/* ----------------------------------------------------------------
 */
EkaTcpSess::EkaTcpSess(EkaDev *pEkaDev, EkaCore *_parent,
                       uint8_t _coreId, uint8_t _sessId,
                       uint32_t _srcIp, uint32_t _dstIp,
                       uint16_t _dstPort, uint8_t *_macSa) {
  dev = pEkaDev;
  coreId = _coreId;
  sessId = _sessId;
  parent = _parent;

  srcIp = _srcIp;
  srcPort = 0;

  dstIp = _dstIp;
  dstPort = _dstPort;

  connectionEstablished = false;

  memcpy(macSa, _macSa, 6);

  if (!dev->epmEnabled) {
    EKA_LOG("FPGA created IGMP-ONLY network channel");
    return;
  }

  if ((sock = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0)
    on_error("error creating TCP socket");

  auto idx = coreId * TOTAL_SESSIONS_PER_CORE + sessId;

  if (sessId == CONTROL_SESS_ID) {
    EKA_LOG("Established TCP Session %u for Control "
            "Traffic, coreId=%u, EpmRegion = %u",
            sessId, coreId,
            EkaEpmRegion::Regions::TcpTxFullPkt);
    fullPktAction = dev->epm->addAction(
        EkaEpm::ActionType::TcpFullPkt, idx,
        EkaEpmRegion::Regions::TcpTxFullPkt);
    fullPktAction->setTcpSess(this);
  } else {
    EKA_LOG("sock=%d for: %s:%u --> %s:%u", sock,
            EKA_IP2STR(srcIp), srcPort, EKA_IP2STR(dstIp),
            dstPort);
    fastPathAction = dev->epm->addAction(
        EkaEpm::ActionType::TcpFastPath, idx,
        EkaEpmRegion::Regions::TcpTxFullPkt);
    fastPathAction->setTcpSess(this);
  }

  emptyAckAction = dev->epm->addAction(
      EkaEpm::ActionType::TcpEmptyAck, idx,
      EkaEpmRegion::Regions::TcpTxEmptyAck);
  emptyAckAction->setTcpSess(this);

  controlTcpSess =
      dev->core[coreId]->tcpSess[EkaCore::CONTROL_SESS_ID];
  /* -------------------------------------------- */
}

/* ----------------------------------------------------------------
 */

ssize_t EkaTcpSess::recv(void *buffer, size_t size,
                         int flags) {
  ssize_t res = lwip_recv(sock, buffer, size, flags);
  return res;
}

/* ----------------------------------------------------------------
 */

int EkaTcpSess::bind() {
  int flag = 1;
  if (lwip_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                      (char *)&flag, sizeof(int)) < 0)
    on_error("Cant set TCP_NODELAY");

  struct linger linger = {
      false, // Linger OFF
      0      // Abort pending traffic on close()
  };

  if (lwip_setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger,
                      sizeof(struct linger)) < 0)
    on_error("Cant set SO_LINGER");

  struct sockaddr_in src = {};
  src.sin_addr.s_addr = srcIp;
  src.sin_port = 0; // dummy
  src.sin_family = AF_INET;

  socklen_t addr_len = sizeof(struct sockaddr_in);

  if (lwip_bind(sock, (struct sockaddr *)&src,
                sizeof(struct sockaddr_in)) < 0)
    on_error("error binding socket");

  if (lwip_getsockname(sock, (struct sockaddr *)&src,
                       &addr_len) < 0)
    on_error("error on getsockname");

  srcPort = be16toh(src.sin_port);
  EKA_LOG("Sock %d is binded to %s:%u", sock,
          EKA_IP2STR(src.sin_addr.s_addr),
          be16toh(src.sin_port));

  return 0;
}

/* ----------------------------------------------------------------
 */

int EkaTcpSess::connect() {

  EKA_LOG("Trying to acqwire lwipConnectMtx for "
          "TCP connection %u:%u : %s:%u --> %s:%u",
          coreId, sessId, EKA_IP2STR(srcIp), srcPort,
          EKA_IP2STR(dstIp), dstPort);

  dev->lwipConnectMtx.lock();

  struct sockaddr_in dst = {};
  dst.sin_addr.s_addr = dstIp;
  dst.sin_port = be16toh(dstPort);
  dst.sin_family = AF_INET;

  struct netif *pLwipNetIf =
      (struct netif *)parent->pLwipNetIf;

  EKA_LOG("Establishing TCP for %u:%u : %s:%u --> %s:%u",
          coreId, sessId, EKA_IP2STR(srcIp), srcPort,
          EKA_IP2STR(dstIp), dstPort);

  // lwIP will do routing of its own, but we need to do our
  // own routing and ARP resolution first, to set macDa.
  // lwip_connect will begin issuing network packets
  // immediately (e.g., TCP SYNs), and if we don't set macDa
  // now, session lookup won't work when we start
  // asynchronously hearing replies.
  // FIXME: confirm that that is how it really works...
  const bool dstIsOnOurNetwork =
      (dstIp & parent->netmask) ==
      (srcIp & parent->netmask);
  const uint32_t nextHopIPv4 =
      dstIsOnOurNetwork ? dstIp : parent->gwIp;
  const char *const who =
      dstIsOnOurNetwork ? "network neighbor" : "gateway";

  // Try to look in the ARP cache before sending an ARP
  // request on the network. This might succeed even if
  // we've never sent any ARP packets at all because we
  // sometimes place static entries in the ARP table, e.g.,
  // for the gateway.
  // FIXME: need LOCK_TCPIP_CORE here? What protects the
  // table?
  uint32_t *ipPtr = NULL;
  uint8_t *macDa_ptr = NULL;
  char arpInCache = false;
  ssize_t arpEntry = etharp_find_addr(
      pLwipNetIf, (const ip4_addr_t *)&nextHopIPv4,
      (eth_addr **)&macDa_ptr, (const ip4_addr_t **)&ipPtr);

  if (arpEntry == -1) {
    if (!eka_is_all_zeros(&parent->macDa, 6)) {
      EKA_LOG("Lane %u: Adding Dest IP %s with MAC %s to "
              "ARP table",
              coreId, EKA_IP2STR(dstIp),
              EKA_MAC2STR(parent->macDa));
      if (ekaAddArpEntry(dev, coreId, &dstIp,
                         parent->macDa) == -1)
        on_error("Failed to add static ARP entry for lane "
                 "%u: %s --> %s",
                 coreId, EKA_IP2STR(dstIp),
                 EKA_MAC2STR(parent->macDa));
      arpEntry = etharp_find_addr(
          pLwipNetIf, (const ip4_addr_t *)&nextHopIPv4,
          (eth_addr **)&macDa_ptr,
          (const ip4_addr_t **)&ipPtr);
    } else {
      // ARP entry is not cached, send a request on the
      // network.
      EKA_LOG("%s %s hw address not cached; sending ARP "
              "request from core %u",
              who, EKA_IP2STR(nextHopIPv4), coreId);
      LOCK_TCPIP_CORE();
      const err_t arpReqErr = etharp_request(
          pLwipNetIf, (const ip4_addr_t *)&nextHopIPv4);
      if (arpReqErr != ERR_OK) {
        EKA_WARN("etharp_query failed for %s %s: %s (%d)",
                 who, EKA_IP2STR(nextHopIPv4),
                 lwip_strerr(arpReqErr), int(arpReqErr));
        errno = err_to_errno(arpReqErr);
        UNLOCK_TCPIP_CORE();
        goto CONNECTION_ERROR;
      }
      UNLOCK_TCPIP_CORE();

      // Keep looking until we find it, with microsecond
      // sleeps in between, since it may take several
      // milliseconds for the host to respond to us.
      for (int trials = 0;
           arpEntry == -1 && dev->servThreadActive &&
           trials < 100000;
           (void)usleep(1), ++trials) {
        arpEntry = etharp_find_addr(
            pLwipNetIf, (const ip4_addr_t *)&nextHopIPv4,
            (eth_addr **)&macDa_ptr,
            (const ip4_addr_t **)&ipPtr);
      }

      if (!dev->servThreadActive) {
        errno = ENETDOWN; // Network device is gone.
        goto CONNECTION_ERROR;
      }
    }
  } else {
    arpInCache = true;
  }

  if (arpEntry == -1) {
    // Still no ARP resolution after sending an explicit
    // request; we don't know how to reach the next hop.
    EKA_WARN("%s address %s is not in the ARP table", who,
             EKA_IP2STR(nextHopIPv4));
    errno = EHOSTUNREACH;
    goto CONNECTION_ERROR;
  }

  if (macDa_ptr == NULL)
    on_error("macDa_ptr == NULL");
  memcpy(macDa, macDa_ptr, 6);

  EKA_LOG("%s %s ARP entry %zd with %s %s",
          arpInCache ? "found cached" : "resolved", who,
          arpEntry, EKA_IP2STR(*ipPtr), EKA_MAC2STR(macDa));

  // Now that macDa is set, preload the network headers.
  preloadNwHeaders();

  if (lwip_connect(sock, (const sockaddr *)&dst,
                   sizeof(struct sockaddr_in)) < 0)
    goto CONNECTION_ERROR;

  EKA_LOG("TCP connected %u:%u : %s:%u --> %s:%u", coreId,
          sessId, EKA_IP2STR(srcIp), srcPort,
          EKA_IP2STR(dstIp), dstPort);

  dev->lwipConnectMtx.unlock();
  connectionEstablished = true;
  return 0;

CONNECTION_ERROR:
  dev->lwipConnectMtx.unlock();
  return -1;
}
/* ----------------------------------------------------------------
 */

EkaTcpSess::~EkaTcpSess() {
  EKA_LOG("Closing socket %d for core%u sess%u", sock,
          coreId, sessId);
  lwip_close(sock);
  if (fullPktAction) {
    delete (fullPktAction);
    fullPktAction = NULL;
  }
  if (emptyAckAction) {
    delete (emptyAckAction);
    emptyAckAction = NULL;
  }
  if (fastPathAction) {
    delete (fastPathAction);
    fastPathAction = NULL;
  }
  EKA_LOG("Closed socket %d for core%u sess%u", sock,
          coreId, sessId);
}
/* ----------------------------------------------------------------
 */

int EkaTcpSess::preloadNwHeaders() {
  emptyAckAction->setNwHdrs(macDa, macSa, srcIp, dstIp,
                            srcPort, dstPort);
  emptyAckAction->copyHeap2Fpga();

  if (sessId == CONTROL_SESS_ID) {
    if (fastPathAction)
      on_error("fastPathAction exists for CONTROL_SESS");
    if (!fullPktAction)
      on_error("!fullPktAction for CONTROL_SESS");

    fullPktAction->setNwHdrs(macDa, macSa, srcIp, dstIp,
                             srcPort, dstPort);
  } else {
    if (fullPktAction)
      on_error("fullPktAction exists for CONTROL_SESS");
    if (!fastPathAction)
      on_error("!fastPathAction for CONTROL_SESS");
    fastPathAction->setNwHdrs(macDa, macSa, srcIp, dstIp,
                              srcPort, dstPort);
  }

  return 0;
}

/* ----------------------------------------------------------------
 */

static uint64_t seq32to64(uint64_t prev64, uint32_t prev32,
                          uint32_t new32,
                          uint32_t baseOffs) {

  uint64_t new64 = prev64;
  if (new32 < prev32) // wraparound
    new64 += (uint64_t)(1) << 32;

  new64 =
      (new64 & 0xFFFFFFFF00000000) | (new32 & 0xFFFFFFFF);
  return new64 - baseOffs;
}

void EkaTcpSess::processSynAck(const void *pkt,
                               size_t len) {

  //  hexDump("SynAck",pkt,len);
  // SYN/ACK part of the handshake contains the peer's TCP
  // session options.
  if (!(EKA_TCP_ACK(pkt)))
    on_error("Non ACK");
  const EkaTcpHdr *const tcpHdr = EKA_TCPH(pkt);
  if (EKA_TCPH_HDRLEN_BYTES(tcpHdr) == 20)
    return;
  // Header length > 20, we have TCP options; parse them.
  auto *opt = (const uint8_t *)(tcpHdr + 1);

  int optLen =
      EKA_TCPH_HDRLEN_BYTES(tcpHdr) - sizeof(EkaTcpHdr);
  bool endOfOptions = false;

  char hexBuf[8192];
  hexDump2str("Bug in SYN-ACK TCP options", pkt, len,
              hexBuf, sizeof(hexBuf));

  int i = 0;
  while (!endOfOptions && i < optLen) {
    switch (opt[i]) {
      /* ---------------------- */
    case 0: // endOfOptions
      if (i != optLen - 1) {
        EKA_ERROR("Session %d:%d: Premature endOfOptions "
                  "in :\n%s",
                  coreId, sessId, hexBuf);
        on_error("Session %d:%d: Premature endOfOptions in "
                 ":\n%s",
                 coreId, sessId, hexBuf);
      }
      endOfOptions = true;
      break;
      /* ---------------------- */
    case 1: // NOP
      i++;
      break;
      /* ---------------------- */
    case 2: // MSS
      i++;
      if (opt[i] != 4) {
        EKA_ERROR("Session %d:%d: Unexpected MSS field len "
                  "0x%x in :\n%s",
                  coreId, sessId, opt[i], hexBuf);
        on_error("Session %d:%d: Unexpected MSS field len "
                 "0x%x in :\n%s",
                 coreId, sessId, opt[i], hexBuf);
      }
      i++;
      mss = be16toh(*(uint16_t *)(&opt[i]));
      EKA_LOG("Session %d:%d: MSS = %u (0x%x)", coreId,
              sessId, mss, mss);
      i += 2;
      break;
      /* ---------------------- */
    case 3: // SndWndShift
      i++;
      if (opt[i] != 3) {
        EKA_ERROR("Session %d:%d: Unexpected SndWndShift "
                  "field len 0x%x in :\n%s",
                  coreId, sessId, opt[i], hexBuf);
        on_error("Session %d:%d: Unexpected SndWndShift "
                 "field len 0x%x in :\n%s",
                 coreId, sessId, opt[i], hexBuf);
      }
      i++;
      tcpSndWndShift = opt[i];
      EKA_LOG("Session %d:%d: tcpSndWndShift = %u (0x%x)",
              coreId, sessId, tcpSndWndShift,
              tcpSndWndShift);
      i++;
      endOfOptions =
          true; // we are not interested in other options
      break;
      /* ---------------------- */
    default:
      EKA_ERROR(
          "Session %d:%d: Unexpected option 0x%x in\n%s",
          coreId, sessId, opt[i], hexBuf);
      on_error(
          "Session %d:%d: Unexpected option 0x%x in\n%s",
          coreId, sessId, opt[i], hexBuf);
    }
  }

  /* while (*opt) { */
  /*   switch (*opt) { */
  /*   case 1:  // No-op */
  /* 	++opt; */
  /* 	break; */
  /*   case 3: // Send window scale. */
  /* 	tcpSndWndShift = opt[2];  */
  /* 	[[fallthrough]]; */
  /*   default: */
  /* 	opt += opt[1]; */
  /* 	break; */
  /*   } */
  /* } */

  return;
}

void EkaTcpSess::insertEmptyRemoteAck(uint64_t seq,
                                      const void *pkt) {
  const size_t HdrSize = sizeof(EkaEthHdr) +
                         sizeof(EkaIpHdr) +
                         sizeof(EkaTcpHdr);

  uint8_t ackPkt[HdrSize] = {};
  memcpy(ackPkt, pkt, HdrSize);

  auto ipHdr =
      (EkaIpHdr *)((uint8_t *)ackPkt + sizeof(EkaEthHdr));
  auto tcpHdr =
      (EkaTcpHdr *)((uint8_t *)ipHdr + sizeof(EkaIpHdr));

  ipHdr->_chksum = 0;
  ipHdr->_len =
      be16toh(sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));
  tcpHdr->chksum = 0;

  ipHdr->_chksum = ekaCsum((const unsigned short *)ipHdr,
                           sizeof(EkaIpHdr));
  tcpHdr->ackno = be32toh(uint32_t(seq & 0xFFFFFFFF));

  //  tcpHdr->chksum = calc_tcp_csum(ipHdr,tcpHdr,NULL,0);
  tcpHdr->chksum = ekaTcpCsum(ipHdr, tcpHdr);

#if _EKA_PRINT_INSERTED_ACK
  char emptyAckStr[2048] = {};
  hexDump2str("Empty Ack", ackPkt, sizeof(ackPkt),
              emptyAckStr, sizeof(emptyAckStr));
  TEST_LOG("Inserting: ACK %ju (%u == 0x%x): %s", seq,
           be32toh(tcpHdr->ackno), be32toh(tcpHdr->ackno),
           emptyAckStr);
#endif
  ekaPushLwipRxPkt(dev, coreId, ackPkt, sizeof(ackPkt));

  return;
}

int EkaTcpSess::updateRx(const uint8_t *pkt, uint32_t len) {

  uint32_t prevTcpRemoteAckNum = tcpRemoteAckNum.load();
  tcpRemoteAckNum.store(EKA_TCPH_ACKNO(pkt));
  tcpSndWnd.store(uint32_t(EKA_TCPH_WND(pkt))
                  << tcpSndWndShift);

  realTcpRemoteAckNum.store(seq32to64(
      realTcpRemoteAckNum, prevTcpRemoteAckNum,
      EKA_TCPH_ACKNO(pkt), 0 /* tcpLocalSeqNumBase */));

  // delay RX Ack if corresponding Seq is not sent on LWIP
  // TX yet
  while (dev->exc_active && (realTcpRemoteAckNum.load() >
                             realTxDriverBytes.load())) {
    if (realTxDriverBytes.load() > lastInsertedEmptyAck) {
      lastInsertedEmptyAck = realTxDriverBytes.load();
      insertEmptyRemoteAck(lastInsertedEmptyAck, pkt);
    }
#if DEBUG_PRINTS
    else {
      TEST_LOG(
          "realTxDriverBytes=%ju,lastInsertedEmptyAck=%ju",
          realTxDriverBytes.load(), lastInsertedEmptyAck);
    }
#endif
    std::this_thread::yield();
  }

  if (EKA_TCP_SYN(pkt))
    processSynAck(pkt, len);

#if DEBUG_PRINTS
  TEST_LOG("realTcpRemoteAckNum entering LWIP RX %ju",
           realTcpRemoteAckNum.load());
#endif
  return 0;
}

/* ----------------------------------------------------------------
 */
int EkaTcpSess::setRemoteSeqWnd2FPGA() {
  // Update FPGA with tcpRemoteSeqNum and tcpRcvWnd
#if 0  
    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;

    uint64_t tcpRcvWnd_64bit = (uint64_t)tcpRcvWnd;
    eka_write(dev,0x60000 + 0x1000*coreId + 8 * (sessId*2),
	      tcpRemoteSeqNum | (tcpRcvWnd_64bit << 32) );
    eka_write(dev,0x6f000 + 0x100*coreId,desc.desc);
#endif
  uint64_t data =
      tcpRemoteSeqNum | ((uint64_t)tcpRcvWnd << 32);
  updateFpgaCtx<RemoteSeqWnd>(data);
  return 0;
}

/* ----------------------------------------------------------------
 */

int EkaTcpSess::setLocalSeqWnd2FPGA() {
#if 0
    exc_table_desc_t desc = {};
    desc.td.source_bank = 0;
    desc.td.source_thread = sessId;
    desc.td.target_idx = (uint32_t)sessId;
    eka_write(dev,0x30000 + 0x1000*coreId + 8 * (sessId*2),
	      (uint64_t)tcpLocalSeqNum.load());
    eka_write(dev,0x3f000 + 0x100*coreId,desc.desc);
#endif
  updateFpgaCtx<LocalSeqWnd>(
      (uint64_t)tcpLocalSeqNum.load());

#if 0
    // ???
    eka_write(dev,0xe0318,tcpRcvWnd);
#endif
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

/* ----------------------------------------------------------------
 */
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
    controlTcpSess->sendEthFrame(pkt, len);
    return 0;
  }
  /* -------------------------------------- */
  if (EKA_TCP_FIN(pkt)) {
    controlTcpSess->sendEthFrame(pkt, len);
    return 0;
  }
  /* -------------------------------------- */
  if (!connectionEstablished) {
    // 1st ACK on SYN-ACK
    tcpRemoteSeqNum = EKA_TCPH_ACKNO(pkt);
    tcpRcvWnd = EKA_TCPH_WND(pkt);
    setRemoteSeqWnd2FPGA();

    controlTcpSess->sendEthFrame(pkt, len);
    return 0;
  }
  /* -------------------------------------- */
  //  if (EKA_TCPH_SEQNO(pkt) < tcpLocalSeqNum) {
  if (TCP_SEQ_LT((EKA_TCPH_SEQNO(pkt)), tcpLocalSeqNum)) {

    // Retransmit
    //    EKA_LOG("Retransmit: Total Len = %u bytes, Seq =
    //    %u, tcpLocalSeqNum=%u",
    //            len, EKA_TCPH_SEQNO(pkt),tcpLocalSeqNum);
    //    hexDump("RetransmitPkt",pkt,len);
    controlTcpSess->sendEthFrame(pkt, len);
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
    // emptyAckAction->copyHeap2FpgaAndSend();
    /* TEST_LOG("Sending Empty ACK %u",EKA_TCPH_ACKNO(pkt));
     */
  } else {
    /* TEST_LOG("NOT Sending ACK %u",EKA_TCPH_ACKNO(pkt));
     */
  }
  tcpLocalSeqNum.store(EKA_TCPH_SEQNO(pkt) +
                       EKA_TCP_PAYLOAD_LEN(pkt));
#if DEBUG_PRINTS
  TEST_LOG("Dropping pkt %u bytes, seq = %u",
           EKA_TCP_PAYLOAD_LEN(pkt),
           EKA_TCPH_SEQNO(pkt) - tcpLocalSeqNumBase);
#endif
  if (EKA_IS_TCP_PKT(pkt)) {
    realTxDriverBytes.fetch_add(EKA_TCP_PAYLOAD_LEN(pkt));
  }
  return 0;
}

/* ----------------------------------------------------------------
 */

int EkaTcpSess::sendEthFrame(void *buf, int len) {
  if (!dev->exc_active) {
    errno = ENETDOWN;
    return -1;
  }

  if ((uint)len > MAX_ETH_FRAME_SIZE)
    on_error("Size %d > MAX_ETH_FRAME_SIZE (%d)", len,
             MAX_ETH_FRAME_SIZE);

  fullPktAction->sendFullPkt(buf, (uint)len);

  return 0;
}

/* ----------------------------------------------------------------
 */

int EkaTcpSess::lwipDummyWrite(void *buf, int len,
                               uint8_t originatedFromHw) {
  auto p = (const uint8_t *)buf;
  int sentSize = 0;

  if (originatedFromHw) {
    realFastPathBytes.fetch_add(len);
    dev->globalFastPathBytes.fetch_add(len);
  }
  do {
    int sentBytes = lwip_write(sock, p, len - sentSize);
    lwip_errno = errno;

    if (sentBytes <= 0 && lwip_errno != EAGAIN &&
        lwip_errno != EWOULDBLOCK) {
      EKA_ERROR(
          "lwip_write(): len= %d, rc = %d, errno=%d \'%s\'",
          sentBytes, len - sentSize, lwip_errno,
          strerror(lwip_errno));
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
  dev->globalDummyBytes.fetch_add(len);

  return len;
}
int EkaTcpSess::preSendCheck(int len, int flags) {
  if (!dev->exc_active) {
    errno = ENETDOWN;
    return -1;
  }

  if (len <= 0)
    return 0;

  const uint32_t WndMargin = 2 * 1024; // 2 real fire MTUs
  const uint64_t FpgaInFlightLimit = 8 * 1024;

  uint payloadSize2send = (uint)len < MAX_PAYLOAD_SIZE
                              ? (uint)len
                              : MAX_PAYLOAD_SIZE;
  int64_t unAckedBytes =
      realFastPathBytes.load() - realTcpRemoteAckNum.load();

  // unAckedBytes can be negative if HW fire was acked faster than
  // getting to realFastPathBytes
  if (unAckedBytes < 0)
    unAckedBytes = 0;
  //      on_error("unAckedBytes %jd < 0",unAckedBytes);
  
  auto currTcpSndWnd = tcpSndWnd.load();
  uint32_t allowedWnd = currTcpSndWnd < WndMargin
                            ? 0
                            : currTcpSndWnd - WndMargin;
  /* const uint32_t AllowedWndSafetyMargin = 1024 * 1024; //
   * Big number */
  /* if (allowedWnd > AllowedWndSafetyMargin) { */
  /*   EKA_ERROR("allowedWnd %u > AllowedWndSafetyMargin %u:
   * currTcpSndWnd = %u", */
  /* 	      allowedWnd, AllowedWndSafetyMargin,
   * currTcpSndWnd); */
  /*   on_error("allowedWnd %u > AllowedWndSafetyMargin %u:
   * currTcpSndWnd = %u", */
  /* 	      allowedWnd, AllowedWndSafetyMargin,
   * currTcpSndWnd); */
  /* } */
  if (txLwipBp.load() ||
      unAckedBytes + payloadSize2send > allowedWnd ||
      dev->globalFastPathBytes.load() -
              dev->globalDummyBytes.load() >
          FpgaInFlightLimit) {

    payloadSize2send = 0;
    errno = EAGAIN;
#if DEBUG_PRINTS
    TEST_LOG("Throttling: "
             "txLwipBp=%d,unAckedBytes=%jd,tcpSndWnd=%u,"
             "realDummyBytes=%ju,realTxDriverBytes=%ju,"
             "realTcpRemoteAckNum=%ju,LwipTxBytes=%jd",
             (int)txLwipBp.load(), unAckedBytes,
             tcpSndWnd.load(),
             (uint64_t)realDummyBytes.load(),
             (uint64_t)realTxDriverBytes.load(),
             realTcpRemoteAckNum.load(),
             (int64_t)(realDummyBytes.load() -
                       realTxDriverBytes.load()));
#endif
    return 0;
  }

  const bool isBlocking =
      this->blocking && !(flags & MSG_DONTWAIT);
  if (isBlocking && payloadSize2send != (uint)len) {
    // FIXME: do something about this
    EKA_WARN(
        "full blocking emulation is not implemented yet!");
    errno = ENOSYS;
    return -1;
  }

  realFastPathBytes.fetch_add(payloadSize2send);
  dev->globalFastPathBytes.fetch_add(payloadSize2send);

  return payloadSize2send;
}
/* ----------------------------------------------------------------
 */
int EkaTcpSess::sendPayload(uint thrId, void *buf, int len,
                            int flags) {
  int payloadSize2send = preSendCheck(len, flags);
  if (payloadSize2send <= 0)
    return 0;

  fastPathAction->fastSend(buf, payloadSize2send);
  return payloadSize2send;
}

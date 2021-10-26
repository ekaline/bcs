#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>

#include "EkaFhPlrGr.h"
#include "EkaFhPlrParser.h"
#include "EkaFhThreadAttr.h"

int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);
int ekaTcpConnect(uint32_t ip, uint16_t port);

using namespace Plr;

static int getPillarProductIdFromProductMask(int productMask) {
  if (productMask & PM_VanillaBook)
      return NYSE_BBO_ProductId;
  if (productMask & PM_VanillaTrades)
    return NYSE_Trades_ProductId;
  if (productMask & PM_ComplexAuction)
    return NYSE_Auction_ProductId;
  if (productMask & PM_ComplexBook)
    return NYSE_Complex_ProductId;
  on_error("unexpected product mask %d: only vanilla_book, vanilla_trades, "
	   "complex_book and complex_auction are allowed", productMask);
  /* switch (productMask) { */
  /* case PM_VanillaBook: */
  /*   return NYSE_BBO_ProductId; */
  /* case PM_VanillaTrades: */
  /*   return NYSE_Trades_ProductId; */
  /* case PM_ComplexAuction: */
  /*   return NYSE_Auction_ProductId; */
  /* default: */
  /*   on_error("unexpected product mask %d, only vanilla_book, vanilla_trades, " */
  /*            "and complex_auction are allowed", productMask); */
  /* } */
}

static bool sendSymbolIndexMappingRequest(EkaFhPlrGr* gr, int sock) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  uint8_t pkt[1500] = {};
  auto pktHdr {reinterpret_cast<PktHdr*>(pkt)};
  auto msg {reinterpret_cast<SymbolIndexMappingRequest*>(pkt + sizeof(*pktHdr))};

  pktHdr->pktSize      = sizeof(*pktHdr) + sizeof(*msg);
  pktHdr->deliveryFlag = static_cast<decltype(pktHdr->deliveryFlag)>(DeliveryFlag::Original);
  pktHdr->numMsgs      = 1;
  pktHdr->seqNum       = 1;
  pktHdr->seconds      = 0;
  pktHdr->ns           = 0;


  msg->hdr.size = sizeof(*msg);
  msg->hdr.type = static_cast<decltype(msg->hdr.type)>(MsgType::SymbolIndexMappingRequest);
  msg->SymbolIndex = 0;
  strncpy(msg->SourceID,gr->sourceId,sizeof msg->SourceID);
  msg->ProductID   = getPillarProductIdFromProductMask(gr->productMask);
  msg->ChannelID = gr->channelId;
  msg->RetransmitMethod = 0;

  EKA_LOG("Sending SymbolIndexMappingRequest: SymbolIndex=%u,SourceID=\'%s\',ProductID=%u,ChannelID=%u",
	  msg->SymbolIndex,msg->SourceID,msg->ProductID,msg->ChannelID);
  int rc = send(sock,pkt,pktHdr->pktSize,0);
  if (rc <= 0) {
    EKA_WARN("Tcp send of msg size %d to sock %d returned rc = %d (%s)",
	     pktHdr->pktSize,sock,rc,strerror(errno));
    return false;
  }
  return true;
}

static bool sendRefreshRequest(EkaFhPlrGr* gr, int sock) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  uint8_t pkt[1500] = {};
  auto pktHdr {reinterpret_cast<PktHdr*>(pkt)};
  auto msg {reinterpret_cast<RefreshRequest*>(pkt + sizeof(*pktHdr))};

  pktHdr->pktSize      = sizeof(*pktHdr) + sizeof(*msg);
  pktHdr->deliveryFlag = static_cast<decltype(pktHdr->deliveryFlag)>(DeliveryFlag::Original);
  pktHdr->numMsgs      = 1;
  pktHdr->seqNum       = 1;
  pktHdr->seconds      = 0;
  pktHdr->ns           = 0;


  msg->hdr.size    = sizeof(*msg);
  msg->hdr.type    = static_cast<decltype(msg->hdr.type)>(MsgType::RefreshRequest);
  msg->SymbolIndex = 0;
  strncpy(msg->SourceID,gr->sourceId,sizeof msg->SourceID);
  msg->ProductID   = getPillarProductIdFromProductMask(gr->productMask);
  msg->ChannelID   = gr->channelId;

  EKA_LOG("Sending RefreshRequest: SymbolIndex=%u,SourceID=\'%s\',ProductID=%u,ChannelID=%u",
	  msg->SymbolIndex,msg->SourceID,msg->ProductID,msg->ChannelID);
  int rc = send(sock,pkt,pktHdr->pktSize,0);
  if (rc <= 0) {
    EKA_WARN("Tcp send of msg size %d to sock %d returned rc = %d (%s)",
	     pktHdr->pktSize,sock,rc,strerror(errno));
    return false;
  }
  return true;
}

static bool sendRetransmissionRequest(EkaFhPlrGr* gr, int sock, uint32_t start, uint32_t end) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  uint8_t pkt[1500] = {};
  auto pktHdr {reinterpret_cast<PktHdr*>(pkt)};
  auto msg {reinterpret_cast<RetransmissionRequest*>(pkt + sizeof(*pktHdr))};

  pktHdr->pktSize      = sizeof(*pktHdr) + sizeof(*msg);
  pktHdr->deliveryFlag = static_cast<decltype(pktHdr->deliveryFlag)>(DeliveryFlag::Original);
  pktHdr->numMsgs      = 1;
  pktHdr->seqNum       = 1;
  pktHdr->seconds      = 0;
  pktHdr->ns           = 0;


  msg->hdr.size    = sizeof(*msg);
  msg->hdr.type    = static_cast<decltype(msg->hdr.type)>(MsgType::RetransmissionRequest);
  msg->BeginSeqNum = start;
  msg->EndSeqNum   = end;
  strncpy(msg->SourceID,gr->sourceId,sizeof msg->SourceID);
  msg->ProductID   = getPillarProductIdFromProductMask(gr->productMask);
  msg->ChannelID   = gr->channelId;

  EKA_LOG("Sending RetransmissionRequest: BeginSeqNum=%u,EndSeqNum=%u,SourceID=\'%s\',ProductID=%u,ChannelID=%u",
	  msg->BeginSeqNum,msg->EndSeqNum,
	  msg->SourceID,msg->ProductID,msg->ChannelID);
  int rc = send(sock,pkt,pktHdr->pktSize,0);
  if (rc <= 0) {
    EKA_WARN("Tcp send of msg size %d to sock %d returned rc = %d",
	     pktHdr->pktSize,sock,rc);
    return false;
  }
  return true;
}

static bool sendHeartbeatResponse(EkaFhPlrGr* gr, int sock) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  uint8_t pkt[1500] = {};
  auto pktHdr {reinterpret_cast<PktHdr*>(pkt)};
  auto msg {reinterpret_cast<HeartbeatResponse*>(pkt + sizeof(*pktHdr))};

  pktHdr->pktSize      = sizeof(*pktHdr) + sizeof(*msg);
  pktHdr->deliveryFlag = static_cast<decltype(pktHdr->deliveryFlag)>(DeliveryFlag::Heartbeat);
  pktHdr->numMsgs      = 0;
  pktHdr->seqNum       = 1;
  pktHdr->seconds      = 0;
  pktHdr->ns           = 0;

  msg->hdr.size = sizeof(*msg);
  msg->hdr.type = static_cast<decltype(msg->hdr.type)>(MsgType::HeartbeatResponse);
  strncpy(msg->SourceID,gr->sourceId,sizeof msg->SourceID);

  int rc = send(sock,pkt,pktHdr->pktSize,0);
  if (rc <= 0) {
    EKA_WARN("Tcp send of msg size %d to sock %d returned rc = %d",
	     pktHdr->pktSize,sock,rc);
    return false;
  }
  EKA_LOG("Heartbeat response sent");
  return true;
}

static bool getRequestResponse(EkaFhPlrGr* gr, int sock, EkaFhMode op) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};
  EKA_LOG("Waiting for %s Refresh Response",EkaFhMode2STR(op));
  
  static const int TimeOut = 2; // seconds
  struct timeval tv = {.tv_sec = TimeOut}; 
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  
  while (1) {
    PktHdr pktHdr{};
    int rc = recv(sock,&pktHdr,sizeof(pktHdr),MSG_WAITALL);
    if (rc <= 0) on_error("\'%s\', rc=%d",strerror(errno),rc);
    
    EKA_LOG("Received Pkt Hdr: pktSize = %u, DeliveryFlag = %s, numMsgs = %u",
	    pktHdr.pktSize,deliveryFlag2str(pktHdr.deliveryFlag).c_str(),pktHdr.numMsgs);

    if (static_cast<DeliveryFlag>(pktHdr.deliveryFlag) == DeliveryFlag::Heartbeat) {
      EKA_LOG("TCP Heartbeat received");
      sendHeartbeatResponse(gr,sock);
      continue;
    }
    if (static_cast<DeliveryFlag>(pktHdr.deliveryFlag) != DeliveryFlag::Original)
      on_error("Unexpexted DeliveryFlag %s",deliveryFlag2str(pktHdr.deliveryFlag).c_str());

    for (uint i = 0; i < pktHdr.numMsgs; i++) {
      char msgBuf[1500] = {};
      auto msgHdr {reinterpret_cast<MsgHdr*>(msgBuf)};

      rc = recv(sock,msgHdr,sizeof(*msgHdr),MSG_WAITALL);
      if (rc <= 0) on_error("\'%s\', rc=%d",strerror(errno),rc);
	
      EKA_LOG("MsgType %u (%s)",msgHdr->type,msgType2str(msgHdr->type).c_str());

      char* msgData = msgBuf + sizeof(*msgHdr);
      rc = recv(sock,msgData,msgHdr->size,MSG_WAITALL);
      if (rc <= 0) on_error("\'%s\', rc=%d",strerror(errno),rc);

      if (static_cast<MsgType>(msgHdr->type) != MsgType::RequestResponse) continue;

      auto msg {reinterpret_cast<const RequestResponse*>(msgHdr)};
	
      switch(msg->Status) {
      case '0' :
	EKA_LOG("Message was accepted");
	return true;
      case '1' :
	EKA_LOG("Rejected due to an Invalid Source ID");
	return false;
      case '3' :
	EKA_LOG("Rejected due to maximum sequence range (see threshold limits)");
	return false;
      case '4' :
	EKA_LOG("Rejected due to maximum request in a day");
	return false;
      case '5' :
	EKA_LOG("Rejected due to maximum number of refresh requests in a day");
	return false;
      case '6' :
	EKA_LOG("Rejected. Request message SeqNum TTL (Time to live) is too old. Use refresh to recover current state if necessary.");
	return false;
      case '7' :
	EKA_LOG("Rejected due to an Invalid Channel ID");
	return false;
      case '8' :
	EKA_LOG("Rejected due to an Invalid Product ID");
	return false;
      case '9' :
	EKA_LOG("Rejected due to: 1) Invalid MsgType, or 2) Mismatch between MsgType and MsgSize");
	return false;
      default :
	on_error("Unexpected response Status: \'%c\'",msg->Status);
      }
    } // for
  } // while (1)
  return false;
}

static bool definitionsRefreshTcp(EkaFhPlrGr* gr, int sock) {
  if (!gr) on_error("gr == NULL");
//  auto dev {gr->dev};
  if (! sendSymbolIndexMappingRequest(gr,sock))
    on_error("sendSymbolIndexMappingRequest failed");
  if (! getRequestResponse(gr,sock,EkaFhMode::DEFINITIONS))
    on_error("getRequestResponse failed");
  
  return true;
}

static bool snapshotRefreshTcp(EkaFhPlrGr* gr, int sock) {
  if (!gr) on_error("gr == NULL");
//  auto dev {gr->dev};
  if (! sendRefreshRequest(gr,sock))
    on_error("sendRefreshRequest failed");
  if (! getRequestResponse(gr,sock,EkaFhMode::SNAPSHOT))
    on_error("getRequestResponse failed");
  
  return true;
}

static bool recoveryRetransmitTcp(EkaFhPlrGr* gr, int sock, uint32_t start, uint32_t end) {
  if (!gr) on_error("gr == NULL");
//  auto dev {gr->dev};
  if (! sendRetransmissionRequest(gr,sock,start,end))
    on_error("RetransmissionRequest failed");
  if (! getRequestResponse(gr,sock,EkaFhMode::RECOVERY))
    on_error("getRequestResponse failed");
  
  return true;
}

static bool establishConnections(EkaFhPlrGr* gr, EkaFhMode op,
				 uint64_t start, uint64_t end,
				 int* udpSock, int* tcpSock) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};
  int rc = 0;
  char buf[2000] = {};
  uint16_t udpPort = op == EkaFhMode::RECOVERY ? gr->retransUdpPort : gr->refreshUdpPort;
  uint32_t udpIp   = op == EkaFhMode::RECOVERY ? gr->retransUdpIp   : gr->refreshUdpIp;
  uint16_t tcpPort = op == EkaFhMode::RECOVERY ? gr->retransTcpPort : gr->refreshTcpPort;
  uint32_t tcpIp   = op == EkaFhMode::RECOVERY ? gr->retransTcpIp   : gr->refreshTcpIp;

  EKA_LOG("%s:%u: Connecting to %s UDP %s:%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),
	  EKA_IP2STR(udpIp),be16toh(udpPort));
  
  *udpSock = ekaUdpMcConnect(dev,udpIp,udpPort);
  if (*udpSock < 0)
    on_error("%s:%u: failed to open %s UDP socket %s:%u",
	     EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),
	     EKA_IP2STR(udpIp),be16toh(udpPort));

  EKA_LOG("%s:%u: Waiting for UDP RX pkt before openning %s TCP connection",
	     EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op));
  rc = recvfrom(*udpSock, buf, sizeof(buf), MSG_WAITALL, NULL, NULL);
  if (rc <= 0)
    on_error("%s:%u: Failed %s UDP recv (rc = %d) from %s:%u",
	     EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),
	     rc,EKA_IP2STR(udpIp),be16toh(udpPort));

  EKA_LOG("%s:%u: Connecting to %s TCP server %s:%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),
	  EKA_IP2STR(tcpIp),be16toh(tcpPort));
  
  *tcpSock = ekaTcpConnect(tcpIp,tcpPort);
  if (*tcpSock < 0)
    on_error("%s:%u: failed to open %s TCP socket %s:%u",
	     EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op),
	     EKA_IP2STR(tcpIp),be16toh(tcpPort));

  switch (op) {
  case EkaFhMode::DEFINITIONS :
    if (definitionsRefreshTcp(gr,*tcpSock)) return true;
    break;
  case EkaFhMode::SNAPSHOT :
    if (snapshotRefreshTcp(gr,*tcpSock)) return true;
    break;
  case EkaFhMode::RECOVERY :
    if (recoveryRetransmitTcp(gr,*tcpSock,start,end)) return true;
    break;
  default:
    on_error("Unexpected op = %d (%s)",(int)op,EkaFhMode2STR(op));
  }
  on_error("%s TCP handshake Failed",EkaFhMode2STR(op));

  return false;
}

static bool processRetransUdpPkt(const EfhRunCtx* pEfhRunCtx, EkaFhPlrGr* gr,
				 const uint8_t* p,EkaFhMode op,
				 uint64_t start, uint64_t end) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};
  
  auto pktHdr {reinterpret_cast<const PktHdr* >(p)};
  p += sizeof(*pktHdr);

  switch (static_cast<DeliveryFlag>(pktHdr->deliveryFlag)) {
    /* ------------------------------------------ */
  case DeliveryFlag::SinglePktRetransmit :
  case DeliveryFlag::PartOfRetransmit :
    break;    
    /* ------------------------------------------ */
  case DeliveryFlag::MsgUnavail : // TO BE FIXED!!!
    break;
    /* ------------------------------------------ */
  case DeliveryFlag::StratOfRefresh :
  case DeliveryFlag::PartOfRefresh :
  case DeliveryFlag::EndOfRefresh :
  case DeliveryFlag::Heartbeat :
  case DeliveryFlag::Failover :
  case DeliveryFlag::SeqReset :
  case DeliveryFlag::SinglePktRefresh :
  case DeliveryFlag::Original :
    return false; // ignore the packet
    /* ------------------------------------------ */
  default :
    on_error("Unexpected DeliveryFlag %u (%s) at Retrans: seqNum=%u, numMsgs=%u",
	     pktHdr->deliveryFlag,deliveryFlag2str(pktHdr->deliveryFlag).c_str(),
	     pktHdr->seqNum, pktHdr->numMsgs);
  }
  EKA_LOG("%s: Re-trans UDP DeliveryFlag=\'%s\',start=%ju,end=%ju,expectedSeq=%ju,pktSeq=%u,numMsgs=%u",
	  EkaFhMode2STR(op),
	  deliveryFlag2str(pktHdr->deliveryFlag).c_str(),
	  start,end,gr->expected_sequence,
	  pktHdr->seqNum, pktHdr->numMsgs);
  
  uint32_t msgSeq = pktHdr->seqNum;
  for (auto i = 0; i < pktHdr->numMsgs; i++) {
    auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};
    if (gr->expected_sequence == msgSeq) {
      gr->parseMsg(pEfhRunCtx,p,msgSeq,op);
      if (msgSeq == end) return true;
      msgSeq++;
      gr->expected_sequence++;
    }
    p += msgHdr->size;
  }

  return false;
}

static bool processRefreshUdpPkt(const EfhRunCtx* pEfhRunCtx, EkaFhPlrGr* gr,
				 const uint8_t* p,EkaFhMode op, bool* myRefreshStarted) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};
  
  auto pktHdr {reinterpret_cast<const PktHdr* >(p)};
  p += sizeof(*pktHdr);

  bool firstPkt = false;
  bool lastPkt  = false;

  switch (static_cast<DeliveryFlag>(pktHdr->deliveryFlag)) {
    /* ------------------------------------------ */
  case DeliveryFlag::StratOfRefresh :
    firstPkt = true;
    if (*myRefreshStarted)
      on_error("%s:%u DeliveryFlag::StratOfRefresh accepted during "
	       "active %s Refresh cycle",
	       EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op));
    
    *myRefreshStarted = true;
    EKA_LOG("%s:%u StratOfRefresh: myRefreshStarted = %d",
	    EKA_EXCH_DECODE(gr->exch),gr->id,(int)*myRefreshStarted);
    break;
    /* ------------------------------------------ */
  case DeliveryFlag::PartOfRefresh :
    if (! *myRefreshStarted) return false;    
    break;
    /* ------------------------------------------ */
  case DeliveryFlag::EndOfRefresh :
    EKA_LOG("%s:%u EndOfRefresh: myRefreshStarted = %d",
	    EKA_EXCH_DECODE(gr->exch),gr->id,(int)*myRefreshStarted);
    if (! *myRefreshStarted) return false;    
    lastPkt = true;
    break;
    /* ------------------------------------------ */
  case DeliveryFlag::Heartbeat :
  case DeliveryFlag::Failover :
  case DeliveryFlag::SeqReset :
  case DeliveryFlag::SinglePktRefresh : // Single pkt refresh means "Single symbol", we use "All symbols"
  case DeliveryFlag::SinglePktRetransmit :
  case DeliveryFlag::PartOfRetransmit :
  case DeliveryFlag::Original :
  case DeliveryFlag::MsgUnavail :
    return false; // ignore the packet
    /* ------------------------------------------ */
  default :
    on_error("Unexpected DeliveryFlag %u (%s) at Refresh: seqNum=%u, numMsgs=%u",
	     pktHdr->deliveryFlag,deliveryFlag2str(pktHdr->deliveryFlag).c_str(),
	     pktHdr->seqNum, pktHdr->numMsgs);
  }

  auto firstMsgHdr {reinterpret_cast<const MsgHdr*>(p)};
  if (op == EkaFhMode::DEFINITIONS && firstMsgHdr->type == MsgType::RefreshHeader)
    return false;

  if (op == EkaFhMode::SNAPSHOT && firstMsgHdr->type != MsgType::RefreshHeader)
    return false;

  if (firstPkt && op == EkaFhMode::SNAPSHOT) {
    if (firstMsgHdr->size != sizeof(RefreshHeader))
      on_error("At first SNAPSHOT packet firstMsgHdr->size = %u",
	       firstMsgHdr->size);
    auto firstRefreshHdr {reinterpret_cast<const RefreshHeader*>(firstMsgHdr)};
    gr->seq_after_snapshot = firstRefreshHdr->LastSeqNum + 1;
    EKA_LOG("%s:%u: First Refresh Header: LastSeqNum = %u, seq_after_snapshot = %ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,
	    firstRefreshHdr->LastSeqNum,gr->seq_after_snapshot);
  }
  
  for (auto i = 0; i < pktHdr->numMsgs; i++) {
    auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};
    gr->parseMsg(pEfhRunCtx,p,0,op);
    p += msgHdr->size;
  }
  if (lastPkt) return true;
  return false;
}

bool plrRecovery(const EfhRunCtx* pEfhRunCtx, EkaFhPlrGr* gr, EkaFhMode op,
		 uint64_t start, uint64_t end) {
  int udpSock,tcpSock;
  bool myRefreshStarted = false;

  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  EKA_LOG("\n-----------------------------------------------\n%s:%u %s started",
	  EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op));
  establishConnections(gr,op,start,end,&udpSock,&tcpSock);
  
  while (1) {
    char buf[2000] = {};
    int rc = recvfrom(udpSock, buf, sizeof(buf), MSG_WAITALL, NULL, NULL);
    if (rc < 0)
      on_error("Failed UDP recv (rc = %d)",rc);
    if (rc == 0) {
      EKA_WARN("No UDP data (rc = %d) %s",rc,strerror(errno));
      continue; // to next recvfrom trial
    }

    auto p {reinterpret_cast<const uint8_t*>(buf)};

    if (op ==  EkaFhMode::RECOVERY) {
      if (processRetransUdpPkt(pEfhRunCtx,gr,p,op,start,end))
	break; // while(1)
    } else {
      if (processRefreshUdpPkt(pEfhRunCtx,gr,p,op,&myRefreshStarted))
	break; // while(1)
    }
  } // while(1)
  close(udpSock);
  close(tcpSock);
  EKA_LOG("%s:%u %s completed\n-----------------------------------------------\n",
	  EKA_EXCH_DECODE(gr->exch),gr->id,EkaFhMode2STR(op));
  return true;
}

void* runPlrRecoveryThread(void* attr) {
#ifdef FH_LAB
  pthread_detach(pthread_self());
  return NULL;
#endif
  auto params {reinterpret_cast<EkaFhThreadAttr*>(attr)};
  auto gr     {reinterpret_cast<EkaFhPlrGr*>(params->gr)};
  if (gr == NULL) on_error("gr == NULL");

  EfhRunCtx*   pEfhRunCtx = params->pEfhRunCtx;
  EkaFhMode    op         = params->op;
  uint32_t     start      = static_cast<decltype(start)>(params->startSeq);
  uint32_t     end        = static_cast<decltype(end)>  (params->endSeq);
  delete params;

  if (op != EkaFhMode::DEFINITIONS) pthread_detach(pthread_self());

  EkaDev* dev = gr->dev;

  EKA_LOG("%s:%u: Start of PlrRecoveryThread %s",
	  EKA_EXCH_DECODE(gr->exch),gr->id, 
	  EkaFhMode2STR(op));

  if (plrRecovery(pEfhRunCtx, gr, op, start, end)) {
    EKA_LOG("%s:%u: End Of PlrRecoveryThread: seq_after_snapshot = %ju",
	    EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
    gr->gapClosed = true;
  }
   
  return NULL;
}


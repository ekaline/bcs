#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>

#include "EkaFhPlrGr.h"
#include "EkaFhPlrParser.h"
#include "EkaFhThreadAttr.h"

int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);
int ekaTcpConnect(uint32_t ip, uint16_t port);

using namespace Plr;

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
  memcpy(msg->SourceID,gr->sourceId,
	 std::min(sizeof(msg->SourceID),sizeof(gr->sourceId)));
  msg->ProductID = NYSE_ARCA_BBO_ProductId;
  msg->ChannelID = gr->channelId;
  msg->RetransmitMethod = 0;

  EKA_LOG("Sending SymbolIndexMappingRequest: SymbolIndex=%u,SourceID=\'%s\',ProductID=%u,ChannelID=%u",
	  msg->SymbolIndex,msg->SourceID,msg->ProductID,msg->ChannelID);
  int rc = send(sock,pkt,pktHdr->pktSize,0);
  if (rc <= 0) {
    EKA_WARN("Tcp send of msg size %d to sock %d returned rc = %d",
	     pktHdr->pktSize,sock,rc);
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
  memcpy(msg->SourceID,gr->sourceId,
	 std::min(sizeof(msg->SourceID),sizeof(gr->sourceId)));
  msg->ProductID   = NYSE_ARCA_BBO_ProductId;
  msg->ChannelID   = gr->channelId;

  EKA_LOG("Sending RefreshRequest: SymbolIndex=%u,SourceID=\'%s\',ProductID=%u,ChannelID=%u",
	  msg->SymbolIndex,msg->SourceID,msg->ProductID,msg->ChannelID);
  int rc = send(sock,pkt,pktHdr->pktSize,0);
  if (rc <= 0) {
    EKA_WARN("Tcp send of msg size %d to sock %d returned rc = %d",
	     pktHdr->pktSize,sock,rc);
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
  memcpy(msg->SourceID,gr->sourceId,
	 std::min(sizeof(msg->SourceID),sizeof(gr->sourceId)));
  msg->ProductID   = NYSE_ARCA_BBO_ProductId;
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
  memcpy(msg->SourceID,gr->sourceId,
	 std::min(sizeof(msg->SourceID),sizeof(gr->sourceId)));

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
  if (! getRequestResponse(gr,sock,EkaFhMode::DEFINITIONS))
    on_error("getRequestResponse failed");
  
  return true;
}

static bool recoveryRetransmitTcp(EkaFhPlrGr* gr, int sock, uint32_t start, uint32_t end) {
  if (!gr) on_error("gr == NULL");
//  auto dev {gr->dev};
  if (! sendRetransmissionRequest(gr,sock,start,end))
    on_error("RetransmissionRequest failed");
  if (! getRequestResponse(gr,sock,EkaFhMode::DEFINITIONS))
    on_error("getRequestResponse failed");
  
  return true;
}

void* getPlrRecovery(const EfhRunCtx* pEfhRunCtx, EkaFhPlrGr* gr, EkaFhMode op, uint64_t start, uint64_t end) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  int rc = 0;

  char buf[2000] = {};
  uint32_t udpIp   = 0;
  uint16_t udpPort = 0;
  uint32_t tcpIp   = gr->refreshTcpIp;
  uint16_t tcpPort = gr->refreshTcpPort;
  
  switch (op) {
  case EkaFhMode::DEFINITIONS :
  case EkaFhMode::SNAPSHOT    :
    udpIp   = gr->refreshUdpIp;
    udpPort = gr->refreshUdpPort;
    break;
  case EkaFhMode::RECOVERY    :
    udpIp   = gr->retransUdpIp;
    udpPort = gr->retransUdpPort;
    break;
  default:
    on_error("Unexpected recovery op %d",(int)op);
  }
  int udpSock = ekaUdpMcConnect(dev,udpIp,udpPort);
  if (udpSock < 0)
    on_error("%s:%u: failed to open UDP socket %s:%u",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     EKA_IP2STR(udpIp),udpPort);
  
  EKA_LOG("Waiting for UDP RX pkt before sending request");
  rc = recvfrom(udpSock, buf, sizeof(buf), MSG_WAITALL, NULL, NULL);
  if (rc <= 0)
    on_error("Failed to get UDP pkt (rc = %d) from %s:%u",
	     rc,EKA_IP2STR(udpIp),udpPort);
  
  EKA_LOG("%s:%u: Connecting to TCP server %s:%u",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  EKA_IP2STR(tcpIp),tcpPort);
  
  int tcpSock = ekaTcpConnect(tcpIp,tcpPort);
  if (tcpSock < 0)
    on_error("%s:%u: failed to open TCP socket %s:%u",
	     EKA_EXCH_DECODE(gr->exch),gr->id,
	     EKA_IP2STR(tcpIp),tcpPort);

  switch (op) {
  case EkaFhMode::DEFINITIONS :
    if (! definitionsRefreshTcp(gr,tcpSock))
      on_error("Definitions Failed");
    break;
  case EkaFhMode::SNAPSHOT    :
    if (! snapshotRefreshTcp(gr,tcpSock))
      on_error("Snapshot Failed");
    break;
  case EkaFhMode::RECOVERY    :
    if (! recoveryRetransmitTcp(gr,tcpSock,start,end))
      on_error("Recovery Failed");
    break;
  default:
    on_error("Unexpected recovery op %d",(int)op);
  }

  while (1) {
    rc = recvfrom(udpSock, buf, sizeof(buf), MSG_WAITALL, NULL, NULL);
    if (rc <= 0)
      on_error("Failed to get UDP pkt (rc = %d) from %s:%u",
	       rc,EKA_IP2STR(udpIp),udpPort);
    auto p {buf};
    auto pktHdr {reinterpret_cast<const PktHdr*>(p)};
    p += sizeof(*pktHdr);
    for (auto i = 0; i < pktHdr->numMsgs; i++) {
      auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};
      printf ("\t%s\n",msgType2str(msgHdr->type).c_str());
      p += msgHdr->size;
    }
  }
  return NULL;
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

  EkaDev* dev = gr->dev;

  EKA_LOG("%s:%u: PlrRecoveryThread %s at %s:%d",
	  EKA_EXCH_DECODE(gr->exch),gr->id, 
	  EkaFhMode2STR(op),
	  EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));

  getPlrRecovery(pEfhRunCtx, gr, op, start, end);

  return NULL;
}


#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>

#include "EkaFhPlrGr.h"
#include "EkaFhPlrParser.h"

int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);
int ekaTcpConnect(uint32_t ip, uint16_t port);

using namespace Plr;

static bool sendSymbolIndexMappingRequest(EkaFhPlrGr* gr, int sock) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  SymbolIndexMappingRequest msg{};

  msg.hdr.size = sizeof(msg);
  msg.hdr.type = static_cast<decltype(msg.hdr.type)>(MsgType::SymbolIndexMappingRequest);
  msg.SymbolIndex = 0;
  memcpy(msg.SourceID,gr->sourceId,
	 std::min(sizeof(msg.SourceID),sizeof(gr->sourceId)));
  msg.ProductID = 0;
  msg.ChannelID = gr->channelId;
  msg.RetransmitMethod = 0;

  EKA_LOG("Sending SymbolIndexMappingRequest: SymbolIndex=%u,SourceID=\'%s\',ProductID=%u,ChannelID=%u",
	  msg.SymbolIndex,msg.SourceID,msg.ProductID,msg.ChannelID);
  int rc = send(sock,&msg,sizeof(msg),0);
  if (rc <= 0) {
    EKA_WARN("Tcp send of msg size %d to sock %d returned rc = %d",
	     (int)sizeof(msg),sock,rc);
    return false;
  }
  return true;
}

static bool getRefreshResponse(EkaFhPlrGr* gr, int sock, EkaFhMode op) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};
  EKA_LOG("Waiting for %s Refresh Response",EkaFhMode2STR(op));
  char buf[2000] = {};

  static const int TimeOut = 2; // seconds
  struct timeval tv = {.tv_sec = TimeOut}; 
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  int rc = recv(sock,buf,sizeof(buf),MSG_WAITALL);
  if (rc <= 0)
    on_error("connection reset by peer: rc=%d, \'%s\'",rc,strerror(errno));
  auto msg {reinterpret_cast<const RequestResponse*>(buf)};

  if (msg->hdr.type != static_cast<decltype(msg->hdr.type)>(MsgType::RequestResponse))
    on_error("Received MsgType %d (%s) != Expected MsgType %d (%s)",
	     (int)msg->hdr.type,msgType2str(msg->hdr.type).c_str(),
	     (int)MsgType::RequestResponse,msgType2str((uint16_t)MsgType::RequestResponse).c_str());
  switch(msg->Status) {
  case '0' :
    EKA_LOG("Message was accepted");
    break;
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
  return true;
}

static bool definitionsRefreshTcp(EkaFhPlrGr* gr, int sock) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};
  if (! sendSymbolIndexMappingRequest(gr,sock))
    on_error("sendSymbolIndexMappingRequest failed");
  if (! getRefreshResponse(gr,sock,EkaFhMode::DEFINITIONS))
    on_error("getRefreshResponse failed");
  
  return true;
}

void* getPlrRecovery(const EfhRunCtx* pEfhRunCtx, EkaFhPlrGr* gr, EkaFhMode op) {
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

    break;
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
  
  return NULL;
}




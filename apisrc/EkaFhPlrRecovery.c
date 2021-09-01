#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>

#include "EkaFhPlrGr.h"
#include "EkaFhPlrParser.h"

int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);
int ekaTcpConnect(uint32_t ip, uint16_t port);

using namespace Plr;

void* getPlrRecovery(EkaFhPlrGr* gr, EkaFhMode op) {
  if (!gr) on_error("gr == NULL");
  auto dev {gr->dev};

  int rc = 0;

  char buf[2000] = {};
  uint32_t udpIp   = 0;
  uint16_t udpPort = 0;
  uint32_t tcpIp   = 0;
  uint16_t tcpPort = 0;
  
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
  
  
  return NULL;
}




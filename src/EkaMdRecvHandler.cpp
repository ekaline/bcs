#include "EkaUdpChannel.h"
#include "EkaDev.h"
#include "EkaIgmp.h"

#include "EkaSnDev.h"
#include "EkaCore.h"
#include "EkaEpmRegion.h"
#include "EkaMdRecvHandler.h"

extern EkaDev *g_ekaDev;

using namespace EkaBcs;

/* ==================================================== */

EkaMdRecvHandler::EkaMdRecvHandler(
    int lane, const UdpMcParams *mcParams,
    MdProcessCallback cb, void *ctx) {

  if (!mcParams)
    on_error("!mcParams");

  if (!cb)
    on_error("!cb");

  if (mcParams->nMcGroups > MaxUdpMcGroups_)
    on_error("nMcGroups %ju > "
             "MaxUdpMcGroups %ju",
             mcParams->nMcGroups, MaxUdpMcGroups_);
  cb_ = cb;
  ctx_ = ctx;

  lane_ = lane;

  udpChannel_ = new EkaUdpChannel(
      g_ekaDev, g_ekaDev->snDev->dev_id, lane_, -1);

  if (!udpChannel_)
    on_error("Failed creating udpChannel");

  for (auto i = 0; i < mcParams->nMcGroups; i++) {
    if (lane_ != mcParams->groups[i].lane)
      on_error("MC lane %d != Handler's lane %d",
               mcParams->groups[i].lane, lane_);
    auto mcIp = inet_addr(mcParams->groups[i].mcIp);
    auto mcUdpPort = mcParams->groups[i].mcUdpPort;

    EKA_LOG("Subscribing UdpChannel[%d]: "
            "Lane %d %s:%u",
            udpChannel_->chId, lane_, EKA_IP2STR(mcIp),
            mcUdpPort);

    udpChannel_->igmp_mc_join(mcIp, mcUdpPort, 0);

    auto srcIp = g_ekaDev->core[lane_]->srcIp;

    auto ssmIP = 0;
    switch (lane_) {
    case 0:
      ssmIP = inet_addr("91.203.253.246");
      break;
    case 2:
      ssmIP = inet_addr("91.203.255.246");
      break;
    default:
      on_error("Unexpected lane %d for IGMP", lane_);
    }

    lnxIgmpJoin(srcIp, mcIp, mcUdpPort, ssmIP);

#if 0
    g_ekaDev->ekaIgmp->mcJoin(EkaEpmRegion::Regions::EfcMc,
                              lane_, mcIp, mcUdpPort,
                              0,     // VLAN
                              NULL); // pPktCnt
#endif
  }
}
/* ==================================================== */
void EkaMdRecvHandler::run() {
  EKA_LOG("\n"
          "------------------------------\n"
          "------ Waiting for MD --------\n"
          "------------------------------");
  while (active_) {
    if (!udpChannel_->has_data())
      continue;

    auto pkt = udpChannel_->get();
    if (!pkt)
      on_error("!pkt");

    auto payloadSize = udpChannel_->getPayloadLen();

    cb_(pkt, payloadSize, ctx_);
    udpChannel_->next();
  }
}
/* ==================================================== */
void EkaMdRecvHandler::stop() { active_ = false; }

/* ==================================================== */

void EkaMdRecvHandler::lnxIgmpJoin(uint32_t srcIp,
                                   uint32_t mcIp,
                                   uint16_t mcPort,
                                   uint32_t ssmIP) {
  if ((lnxUdpSock_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    on_error("cannot open UDP socket");

  int const_one = 1;
  if (setsockopt(lnxUdpSock_, SOL_SOCKET, SO_REUSEADDR,
                 &const_one, sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(lnxUdpSock_, SOL_SOCKET, SO_REUSEPORT,
                 &const_one, sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEPORT) failed");

  struct sockaddr_in local2bind = {};
  local2bind.sin_family = AF_INET;
  local2bind.sin_addr.s_addr = INADDR_ANY;
  local2bind.sin_port = be16toh(mcPort);

  if (bind(lnxUdpSock_, (struct sockaddr *)&local2bind,
           sizeof(struct sockaddr)) < 0)
    on_error("cannot bind UDP socket to port %d",
             be16toh(local2bind.sin_port));

  if (ssmIP) {
    struct ip_mreq_source mreq = {};
    mreq.imr_interface.s_addr = srcIp;
    mreq.imr_sourceaddr.s_addr = ssmIP;
    mreq.imr_multiaddr.s_addr = mcIp;
    TEST_LOG(
        "joining IGMP %s from local addr %s using SSM %s",
        EKA_IP2STR(mreq.imr_multiaddr.s_addr),
        EKA_IP2STR(mreq.imr_interface.s_addr),
        EKA_IP2STR(mreq.imr_sourceaddr.s_addr));

    if (setsockopt(lnxUdpSock_, IPPROTO_IP,
                   IP_ADD_SOURCE_MEMBERSHIP, &mreq,
                   sizeof(mreq)) < 0)
      on_error("fail to join IGMP  %s:%d\n",
               inet_ntoa(mreq.imr_multiaddr),
               be16toh(local2bind.sin_port));
  } else {
    struct ip_mreq mreq = {};
    mreq.imr_interface.s_addr = srcIp;
    mreq.imr_multiaddr.s_addr = mcIp;
    TEST_LOG("joining IGMP %s from local addr %s ",
             EKA_IP2STR(mreq.imr_multiaddr.s_addr),
             EKA_IP2STR(mreq.imr_interface.s_addr));

    if (setsockopt(lnxUdpSock_, IPPROTO_IP,
                   IP_ADD_MEMBERSHIP, &mreq,
                   sizeof(mreq)) < 0)
      on_error("fail to join IGMP  %s:%d\n",
               inet_ntoa(mreq.imr_multiaddr),
               be16toh(local2bind.sin_port));
  }
}

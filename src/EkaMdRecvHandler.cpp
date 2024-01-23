#include "EkaUdpChannel.h"
#include "EkaDev.h"
#include "EkaIgmp.h"

#include "EkaSnDev.h"
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

    g_ekaDev->ekaIgmp->mcJoin(EkaEpmRegion::Regions::EfcMc,
                              lane_, mcIp, mcUdpPort,
                              0,     // VLAN
                              NULL); // pPktCnt
  }
}
/* ==================================================== */
void EkaMdRecvHandler::run() {
  while (active_) {
    if (!udpChannel_->has_data())
      continue;

    auto pkt = udpChannel_->get();
    if (!pkt)
      on_error("!pkt");

    auto payloadSize = udpChannel_->getPayloadLen();

    cb_(pkt, payloadSize, ctx_);
  }
}
/* ==================================================== */
void EkaMdRecvHandler::stop() { active_ = false; }

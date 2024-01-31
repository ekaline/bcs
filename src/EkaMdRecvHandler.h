#ifndef __EKA_MD_RECV_HANDLER_H__
#define __EKA_MD_RECV_HANDLER_H__

using namespace EkaBcs;

class EkaUdpChannel;

class EkaMdRecvHandler {
public:
  EkaMdRecvHandler(int lane, const UdpMcParams *mcParams,
                   MdProcessCallback cb, void *ctx);

  ~EkaMdRecvHandler(){};

  void run();

  void stop();

  void lnxIgmpJoin(uint32_t srcIp, uint32_t mcIp,
                   uint16_t mcPort, uint32_t ssmIP = 0);

private:
  static const size_t MaxUdpMcGroups_ = 64;
  EkaUdpChannel *udpChannel_ = NULL;

  MdProcessCallback cb_ = NULL;
  void *ctx_ = NULL;

  EkaBcLane lane_ = -1;

  volatile bool active_ = true;

  int lnxUdpSock_ = -1;
};
#endif

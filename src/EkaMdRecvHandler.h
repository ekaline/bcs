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

private:
  static const size_t MaxUdpMcGroups_ = 64;
  EkaUdpChannel *udpChannel_ = NULL;

  MdProcessCallback cb_ = NULL;
  void *ctx_ = NULL;

  EkaBcLane lane_ = -1;

  volatile bool active_ = true;
};
#endif

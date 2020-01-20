#ifndef _EKA_USER_CHANNEL_H
#define _EKA_USER_CHANNEL_H
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>

#include "smartnic.h"
//#include "Exc.h"

class EkaDev;

#define USR_CH_DECODE(x) \
  x == eka_user_channel::TYPE::FIRE_REPORT ? "FIRE_REPORT" : \
  x == eka_user_channel::TYPE::FAST_PATH   ? "FAST_PATH"   : \
  x == eka_user_channel::TYPE::TCP         ? "TCP"         : \
                                             "UNKNOWN"

class eka_user_channel {
 public:
  enum class TYPE : uint8_t { FIRE_REPORT = 0, FAST_PATH = 1, TCP = 2 } ;

  TYPE type;

  eka_user_channel(EkaDev* pEkaDev,SN_DeviceId devId, TYPE t);
  ~eka_user_channel();

  bool              has_data();
  const uint8_t*    get();
  uint              get_payload_size();
  void              next();

  private:
  EkaDev*           dev;
  SN_DeviceId       dev_id;

  uint              ptr_update_ctr;
  uint64_t          packetBytesTotal;
  SC_ChannelId      ChannelId;

  const uint8_t*    pkt_payload_ptr;

  const SC_Packet * pIncomingUdpPacket;
  const SC_Packet * pPreviousUdpPacket;
};



#endif

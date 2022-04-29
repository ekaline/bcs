#pragma once

#include <arpa/inet.h>
#include <assert.h>
#include <compat/Eka.h>
#include <compat/Epm.h>
#include <string.h>

#include "common.h"

namespace gts::ekaline {

class Trigger {
 public:
  Trigger(const EpmTriggerParams &epmTrigger)
      : Trigger(epmTrigger.coreId, epmTrigger.mcIp, epmTrigger.mcUdpPort) {}

  Trigger(int portIdx, const char *inaddr, u16 udpPort)
      : portIdx_{(s8) portIdx}, multicastIP_{inaddr}, multicastPort_{udpPort} {}

  bool isValid() const {
    struct in_addr inaddr;
    return portIdx_ >= 0 && portIdx_ < 4 &&
           multicastPort_ >= 1024 &&
           inet_aton(multicastIP_.c_str(), &inaddr) != 0;
  }

  EpmTriggerParams build() const {
    return makeCopy(peek());
  }

  static EpmTriggerParams makeCopy(const EpmTriggerParams &epmParam) {
    EpmTriggerParams copy{epmParam};
    copy.mcIp = strdup(copy.mcIp);
    return copy;
  }

 private:
  EpmTriggerParams peek() const {
    assert(isValid());
    return EpmTriggerParams{(s8) portIdx_, multicastIP_.data(), multicastPort_};
  }

  s8 portIdx_;
  std::string multicastIP_;
  u16 multicastPort_;
};

}  // namespace gts::ekaline
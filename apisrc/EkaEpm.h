#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <iterator>
#include <thread>

#include <sys/time.h>
#include <chrono>

#include "eka_macros.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "eka_dev.h"

class EpmCore;

class EkaEpm {
 public:
  uint64_t getPayloadMemorySize () {
    return PayloadMemorySize;
  }

  uint64_t getPayloadAlignment () {
    return PayloadAlignment;
  }

  uint64_t getDatagramOffset () {
    return DatagramOffset;
  }

  uint64_t getRequiredTailPadding () {
    return RequiredTailPadding;
  }

  uint64_t getMaxStrategies () {
    return MaxStrategies;
  }

  uint64_t getMaxActions () {
    return MaxActions;
  }


 private:
  static const uint64_t PayloadMemorySize = 4096;
  static const uint64_t PayloadAlignment = 8;
  static const uint64_t DatagramOffset = 54; // 14+20+20
  static const uint64_t RequiredTailPadding = 4;
  static const uint64_t MaxStrategies = 32;
  static const uint64_t MaxActions = 1024;

  EkaOpResult epmSetAction(EkaCoreId coreId,
			   epm_strategyid_t strategy, epm_actionid_t action,
			   const EpmAction *epmAction) {

  }

  EpmCore* core[EkaDev::CONF::MAX_CORES];


};


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

#include "epmCaps.h"
#include "EkaEpm.h"

uint64_t epmGetDeviceCapability(const EkaDev *dev, EpmDeviceCapability cap) {
  switch (cap) {
    case EHC_PayloadMemorySize :
      return dev->epm->getPayloadMemorySize();

    case EHC_PayloadAlignment :
      return dev->epm->getPayloadAlignment();

    case EHC_DatagramOffset :
      return dev->epm->getDatagramOffset();

    case EHC_RequiredTailPadding :
      return dev->epm->getRequiredTailPadding();

    case EHC_MaxStrategies :
      return dev->epm->getMaxStrategies();

    case EHC_MaxActions :
      return dev->epm->getMaxActions();

    default:
      on_error("Unsupported EPM Cap: %d",(int)cap);
    }
}

EkaOpResult epmInitStrategies(EkaDev *ekaDev, EkaCoreId coreId,
                              const EpmStrategyParams *params,
                              epm_strategyid_t numStrategies) {

}

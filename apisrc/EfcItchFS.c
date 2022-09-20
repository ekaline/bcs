#include "EkaEfc.h"
#include "EfcItchFS.h"
#include "EkaEpmAction.h"
#include "EkaTcpSess.h"
#include "EkaHwInternalStructs.h"

EkaOpResult efcItchFastSweepInit(EkaDev *dev,
				 const EfcItchFastSweepParams* params) {

  if (! dev) on_error("! dev");
  if (! params) on_error("! params");



  volatile EfcCmeFastCancelStrategyConf conf = {
//      .pad            = {},
      .token          = params->token,
      .minMsgCount    = params->minMsgCount,
      .minUDPSize     = params->minUDPSize,
      .fireActionId   = (uint16_t)EfcItchFSActionId::HwSweep,
      .strategyId     = (uint8_t)EFC_STRATEGY
  };

  EKA_LOG("Configuring ITCH Fast Sweep FPGA: minMsgCount=%d,minUDPSize=%u,token=0x%jx,fireActionId=%d,strategyId=%d",
	  conf.minMsgCount,
	  conf.minUDPSize,
	  conf.token,
	  conf.fireActionId,
	  conf.strategyId);
  //  hexDump("EfcCmeFastCancelStrategyConf",&conf,sizeof(conf),stderr);
  copyBuf2Hw(dev,0x84000,(uint64_t *)&conf,sizeof(conf));
  return EKA_OPRESULT__OK;
}


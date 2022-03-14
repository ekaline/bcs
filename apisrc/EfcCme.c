#include "EkaEfc.h"
#include "EfcCme.h"
#include "EkaEpmAction.h"

EkaOpResult efcCmeFastCancelInit(EkaDev *dev,
				 const EfcCmeFastCancelParams* params) {

  if (! dev) on_error("! dev");
  if (! params) on_error("! params");

/* typedef struct packed { */
/*         bit [7:0] strategy_region;   */
/*         bit [15:0] strategy_index;   */
/*         bit [63:0] token;   */
/* 	bit [15:0] max_header_size; */
/* 	bit [7:0] min_num_in_group; */
/* } sh_cancels_param_t; */

  struct EfcCmeFastCancelStrategyConf {
      uint8_t        minNoMDEntries;
      uint16_t       maxMsgSize;
      uint64_t       token;
      uint16_t       fireActionId;
      uint8_t        strategyId;
      uint8_t        pad[2];      
  } __attribute__((packed));

  const EfcCmeFastCancelStrategyConf conf = {
      params->minNoMDEntries,
      params->maxMsgSize,
      params->token,
      (uint16_t)EfcCmeActionId::HwCancel,
      EFC_STRATEGY
  };

  copyHw2Buf(dev,(uint64_t *)&conf,0x83000,sizeof(conf));
  
  return EKA_OPRESULT__OK;
}

ssize_t efcCmeSend(EkaDev* dev, ExcConnHandle hConn,
		   const void* buffer, size_t size, int tcpFlags,
		   bool incrAppSequence) {
    if (! dev) on_error("! dev");
    auto epm = dev->epm;
    if (! epm) on_error("! epm");
    
    auto efc = epm->strategy[EFC_STRATEGY];
    if (! efc) on_error("! efc");

    auto ekaA = incrAppSequence ? efc->action[(size_t)EfcCmeActionId::SwFire] :
	efc->action[(size_t)EfcCmeActionId::Heartbeat];

    if (ekaA->coreId != excGetCoreId(hConn) ||
	ekaA->sessId != excGetSessionId(hConn)) {
	EKA_WARN("Cme Fast Cancel action is not set");
	return -1;
    }
    return ekaA->fastSend(buffer, size);
}

#include "EkaEfc.h"
#include "EfcCme.h"
#include "EkaEpmAction.h"
#include "EkaTcpSess.h"
#include "EkaHwInternalStructs.h"

int getSessAppSeqId(EkaDev* dev, ExcConnHandle hConn);

EkaOpResult efcCmeFastCancelInit(EkaDev *dev,
				 const EfcCmeFastCancelParams* params) {

  if (! dev) on_error("! dev");
  if (! params) on_error("! params");



  volatile EfcCmeFastCancelStrategyConf conf = {
//      .pad            = {},
      .minNoMDEntries = params->minNoMDEntries,
      .maxMsgSize     = params->maxMsgSize,
      .token          = params->token,
      .fireActionId   = (uint16_t)params->fireActionId, 
      .strategyId     = (uint8_t)EFC_STRATEGY
  };

  EKA_LOG("Configuring Cme Fast Cancel FPGA: "
					"minNoMDEntries=%d,maxMsgSize=%u,"
					"token=0x%jx,fireActionId=%d,strategyId=%d",
	  conf.minNoMDEntries,
	  conf.maxMsgSize,
	  conf.token,
	  conf.fireActionId,
	  conf.strategyId);
  //  hexDump("EfcCmeFastCancelStrategyConf",&conf,
	//          sizeof(conf),stderr);
  copyBuf2Hw(dev,0x84000,(uint64_t *)&conf,sizeof(conf));
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

    auto ekaA = incrAppSequence ?
			efc->action[(size_t)EfcCmeActionId::SwFire] :
			efc->action[(size_t)EfcCmeActionId::Heartbeat];

    if (ekaA->coreId != excGetCoreId(hConn) ||
				ekaA->sessId != excGetSessionId(hConn)) {
			EKA_WARN("Cme Fast Cancel action is not set");
			return -1;
    }
    return ekaA->fastSend(buffer, size);
}


EkaOpResult efcCmeSetILinkAppseq(EkaDev *dev,
				 ExcConnHandle hConn,
				 int32_t appSequence) {
  auto coreId = excGetCoreId(hConn);
  auto sessId = excGetSessionId(hConn);
  if (!dev->core[coreId] || !dev->core[coreId]->tcpSess[sessId])
    on_error("hConn 0x%x does not exist",hConn);


  struct SessAppCtx {
    char asciiSeq[8] = {};
    uint64_t binSeq = 0;
  };

  
  
  auto s = dev->core[coreId]->tcpSess[sessId];

  s->updateFpgaCtx<EkaTcpSess::AppSeqBin>((uint64_t)appSequence);

  return EKA_OPRESULT__OK;
}
  

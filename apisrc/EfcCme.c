#include <iostream>

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
      .fireActionId   = (uint16_t)EfcCmeActionId::HwCancel,
      .strategyId     = (uint8_t)EFC_STRATEGY
  };

  EKA_LOG("Configuring Cme Fast Cancel FPGA: minNoMDEntries=%d,maxMsgSize=%u,token=0x%jx,fireActionId=%d,strategyId=%d",
	  conf.minNoMDEntries,
	  conf.maxMsgSize,
	  conf.token,
	  conf.fireActionId,
	  conf.strategyId);
  //  hexDump("EfcCmeFastCancelStrategyConf",&conf,sizeof(conf),stderr);
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

    auto ekaA = incrAppSequence ? efc->action[(size_t)EfcCmeActionId::SwFire] :
	efc->action[(size_t)EfcCmeActionId::Heartbeat];

    if (ekaA->coreId != excGetCoreId(hConn) ||
	ekaA->sessId != excGetSessionId(hConn)) {
	EKA_WARN("Cme Fast Cancel action is not set");
	return -1;
    }
  auto t1 = std::chrono::high_resolution_clock::now();
  ssize_t res = ekaA->fastSend(buffer, size);
  auto t2 = std::chrono::high_resolution_clock::now();

  /* Getting number of milliseconds as a double. */
  std::chrono::duration<double, std::nano> nanos_double = t2 - t1;
  printf("ekaA->fastSend = [%f]\n", nanos_double.count());
    return res;
}


EkaOpResult efcCmeSetILinkAppseq(EkaDev *dev,
				 ExcConnHandle hCon,
				 int32_t appSequence) {
  auto sessAppSeqId = getSessAppSeqId(dev,hCon);  
  dev->eka_write(0x86000 + sessAppSeqId * 8,appSequence);
  
  return EKA_OPRESULT__OK;
}
  

#include "EfcQED.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaHwInternalStructs.h"
#include "EkaTcpSess.h"
#if 0
EkaOpResult efcQEDInit(EkaDev *dev,
		       const EfcQEDParams* params) {

  if (! dev) on_error("! dev");
  if (! params) on_error("! params");


  volatile EfcQEDStrategyConf conf = {};

  for (auto i = 0; i < 4; i++) {
    conf.product[i].enable       = params->product[i].enable;
    conf.product[i].minNumLevel  = params->product[i].min_num_level;
    conf.product[i].dsID         = params->product[i].ds_id;
    conf.product[i].token        = params->product[i].token;
    conf.product[i].fireActionId = params->product[i].fireActionId;
    conf.product[i].strategyId   = (uint8_t)EFC_STRATEGY;
  }
  
  for (auto i = 0; i < 4; i++) {
    EKA_LOG("Configuring QED FPGA: product=%d, enable=%d, min_num_level=%d,ds_id=0x%x,token=0x%jx,fireActionId=%d,strategyId=%d",
	    i,
	    conf.product[i].enable,
	    conf.product[i].minNumLevel,
	    conf.product[i].dsID,
	    conf.product[i].token,
	    conf.product[i].fireActionId,
	    conf.product[i].strategyId
	    );
    //  hexDump("EfcQEDStrategyConf",&conf,sizeof(conf),stderr);
  }
  
  copyBuf2Hw(dev,0x86000,(uint64_t *)&conf,sizeof(conf));
  return EKA_OPRESULT__OK;
}

#endif
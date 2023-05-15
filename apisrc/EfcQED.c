#include "EkaEfc.h"
#include "EfcQED.h"
#include "EkaEpmAction.h"
#include "EkaTcpSess.h"
#include "EkaHwInternalStructs.h"

EkaOpResult efcQEDInit(EkaDev *dev,
		       const EfcQEDParams* params) {

  if (! dev) on_error("! dev");
  if (! params) on_error("! params");


  volatile EfcQEDStrategyConf conf = {};
  
  conf.product[0].enable       = params->product[0].enable;
  conf.product[0].minNumLevel  = params->product[0].min_num_level;
  conf.product[0].dsID         = params->product[0].ds_id;
  conf.product[0].token        = params->product[0].token;
  conf.product[0].fireActionId = params->product[0].fireActionId;
  conf.product[0].strategyId   = (uint8_t)EFC_STRATEGY;

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


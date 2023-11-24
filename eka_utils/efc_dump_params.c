#include <inttypes.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "Efc.h"
#include "EfcMsgs.h"
#include "EkaHwCaps.h"
#include "EkaHwExceptionsDecode.h"
#include "EkaMcState.h"
#include "ctls.h"
#include "eka.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"
#include "eka_sn_addr_space.h"
#include "smartnic.h"
#include "EkaEobiTypes.h"

SN_DeviceId DeviceId;
#define ADDR_VERSION_ID       0xf0ff0

#define SW_SCRATCHPAD_BASE 0x84000

#define STR_EN  "yes"
#define STR_DIS "no"
#define STR_IGNORE "ignore"
#define UNDER "\x1B[0;4m"
//#define UNDER ""
#define RESET "\x1B[0m"
#define GRN   "\x1B[32m"


using namespace EkaEobi;

enum  class EKA_CONF_TYPE : int {
  SJUMP = 0,
  MOMENTUM  = 1,
  PRODUCT = 2,
  RJUMP = 3
};

uint64_t reg_read (uint32_t addr)
{
    uint64_t value = -1;
    SN_ReadUserLogicRegister (DeviceId, addr/8, &value);
    return value;
}

int eka_dump_params (EKA_CONF_TYPE param_id, int index) {
  volatile struct HwJumpParams jump_configs = {};
  //  volatile struct hw_rjump_strategy_params_t rjump = {};

  //  volatile struct hw_product_param_entry_t product_params;
  int iter;
  //  char buf[16];
  //  resolve_product(feed_id,buf,prod_id);

  bool header_printed = false;

  switch(param_id) {
  case EKA_CONF_TYPE::SJUMP : {//jump
    iter = sizeof(struct HwJumpParams)/8 + !!(sizeof(struct HwJumpParams)%8); // num of 8Byte words
    for(uint64_t i = 0; i <(uint64_t)iter; i++){
      uint64_t var_config = reg_read(0x50000+i*8+256*index);
      uint64_t* wr_ptr = (uint64_t*) &jump_configs;
      *(wr_ptr + i) = var_config;
      //      printf("config addr 0x%jx = 0x%jx\n",0x90000+i*8+EKA_HW_JUMP_LINE*index,var_config);

    }

    printf("%-8s\t","Product Params");
    printf("%-12s\t","maxBookSpread");
    printf("%-12s\t","bidSize");
    printf("%-12s\t","askSize");
    printf("%-12s\t","actionIdx");
    printf("%-12s\t","prodHandle");
    printf("%-12s\n","secId");
    //    printf(RESET);

    printf(GRN);
    printf("%-12s\t","");
    printf("%-12u\t",jump_configs.prodParams.maxBookSpread);
    printf("%-12u\t",jump_configs.prodParams.bidSize);
    printf("%-12u\t",jump_configs.prodParams.askSize);
    printf("%-12u\t",jump_configs.prodParams.actionIdx);
    printf("%-12u\t",jump_configs.prodParams.prodHandle);
    printf("%-12u\n",jump_configs.prodParams.secId);
    printf(RESET);

    for(uint32_t i = 0; i < EKA_JUMP_ATBEST_SETS; i++){
      if ((jump_configs.atBest[i].bitParams>>BITPARAM_ENABLE)&0x1) {
	if (!header_printed) {
	  header_printed = true;
	  //	  printf(UNDER);
	  //    printf("\n%s %-8s\t",buf,"Jump Best");
	  printf("%-8s\t","Jump Best");
	  printf("%-12s\t","minPriceDelta");
	  printf("%-12s\t","minTickerSize");
	  printf("%-12s\t","maxPostSize");
	  printf("%-12s\t","maxTobSize");
	  printf("%-12s\t","minTobSize");
	  printf("%-12s\t","bidSize");
	  printf("%-12s\t","askSize");
	  printf("%-12s\t","BOC");
	  printf("%-12s\n","Enabled");    
	  //	  printf(RESET);
	}

	printf(GRN);
	printf("%s%-12d\t","Set",i);

	if (jump_configs.atBest[i].minPriceDelta == (uint64_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12ju\t",jump_configs.atBest[i].minPriceDelta);

	if (jump_configs.atBest[i].minTickerSize == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.atBest[i].minTickerSize);

	if (jump_configs.atBest[i].maxPostSize == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.atBest[i].maxPostSize);

	if (jump_configs.atBest[i].maxTobSize == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.atBest[i].maxTobSize);

	if (jump_configs.atBest[i].minTobSize == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.atBest[i].minTobSize);

	if (jump_configs.atBest[i].bidSize == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.atBest[i].bidSize);

	if (jump_configs.atBest[i].askSize == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.atBest[i].askSize);

	printf("%-12s\t",(jump_configs.atBest[i].bitParams>>BITPARAM_BOC)&0x1 ? STR_EN : STR_DIS);
	printf("%-12s\n",(jump_configs.atBest[i].bitParams>>BITPARAM_ENABLE)&0x1 ? STR_EN : STR_DIS);
	printf(RESET);
      }
    }

    header_printed = false;

    for(uint32_t i = 0; i < EKA_JUMP_BETTERBEST_SETS; i++){
      if ((jump_configs.betterBest[i].bitParams>>BITPARAM_ENABLE)&0x1) {
	if (!header_printed) {
	  //	  printf(UNDER);
	  //    printf("\n%s %-8s\t",buf,"Jump Better");
	  printf("%-8s\t","Jump Better");
	  printf("%-12s\t","minPriceDelta");
	  printf("%-12s\t","minTickerSize");
	  printf("%-12s\t","maxPostSize");
	  printf("%-12s\t","maxTobSize");
	  printf("%-12s\t","minTobSize");
	  printf("%-12s\t","bidSize");
	  printf("%-12s\t","askSize");
	  printf("%-12s\t","BOC");
	  printf("%-12s\n","Enabled");    
	  //	  printf(RESET);
	  header_printed = true;
	}

	printf(GRN);
	printf("%s%-12d\t","Set",i);

	if (jump_configs.betterBest[i].minPriceDelta == (uint64_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12ju\t",jump_configs.betterBest[i].minPriceDelta);

	if (jump_configs.betterBest[i].minTickerSize == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.betterBest[i].minTickerSize);

	if (jump_configs.betterBest[i].maxPostSize == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.betterBest[i].maxPostSize);

	if (jump_configs.betterBest[i].maxTobSize == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.betterBest[i].maxTobSize);

	if (jump_configs.betterBest[i].minTobSize == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.betterBest[i].minTobSize);

	if (jump_configs.betterBest[i].bidSize == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.betterBest[i].bidSize);

	if (jump_configs.betterBest[i].askSize == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",jump_configs.betterBest[i].askSize);

	printf("%-12s\t",(jump_configs.betterBest[i].bitParams>>BITPARAM_BOC)&0x1 ? STR_EN : STR_DIS);
	printf("%-12s\n",(jump_configs.betterBest[i].bitParams>>BITPARAM_ENABLE)&0x1 ? STR_EN : STR_DIS);
	printf(RESET);

      }
    }
  }
    break;

#if 0

  case EKA_CONF_TYPE::PRODUCT : //per product
    iter = sizeof(struct hw_product_param_entry_t)/8 + !!(sizeof(struct hw_product_param_entry_t)%8); // num of 8Byte words
    for(uint64_t i = 0; i < (uint64_t)iter; i++){
      uint64_t var_config = reg_read(0x83000+index*iter*8+i*8);
      uint64_t* wr_ptr = (uint64_t*) &product_params;
      *(wr_ptr + i) = var_config;
      //      printf ("%ju 0x%jx\n",i,var_config);
    }

    //    printf(UNDER);
    //    printf("\n%s %-8s\t",buf,"Params");
    printf("%-8s\t","Product Params");
    printf("%-12s\t","max_book_spread");
    printf("%-12s\t","sell_size");
    printf("%-12s\t","buy_size");
    //    printf("%-12s\t","midpoint");
    printf("%-12s\t","step");
    printf("%-12s\t","ei_if");
    printf("%-12s\n","ei_session");
    //    printf(RESET);

    printf(GRN);
    printf("%-12s\t","");
    printf("%-12u\t",product_params.max_book_spread);
    printf("%-12u\t",product_params.sell_size);
    printf("%-12u\t",product_params.buy_size);
    //    printf("%-12u\t",product_params.midpoint);
    printf("%-12u\t",product_params.step);
    printf("%-12u\t",product_params.ei_if);
    printf("%-12u\n",product_params.ei_session);
    printf(RESET);

    break;
#endif
#if 0
  case EKA_CONF_TYPE::RJUMP: 
    iter = sizeof(struct hw_rjump_strategy_params_t)/8 + !!(sizeof(struct hw_rjump_strategy_params_t)%8); // num of 8Byte words
    for(uint64_t i = 0; i < (uint64_t)iter; i++){
      uint64_t var_config = reg_read(0xa0000+i*8+EKA_HW_RJUMP_LINE*index);
      //      printf("config addr 0x%jx = 0x%jx\n",0xa0000+i*8+EKA_HW_RJUMP_LINE*index,var_config);

      uint64_t* wr_ptr = (uint64_t*) &rjump;
      *(wr_ptr + i) = var_config;
    }

    header_printed = false;

    for(uint32_t i = 0; i < EKA_RJUMP_ATBEST_SETS; i++){
      //      if ((rjump.atBest[i].bit_params>>BP_ENABLE)&0x1) {
      if (1) {
	if (!header_printed) {
	  header_printed = true;
	  //	  printf(UNDER);
	  //    printf("\n%s %-8s\t",buf,"RJmp Best");
	  printf("%-8s\t","RJmp Best");
	  printf("%-12s\t","ticker_lots");
	  printf("%-12s\t","time_delta");
	  printf("%-12s\t","maxtob_size");
	  printf("%-12s\t","mintob_size");
	  printf("%-12s\t","maxopp_size");
	  printf("%-12s\t","min_spread");
	  printf("%-12s\t","buy_size");
	  printf("%-12s\t","sell_size");
	  printf("%-12s\t","boc_en");
	  printf("%-12s\n","strat_en");
	  //	  printf(RESET);
	}
	printf(GRN);
	printf("%s%-12d\t","Set",i);

	if (rjump.atBest[i].tickersize_lots == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].tickersize_lots);

	if (rjump.atBest[i].time_delta_ns == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].time_delta_ns);

	if (rjump.atBest[i].max_tob_size == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].max_tob_size);

	if (rjump.atBest[i].min_tob_size == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].min_tob_size);

	if (rjump.atBest[i].max_opposit_tob_size == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].max_opposit_tob_size);

	if (rjump.atBest[i].min_spread == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].min_spread);

	if (rjump.atBest[i].buy_size == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].buy_size);

	if (rjump.atBest[i].sell_size == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.atBest[i].sell_size);

	printf("%-12s\t",(rjump.atBest[i].bit_params>>BP_BOC)&0x1 ? STR_EN : STR_DIS);
	printf("%-12s\n",(rjump.atBest[i].bit_params>>BP_ENABLE)&0x1 ? STR_EN : STR_DIS);
	printf(RESET);
      }
    }

    header_printed = false;



    for(uint32_t i = 0; i < EKA_RJUMP_BETTERBEST_SETS; i++){
      //      if ((rjump.betterBest[i].bit_params>>BP_ENABLE)&0x1) {
      if (1) {
	if (!header_printed) {
	  header_printed = true;
	  //	  printf(UNDER);
	  //    printf("\n%s %-8s\t",buf,"RJmp Better");
	  printf("%-8s\t","RJmp Better");
	  printf("%-12s\t","ticker_lots");
	  printf("%-12s\t","time_delta");
	  printf("%-12s\t","maxtob_size");
	  printf("%-12s\t","mintob_size");
	  printf("%-12s\t","maxopp_size");
	  printf("%-12s\t","min_spread");
	  printf("%-12s\t","buy_size");
	  printf("%-12s\t","sell_size");
	  printf("%-12s\t","boc_en");
	  printf("%-12s\n","strat_en");
	  //	  printf(RESET);
	}
	printf(GRN);
	printf("%s%-12d\t","Set",i);

	if (rjump.betterBest[i].tickersize_lots == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].tickersize_lots);

	if (rjump.betterBest[i].time_delta_ns == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].time_delta_ns);

	if (rjump.betterBest[i].max_tob_size == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].max_tob_size);

	if (rjump.betterBest[i].min_tob_size == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].min_tob_size);

	if (rjump.betterBest[i].max_opposit_tob_size == (uint16_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].max_opposit_tob_size);

	if (rjump.betterBest[i].min_spread == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].min_spread);

	if (rjump.betterBest[i].buy_size == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].buy_size);

	if (rjump.betterBest[i].sell_size == (uint8_t)-1)
	  printf("%-12s\t",STR_IGNORE);
	else
	  printf("%-12u\t",rjump.betterBest[i].sell_size);

	printf("%-12s\t",(rjump.betterBest[i].bit_params>>BP_BOC)&0x1 ? STR_EN : STR_DIS);
	printf("%-12s\n",(rjump.betterBest[i].bit_params>>BP_ENABLE)&0x1 ? STR_EN : STR_DIS);
	printf(RESET);
      }
    }

    break;
#endif

  }

  return 0;
}



int main(int argc, char *argv[]) {
  DeviceId = SN_OpenDevice(NULL, NULL);
  if (DeviceId == NULL) {
    fprintf( stderr, "Cannot open FiberBlaze device. Is driver loaded?\n" );
    exit( -1 );
  }

  //  printf ("Reading configurable parameters from FPGA...\n");
  for (int i=0;i<1;i++) {
    //    eka_product_t prod_id = (eka_product_t)((var_sw_stats_zero>>(8+i*4))&0xf);
    //    if (prod_id==INVALID) continue;
    //    resolve_product(feed_id,buf,prod_id);

    //    scratchpadval = reg_read(SW_SCRATCHPAD_BASE + 8*i);
    //    if (!scratchpadval) continue;
    //    memcpy(buf,(char*)&scratchpadval,8);
    //

    //    eka_dump_params(feed_id,EKA_CONF_TYPE::PRODUCT,i,buf);
    printf(UNDER "\n      Prod %d       \n" RESET,i);
    eka_dump_params(EKA_CONF_TYPE::SJUMP,i);
    //    eka_dump_params(feed_id,EKA_CONF_TYPE::RJUMP,i,buf);
  }


  SN_CloseDevice(DeviceId);
}
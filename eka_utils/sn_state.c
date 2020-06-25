#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

#include "smartnic.h"

#define NUM_OF_CORES 16
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"
#define FREQUENCY 161

#define ADDR_RT_COUNTER          0x20000
#define ADDR_INTERRUPT_SHADOW_RO 0xf0790

// global configurations
#define ADDR_VERSION_ID       0xf0ff0
#define ADDR_VERSION_HIGH_ID  0xf0ff8
#define ADDR_NW_GENERAL_CONF  0xf0020
#define ADDR_STATS_SECCTX     0xf0290
#define ADDR_GENERAL_CONF     0xf0520
#define ADDR_STATS_SECCTX_TOT 0xf0298
#define ADDR_SW_STATS_ZERO    0xf0770
#define ADDR_P4_GENERAL_CONF  0xf07c0
#define ADDR_P4_FIRE_MDCNT    0xf07f0
#define ADDR_FATAL_CONF       0xf0f00

#define ADDR_DIRECT_TCP_DESC_SENT    0xf0208
#define ADDR_DIRECT_TCP_SENT_BYTES   0xf0210

// per core base
#define ADDR_P4_DEBUG0                 0xe0200
#define ADDR_P4_CONT_COUNTER1          0xe0210
#define ADDR_P4_CONT_COUNTER2          0xe0218
#define ADDR_P4_CONT_COUNTER3          0xe0220

#define ADDR_STATS_RX_PPS            0xe0008 //  Statistics: packet per second
#define ADDR_STATS_RX_BPS_MAX        0xe0010 //  Statistics: bytes per second max
#define ADDR_STATS_RX_BPS_CURR       0xe0018 //  Statistics: bytes per second current
#define ADDR_STATS_RX_BPS_TOT        0xe0020 //  Statistics: total bytes
#define ADDR_STATS_RX_PPS_TOT        0xe0028 //  Statistics: total packets

/* #define ADDR_STATS_TX_PPS            0xe0040 //  Statistics: packet per second */
/* #define ADDR_STATS_TX_BPS_MAX        0xe0048 //  Statistics: bytes per second max */
/* #define ADDR_STATS_TX_BPS_CURR       0xe0050 //  Statistics: bytes per second current */
/* #define ADDR_STATS_TX_BPS_TOT        0xe0058 //  Statistics: total bytes */
/* #define ADDR_STATS_TX_PPS_TOT        0xe0060 //  Statistics: total packets */

#define ADDR_SOP_COUNTER             0xe0100 //  SOP Counter
#define ADDR_ORDER_COUNTER           0xe0108 //  ORDER Counter
#define ADDR_OVERRUN_TRADE_COUNTER   0xe0110 //  4B overrun 4B trade counter

#define ADDR_SEQ_EXC_ACTUAL          0xe0350 //  Seq follower actual
#define ADDR_SEQ_EXC_EXPECTED        0xe0358 //  Seq follower expected
#define ADDR_SEQ_EXC_PARAMS          0xe0360 //  Seq follower group,32'dcounter

#define MASK64 0xffffffffffffffff
#define MASK32 0xffffffff
#define MASK8  0xff
#define MASK4  0xf
#define MASK1  0x1

SN_DeviceId DeviceId;

uint64_t reg_read (uint32_t addr)
{
    uint64_t value = -1;

    SN_ReadUserLogicRegister (DeviceId, addr/8, &value);

    return value;

}

int main(int argc, char *argv[]){
    DeviceId = SN_OpenDevice(NULL, NULL);
    if (DeviceId == NULL) {
        fprintf( stderr, "Cannot open FiberBlaze device. Is driver loaded?\n" );
        exit( -1 );
    }

    int sleep_seconds;
    int stop_loop = 0;
    char *endptr;

    if ( argc == 2 )
      {
	sleep_seconds = strtod(argv[1], &endptr);
	if (1 > sleep_seconds)
	  {
	    stop_loop = 1;
	  }
      }
    else
      {
	sleep_seconds = 1;
      }

    
    int curr_core;
    const char* format="%'-16ju\t";
    
    uint64_t total_orders;
    uint64_t total_fires;
    float subscribed_percentage;
    float fire_percentage;
    
    // per core variable definition
    uint64_t var_stats_rx_pps[NUM_OF_CORES]      = {};
    /* uint64_t var_stats_rx_bps_curr[NUM_OF_CORES] = {}; */
    /* uint64_t var_stats_rx_bps_max[NUM_OF_CORES]  = {}; */
    /* uint64_t var_stats_rx_bps_tot[NUM_OF_CORES]  = {}; */
    uint64_t var_stats_rx_pps_tot[NUM_OF_CORES]  = {};
    
    /* uint64_t var_stats_tx_pps[NUM_OF_CORES]      = {}; */
    /* uint64_t var_stats_tx_bps_curr[NUM_OF_CORES] = {}; */
    /* uint64_t var_stats_tx_bps_max[NUM_OF_CORES]  = {}; */
    /* uint64_t var_stats_tx_bps_tot[NUM_OF_CORES]  = {}; */
    /* uint64_t var_stats_tx_pps_tot[NUM_OF_CORES]  = {}; */
    
    uint64_t var_sop_counter[NUM_OF_CORES]           = {};
    uint64_t var_order_counter[NUM_OF_CORES]         = {};
    uint64_t var_overrun_trade_counter[NUM_OF_CORES] = {};
    
    uint64_t var_p4_cont_debug0[NUM_OF_CORES]           = {};
    uint64_t var_p4_cont_counter1[NUM_OF_CORES]         = {};
    uint64_t var_p4_cont_counter2[NUM_OF_CORES]         = {};
    uint64_t var_p4_cont_counter3[NUM_OF_CORES]         = {};
    
    uint64_t var_seq_exc_params[NUM_OF_CORES]         = {};
    uint64_t var_seq_exc_expected[NUM_OF_CORES]       = {};
    uint64_t var_seq_exc_actual[NUM_OF_CORES]         = {};

    uint64_t var_version_id_high = reg_read(ADDR_VERSION_HIGH_ID);
    uint8_t number_of_cores = ((var_version_id_high>>56)&MASK8);
    uint64_t var_version_id	 = reg_read(ADDR_VERSION_ID);
    int feed_id        = ((var_version_id>>56)&MASK4);
    char feed_string[256];
    switch(feed_id) {
    case 0 :
      strcpy(feed_string, "NOM\t\t\t"); 
      break;
    case 1 :
      strcpy(feed_string, "MIAX\t\t\t"); 
      break;
    case 2 :
      strcpy(feed_string, "PHLX\t\t\t"); 
      break;
    case 3 :
      strcpy(feed_string, "ISE/GEMX/MRX\t\t"); 
      break;
    case 4 :
      strcpy(feed_string, "BZX/C2/EDGEX\t\t"); 
      break;
    default :
      strcpy(feed_string, "Unknown"); 
    }

    
    uint64_t var_subscribed_prev = 0;



    while (1)
      {
	system("clear");
	
	// Reading All central regs
	uint64_t rt_counter              = reg_read(ADDR_RT_COUNTER);
	uint64_t var_global_shadow       = reg_read(ADDR_INTERRUPT_SHADOW_RO);
	uint64_t var_nw_general_conf	 = reg_read(ADDR_NW_GENERAL_CONF);
	uint64_t var_general_conf	 = reg_read(ADDR_GENERAL_CONF);
	uint64_t var_stats_secctx	 = reg_read(ADDR_STATS_SECCTX); 
	uint64_t var_stats_secctx_tot	 = reg_read(ADDR_STATS_SECCTX_TOT); 
	uint64_t var_sw_stats_zero	 = reg_read(ADDR_SW_STATS_ZERO);
	uint64_t var_p4_general_conf	 = reg_read(ADDR_P4_GENERAL_CONF);
	uint64_t var_fatal_debug   	 = reg_read(ADDR_FATAL_CONF);
	uint64_t var_p4_fire_mdcnt   	 = reg_read(ADDR_P4_FIRE_MDCNT);
	uint64_t var_directtcp_desc_cnt	 = reg_read(ADDR_DIRECT_TCP_DESC_SENT);
	uint64_t var_directtcp_byte_cnt  = reg_read(ADDR_DIRECT_TCP_SENT_BYTES);

      // Reading All per core regs

      for(curr_core = 0; curr_core < NUM_OF_CORES; curr_core++){
	var_stats_rx_pps[curr_core]      = reg_read(ADDR_STATS_RX_PPS+curr_core*0x1000);
	//	var_stats_rx_bps_curr[curr_core] = reg_read(ADDR_STATS_RX_BPS_CURR+curr_core*0x1000);
	//	var_stats_rx_bps_max[curr_core]  = reg_read(ADDR_STATS_RX_BPS_MAX+curr_core*0x1000);
	//	var_stats_rx_bps_tot[curr_core]  = reg_read(ADDR_STATS_RX_BPS_TOT+curr_core*0x1000);
	var_stats_rx_pps_tot[curr_core]  = reg_read(ADDR_STATS_RX_PPS_TOT+curr_core*0x1000);

	//	var_stats_tx_pps[curr_core]      = reg_read(ADDR_STATS_TX_PPS+curr_core*0x1000);
	//	var_stats_tx_bps_curr[curr_core] = reg_read(ADDR_STATS_TX_BPS_CURR+curr_core*0x1000);
	//	var_stats_tx_bps_max[curr_core]  = reg_read(ADDR_STATS_TX_BPS_MAX+curr_core*0x1000);
	// 	var_stats_tx_bps_tot[curr_core]  = reg_read(ADDR_STATS_TX_BPS_TOT+curr_core*0x1000);
	//	var_stats_tx_pps_tot[curr_core]  = reg_read(ADDR_STATS_TX_PPS_TOT+curr_core*0x1000);

	var_sop_counter[curr_core]           = reg_read(ADDR_SOP_COUNTER+curr_core*0x1000);
	var_order_counter[curr_core]         = reg_read(ADDR_ORDER_COUNTER+curr_core*0x1000);
	var_overrun_trade_counter[curr_core] = reg_read(ADDR_OVERRUN_TRADE_COUNTER+curr_core*0x1000);

	var_p4_cont_debug0[curr_core]   = reg_read(ADDR_P4_DEBUG0+curr_core*0x1000);
	var_p4_cont_counter1[curr_core] = reg_read(ADDR_P4_CONT_COUNTER1+curr_core*0x1000);
	var_p4_cont_counter2[curr_core] = reg_read(ADDR_P4_CONT_COUNTER2+curr_core*0x1000);
	var_p4_cont_counter3[curr_core] = reg_read(ADDR_P4_CONT_COUNTER3+curr_core*0x1000);

	var_seq_exc_params[curr_core]   = reg_read(ADDR_SEQ_EXC_PARAMS+curr_core*0x1000);
	var_seq_exc_actual[curr_core]   = reg_read(ADDR_SEQ_EXC_ACTUAL+curr_core*0x1000);
	var_seq_exc_expected[curr_core] = reg_read(ADDR_SEQ_EXC_EXPECTED+curr_core*0x1000);
      }

      uint64_t seconds = rt_counter * (1000/FREQUENCY) / 1000000000;
      //      printf("RealTime counter: \t %'.2ju (clocks) %'.2ju (seconds) %dh:%dm:%ds \n",rt_counter,seconds,seconds/3600,(seconds%3600)/60,seconds%60);
      //      printf("RealTime counter: \t %'.2ju (seconds) %dh:%dm:%ds \n",seconds,seconds/3600,(seconds%3600)/60,seconds%60);
      printf("%s %dd:%dh:%dm:%ds %s\n",feed_string,(int)seconds/86400,(int)(seconds%86400)/3600,(int)(seconds%3600)/60,(int)seconds%60,((var_general_conf>>0)&0x1) ? "(udp pass)" : "(udp kill)");

      char* is_exception= (var_global_shadow) ? (char*) RED "YES, run sn_exceptions" RESET : 
	(char*) GRN "none" RESET;
      printf("Exceptions: \t\t %s\n",is_exception);

      // General
      printf("\n --- Global Statistics ---  \n");

      printf("Subscibed: \t\t %'.1ju (%ju per second, %s)\n",(var_sw_stats_zero>>0)&MASK32, (((var_sw_stats_zero>>0)&MASK32)-var_subscribed_prev)/sleep_seconds, ((var_sw_stats_zero>>63)&0x1) ? "finalized" : "not finalized");
      printf("MC Joined: \t\t %'.1ju\n",(var_sw_stats_zero>>32)&0xff);

      var_subscribed_prev = ((var_sw_stats_zero>>0)&MASK32);

      printf("Security CTX: \t\t Current: %'.1ju Max: %'.1ju Total: %'.1ju\n",(var_stats_secctx>>0)&MASK32, (var_stats_secctx>>32)&MASK32, (var_stats_secctx_tot>>0)&MASK64);
      printf("DirectTCP: \t\t Packets: %'.1ju Bytes: %'.1ju\n",var_directtcp_desc_cnt,var_directtcp_byte_cnt);

      if ((var_fatal_debug == 0xefa0beda) || 
	  ((var_p4_general_conf>>63)&0x1) || 
	  ((var_p4_general_conf>>62)&0x1) || 
	  ((var_p4_general_conf>>0)&0x1)) {
	  printf(RED);
	  printf("\n --- Phase4 Configs --- \n");
	  if (var_fatal_debug == 0xefa0beda) printf("Fatal Debug is Active\n");
	  if ((var_p4_general_conf>>63)&0x1) printf("Force Fire mode is enabled\n");
	  if ((var_p4_general_conf>>4)&0x1)  printf("Force Fire mode is enabled even for unsubscribed\n");
	  if ((var_p4_general_conf>>62)&0x1) printf("Self Rearm mode is enabled\n");
	  if ((var_p4_general_conf>>0)&0x1 ) printf("Reports Only mode is enabled\n");
	  printf(RESET);
      }

      // P4 State
      /* printf("\n --- Phase4 State --- \n"); */

      /* printf("Last unarm reason: \t 0x%x (StratPass:%'.1ju RX_EN:%'.1ju WD:%'.1ju CTX_COL:%'.1ju GAP:%'.1ju CRC:%'.1ju HOST:%'.1ju)\n", */
      /* 	     (var_p4_cont_debug_bus>>24)&0xff, */
      /* 	     (var_p4_cont_debug_bus>>24)&0x1, */
      /* 	     (var_p4_cont_debug_bus>>25)&0x1, */
      /* 	     (var_p4_cont_debug_bus>>26)&0x1,  */
      /* 	     (var_p4_cont_debug_bus>>27)&0x1,  */
      /* 	     (var_p4_cont_debug_bus>>28)&0x1,  */
      /* 	     (var_p4_cont_debug_bus>>29)&0x1, */
      /* 	     (var_p4_cont_debug_bus>>30)&0x1 */
      /* 	     ); */
      /* printf("Last fire reason: \t 0x%x (AlwaysFire:%'.1ju BidPass:%'.1ju AskPass:%'.1ju SrcFeed:%'.1ju ArmStatus:%'.1ju)\n", */
      /* 	     (var_p4_cont_debug_bus>>33)&0xff, */
      /* 	     (var_p4_cont_debug_bus>>33)&0x1, */
      /* 	     (var_p4_cont_debug_bus>>34)&0x1, */
      /* 	     (var_p4_cont_debug_bus>>35)&0x1,  */
      /* 	     (var_p4_cont_debug_bus>>39)&0x1,  */
      /* 	     (var_p4_cont_debug_bus>>40)&0x1 */
      /* 	     ); */

      // Per Core
      printf("\n\t\t");


      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	int rxen = (var_nw_general_conf>>curr_core)&0x1;
	//	int txen = (var_nw_general_conf>>(curr_core+8))&0x1;
	int p4en = (var_nw_general_conf>>(curr_core+16))&0x1;

	char* rxenstr= (rxen) ? (char*) GRN "RX" RESET : (char*) RED "rx" RESET;
	//	char* txenstr= (txen) ? (char*) GRN "TX" RESET : (char*) RED "tx" RESET;
	char* p4enstr= (p4en) ? (char*) GRN "FIRE" RESET : (char*) RED "fire" RESET;

	//printf("Core%d Link %s\t\t",curr_core,(((var_nw_general_conf>>(curr_core+32))&0x1) == 1) ? "Up" : "Down");	  
	printf("Core%d (%s : %s)\t",curr_core,rxenstr,p4enstr);	  
	}
      printf("\n\t\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf("-----------------\t");	  
	}
      printf("\nParser Counter\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_sop_counter[curr_core])&MASK64);	  
	}
      printf("\nOrder Counter\t" );
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_order_counter[curr_core])&MASK64);	  
	}
      printf("\nTrade Counter\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_overrun_trade_counter[curr_core]>>0)&MASK32);	  
      }
      printf("\n\nRX PPS Current \t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
        uint64_t rxpps = (var_stats_rx_pps[curr_core]>>0)&MASK32;
	if (rxpps) {
	  printf(GRN);
	  printf(format,rxpps);	  
	  printf(RESET);
	}
	else
	  printf(format,rxpps);	  
      }
      printf("\nRX PPS Maximum \t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_stats_rx_pps[curr_core]>>32)&MASK32);	  
	}
      printf("\nRX Overrun\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
        uint64_t overrun = (var_overrun_trade_counter[curr_core]>>32)&MASK32;
	if (overrun) {
	  printf(RED);
	  printf(format,overrun);	  
	  printf(RESET);
	}
	else
	  printf(format,overrun);	  
      }
      printf("\nRX Gaps\t\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
        uint64_t gaps = (var_seq_exc_params[curr_core]>>0)&MASK32;
	if (gaps) {
	  printf(RED);
	  printf(format,gaps);	  
	  printf(RESET);
	}
	else
	  printf(format,gaps);	  
      }

      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
        uint64_t gaps = (var_seq_exc_params[curr_core]>>0)&MASK32;
	if (gaps) {
	  printf(RED);
	  printf("\ng:e:a:(diff)\t"); 
	  printf("%d:0x%jx:0x%jx:(%d)\t\t",int(var_seq_exc_params[curr_core]>>32)&MASK32,var_seq_exc_expected[curr_core],var_seq_exc_actual[curr_core],
		 int( (var_seq_exc_actual[curr_core]&0xfff)- var_seq_exc_expected[curr_core]));	  
	  printf(RESET);
	}
      }

      printf("\nRX Total\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_stats_rx_pps_tot[curr_core]>>0)&MASK64);	  
      }
      /* printf("\n\nTX PPS Current\t"); */
      /* for(curr_core = 0; curr_core < number_of_cores; curr_core++){ */
      /*   uint64_t txpps = (var_stats_tx_pps[curr_core]>>0)&MASK32; */
      /* 	if (txpps) { */
      /* 	  printf(GRN); */
      /* 	  printf(format,txpps);	   */
      /* 	  printf(RESET); */
      /* 	} */
      /* 	else */
      /* 	  printf(format,txpps);	   */
      /* } */
      /* printf("\nTX PPS Maximum\t"); */
      /* for(curr_core = 0; curr_core < number_of_cores; curr_core++){ */
      /* 	printf(format,(var_stats_tx_pps[curr_core]>>32)&MASK32);	   */
      /* 	} */
      /* printf("\nTX Total\t"); */
      /* for(curr_core = 0; curr_core < number_of_cores; curr_core++){ */
      /* 	printf(format,(var_stats_tx_pps_tot[curr_core]>>0)&MASK64);	   */
      /* 	} */



      printf("\n\nArmed (SM)\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
        uint64_t armed = (var_p4_cont_debug0[curr_core]>>0)&MASK1;
	if (armed) {
	  printf(GRN);
	  printf("%ju ",armed);
	  printf(RESET);
	  printf("(%s)\t\t",(((var_p4_cont_debug0[curr_core]>>1)&MASK4) == 0) ? "ready" : "busy");
	}
	else {
	  printf(RED);
	  printf("%ju ",armed);
	  printf(RESET);
	  printf("(%s)\t\t",(((var_p4_cont_debug0[curr_core]>>1)&MASK4) == 0) ? "ready" : "busy");
	}
      }
      printf("\nStrategy Runs\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_p4_cont_counter1[curr_core]>>0)&MASK32);	  
      }
      printf("\nStrategy Passed\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_p4_cont_counter1[curr_core]>>32)&MASK32);	  
	}
      printf("\nReports Sent\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_p4_cont_counter2[curr_core]>>0)&MASK32);	  
	}
      printf("\nExchange Sent\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	printf(format,(var_p4_cont_counter2[curr_core]>>32)&MASK32);	  
	}

      printf("\nFire Src Core\t");
      total_fires = 0;
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	total_fires = total_fires + ((var_p4_fire_mdcnt>>(curr_core*10))&0x3ff);
	}

      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	fire_percentage = (total_fires  == 0) ? (float)0 : (float)100*((var_p4_fire_mdcnt>>(curr_core*10))&0x3ff) / total_fires;
	printf("%ju (%.2f%%)\t\t",((var_p4_fire_mdcnt>>(curr_core*10))&0x3ff),fire_percentage);	  
	}

      printf("\nSubscribed %%\t");
      for(curr_core = 0; curr_core < number_of_cores; curr_core++){
	total_orders          = ((var_p4_cont_counter3[curr_core]>>0)&MASK32) + ((var_p4_cont_counter3[curr_core]>>32)&MASK32);
	subscribed_percentage = (total_orders == 0) ? (float)0 : (float)100*((var_p4_cont_counter3[curr_core]>>0)&MASK32) / total_orders;
	printf("%-24.2f%%",subscribed_percentage);	  
	}

      printf("\n");

      for( int a = 0; a < sleep_seconds; a=a+1){

	sleep (1);
	printf(".");
	fflush(stdout); 
      }

    if (stop_loop)
        break;
    }

  printf(RESET);


    SN_CloseDevice(DeviceId);

    return 0;
}

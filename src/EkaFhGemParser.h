#ifndef GEM_MESSAGES_H
#define GEM_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define EFH_GEMX_STRIKE_PRICE_SCALE 10000

#define EKA_GEM_TS(x) (be64toh(*(uint64_t*)(x-1) & 0xffffffffffff0000))

namespace Gem {
  inline uint64_t get_ts(const uint8_t* m) {
    return be64toh(*(uint64_t*)(m-1) & 0xffffffffffff0000);
  }

  struct GenericHdr { // Dummy
    char		message_type;     // 1
    char	        time_nano[6];     // 6
  } __attribute__((packed));

  struct best_bid_or_ask_update_short {
    char		type; // 'b' or 'a'
    char	        time_nano[6];
    uint32_t	option_id;
    char          quote_condition;
    uint16_t	market_size;
    uint16_t	price;
    uint16_t	size;
    uint16_t	cust_size;
    uint16_t	pro_cust_size;
  } __attribute__((packed));

  struct best_bid_or_ask_update_long {
    char		type; // 'B' or 'A'
    char	        time_nano[6];
    uint32_t	option_id;
    char          quote_condition;
    uint32_t	market_size;
    uint32_t	price;
    uint32_t	size;
    uint32_t	cust_size;
    uint32_t	pro_cust_size;
  } __attribute__((packed));

  struct option_directory {
    char         message_type; // 'D'
    char         time_nano[6];
    uint32_t     option_id;
    char         security_symbol[6];
    uint8_t      expiration_year; 
    uint8_t      expiration_month;
    uint8_t      expiration_day;
    uint64_t     strike_price;
    char         option_type; // 'C' or 'P'
    uint8_t      source;
    char         underlying_symbol[13];
    char         trading_type;    // “E” = Equity “I” = Index “F” = ETF “C” = Currency
    uint16_t     contract_size;
    char         option_closing_type; // “N” = Normal Hours “L” = Late Hours
    char         tradable; // “Y” = Option is tradable “N” = Option is not tradable
    char         mpv; // “E” = penny Everywhere “S” = Scaled “P” = penny Pilot
    char         closing_only; // Closing position of the option: 
    // “Y” = Option is Closing Position Only. 
    // Only Market Maker origin orders can have open position 
    // “N” = Option is not Closing Position Only
  } __attribute__((packed));

  struct system_event {
    char message_type;  // 'S' 
    char time_nano[6];
    char event_code;
    /* “O” -- Start of Messages.  */
    /*        This is always the first message sent in any trading day. */
    /*        After ~12:30am */
    /* “S” -- Start of System Hours.  */
    /*        This message indicates that the options system is open and ready to start accepting orders. */
    /*        After Start of Messages and before Start of Currency Opening Process */
    /* “F” -- Start of Currency Opening Process.  */
    /*        This message is intended to indicate that the options system has started its opening  */
    /*        auction process for currency options. */
    /*        7:30:00am */
    /* “Q” -- Start of Opening Process.  */
    /*        This message is intended to indicate that the options system has started its opening auction process. */
    /*        9:30:00am */
    /* “N” -- Start of Normal Hours Closing Process.  */
    /*        This message is intended to indicate that the options system will no longer generate new executions  */
    /*        for options that trade during normal hours. */
    /*        4:00:00pm */
    /* “L” -- Start of Late Hours Closing Process. This message is intended to indicate that the options system  */
    /*        will no longer generate new executions for options that trade during extended hours. */
    /*        4:15:00pm */
    /* “E” -- End of System Hours.  */
    /*        This message indicates that the options system is now closed. */
    /*        ~5:15pm */
    /* “C” -- End of Messages. This is always the last message sent in any trading day. */
    /*        ~5:20pm */
    /* “W” -- End of WCO Early closing. This message is intended to indicate that the exchange will no  */
    /*        longer accept any new orders or changes to existing Orders on last trading date of WCO options. */
    /*        12:00 Noon */

    uint16_t curr_year; // 2019
    uint8_t  curr_month;
    uint8_t  curr_day;
    uint8_t  version;
    uint8_t  sub_version;
  } __attribute__((packed));

  struct trading_action {
    char     message_type; // 'H'
    char     time_nano[6];
    uint32_t option_id;
    char     current_trading_state; // “H” = Halt in effect, “T” = Trading on the options system
  } __attribute__((packed));

  struct security_open_close {
    char     message_type; // 'O'
    char     time_nano[6];
    uint32_t option_id;
    char     open_state; // Y = Open for auto execution, N = Closed for auto execution
  } __attribute__((packed));

  struct opening_imbalance {
    char     message_type; // 'N'
    char     time_nano[6];
    uint32_t option_id;
    uint32_t paired_contracts;
    char     imbalance_direction; // “B” = buy imbalance “S” = sell imbalance
    uint32_t imbalance_price;
    uint32_t imbalance_volume;
  } __attribute__((packed));

  struct best_bid_and_ask_update_short {
    char     message_type; // 'q'
    char     time_nano[6];
    uint32_t option_id;
    char     quote_condition; // <space> = regular quote/autox eligible “X” = not autox eligible
    uint16_t bid_market_order_size;
    uint16_t bid_price;
    uint16_t bid_size;
    uint16_t bid_cust_size;
    uint16_t bid_pro_cust_size;
    uint16_t ask_market_order_size;
    uint16_t ask_price;
    uint16_t ask_size;
    uint16_t ask_cust_size;
    uint16_t ask_pro_cust_size;
  } __attribute__((packed));

  struct best_bid_and_ask_update_long {
    char     message_type; // 'Q'
    char     time_nano[6];
    uint32_t option_id;
    char     quote_condition; // <space> = regular quote/autox eligible “X” = not autox eligible
    uint32_t bid_market_order_size;
    uint32_t bid_price;
    uint32_t bid_size;
    uint32_t bid_cust_size;
    uint32_t bid_pro_cust_size;
    uint32_t ask_market_order_size;
    uint32_t ask_price;
    uint32_t ask_size;
    uint32_t ask_cust_size;
    uint32_t ask_pro_cust_size;
  } __attribute__((packed));

  struct ticker {
    char     message_type; // 'T'
    char     time_nano[6];
    uint32_t option_id;
    uint32_t last_price;
    uint32_t size;
    uint32_t volume;
    uint32_t high;
    uint32_t low;
    uint32_t first;
    char     trade_condition;
  } __attribute__((packed));


  struct snapshot_end {
    char message_type;
    char sequence_number[20];
  } __attribute__((packed));

} //namespece Gem

#endif

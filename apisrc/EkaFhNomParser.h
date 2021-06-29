#ifndef _EKA_FH_NOM_MESSAGES_H
#define _EKA_FH_NOM_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

//#define EFH_NOM_STRIKE_PRICE_SCALE 10
#define EFH_NOM_STRIKE_PRICE_SCALE 1

#define EKA_NOM_TS(x) (be64toh(*(uint64_t*)(x+1) & 0xffffffffffff0000))

namespace Nom {
  inline uint64_t get_ts(const uint8_t* m) {
    uint64_t ts_tmp = 0;
    memcpy((uint8_t*)&ts_tmp+2,m+3,6);
    return be64toh(ts_tmp);
  }

  
  struct GenericHdr { // Dummy
    char	  message_type;     // 1
    uint16_t      tracking_num;     // 2
    char	  time_nano[6];     // 6
  } __attribute__((packed));

  
  /* struct time_stamp { // obsolete */
  /* 	char		type; */
  /* 	uint32_t	time_sec; // number of seconds since midnight */
  /* } __attribute__((packed)); */

  struct system_event { // 'S'
    char		type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    char		event_code;       // 1
  } __attribute__((packed));

  /* struct base_reference { // obsolete */
  /* 	char		type; */
  /* 	char	        time_nano[6]; */
  /* 	uint64_t	reference; */
  /* } __attribute__((packed)); */

  struct definition { // 'R'
    char		type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint32_t	option_id;        // 4
    char		security_symbol[6];// 6
    uint8_t	expiration_year;  // 1
    uint8_t	expiration_month; // 1
    uint8_t       expiration_day;   // 1
    uint32_t	strike_price;     // 4
    char		option_type;      // 1
    uint8_t      	source;           // 1
    char		underlying_symbol[13]; // 13
    char		option_closing_type;// 1
    char		tradable;         // 1
    char		mpv;              // 1
  } __attribute__((packed));

  struct trading_action { // 'H'
    char		type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint32_t	option_id;        // 4
    char		trading_state;    // 1
  } __attribute__((packed));

  struct option_open { // 'O'
    char		type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint32_t	option_id;        // 4
    char		open_state;       // 1
  } __attribute__((packed));

  struct add_order_short { // 'a'
    char		type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	order_reference_delta;//8
    char		side;             // 1
    uint32_t	option_id;        // 4
    uint16_t	price;            // 2
    uint16_t	size;             // 2
  } __attribute__((packed));

  struct add_order_long { // 'A'
    char		type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	order_reference_delta; // 8
    char		side;             // 1
    uint32_t	option_id;        // 4
    uint32_t	price;            // 4
    uint32_t	size;             // 4
  } __attribute__((packed));

  struct add_quote_short { // 'j'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t      bid_reference_delta; // 8
    uint64_t      ask_reference_delta; // 8
    uint32_t      option_id;        // 4
    uint16_t      bid_price;        // 2
    uint16_t      bid_size;         // 2
    uint16_t      ask_price;        // 2
    uint16_t      ask_size;         // 2
  } __attribute__((packed));

  struct add_quote_long { // 'J'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t      bid_reference_delta; // 8
    uint64_t      ask_reference_delta; // 8
    uint32_t      option_id;        // 4
    uint32_t      bid_price;        // 4
    uint32_t      bid_size;         // 4
    uint32_t      ask_price;        // 4
    uint32_t      ask_size;         // 4
  } __attribute__((packed));

  struct executed {  // 'E'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	order_reference_delta; // 8
    uint32_t	executed_contracts;// 4
    uint32_t	cross_number;     // 4
    uint32_t	match_number;     // 4
  } __attribute__((packed));

  struct executed_price {  // 'C'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	order_reference_delta; // 8
    uint32_t	cross_number;     // 4
    uint32_t	match_number;     // 4
    char		printable;        // 1
    uint32_t	price;            // 4
    uint32_t	size;             // 4
  } __attribute__((packed));

  struct order_cancel {  // 'X'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	order_reference_delta; // 8
    uint32_t	cancelled_orders; // 4
  } __attribute__((packed));

  struct order_replace_short {  // 'u'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	original_reference_delta; // 8
    uint64_t	new_reference_delta;      // 8
    uint16_t	price;            // 2
    uint16_t	size;             // 2
  } __attribute__((packed));
 
  struct order_replace_long {  // 'U'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	original_reference_delta; // 8
    uint64_t	new_reference_delta;      // 8
    uint32_t	price;            // 4
    uint32_t	size;             // 4
  } __attribute__((packed));

  struct order_delete {  // 'D'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	reference_delta;  // 8
  } __attribute__((packed));

  struct order_update {  // 'G'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	reference_delta;  // 8
    char		reason;           // 1
    uint32_t	price;            // 4
    uint32_t	size;	          // 4
  } __attribute__((packed));

  struct quote_replace_short {  // 'k'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	original_bid_delta; // 8
    uint64_t	new_bid_delta;      // 8  
    uint64_t	original_ask_delta; // 8
    uint64_t	new_ask_delta;    // 8 
    uint16_t	bid_price;        // 2 
    uint16_t	bid_size;         // 2 
    uint16_t	ask_price;        // 2 
    uint16_t	ask_size;         // 2 	
  } __attribute__((packed));

  struct quote_replace_long {  // 'K'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	original_bid_delta; // 8
    uint64_t	new_bid_delta;      // 8  
    uint64_t	original_ask_delta; // 8
    uint64_t	new_ask_delta;    // 8 
    uint32_t	bid_price;        // 4 
    uint32_t	bid_size;         // 4 
    uint32_t	ask_price;        // 4 
    uint32_t	ask_size;         // 4 	
  } __attribute__((packed));

  struct quote_delete {  // 'Y'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	bid_delta;	  // 8
    uint64_t	ask_delta;	  // 8
  } __attribute__((packed));

  struct block_delete {  // 'Z'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint16_t	count;            // 2
  } __attribute__((packed));

  struct options_trade { // 'P'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint8_t       buy_sell;         // 1
    uint32_t      option_id;        // 4
    uint32_t      cross_num;        // 4
    uint32_t      match_num;        // 4
    uint32_t	price;            // 4 
    uint32_t	size;             // 4 
  } __attribute__((packed));

  struct cross_trade {   // 'Q'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint32_t      option_id;        // 4
    uint32_t      cross_num;        // 4
    uint32_t      match_num;        // 4
    uint8_t       cross_type;       // 1
    uint32_t	price;            // 4 
    uint32_t	size;             // 4 
  } __attribute__((packed));

  struct broken_exec {  // 'B'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint32_t      cross_num;        // 4
    uint32_t      match_num;        // 4
  } __attribute__((packed));

  struct noii {          // 'I'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint32_t      auction_id;       // 4
    char          auction_type;     // 1
    uint32_t      paired_contracts; // 4
    char          imbalance_dir;    // 1
    uint32_t      option_id;        // 4
    uint32_t	imbalance_price;  // 4 
    uint32_t	imbalance_size;   // 4 
  } __attribute__((packed));

  struct end_of_snapshot { // 'M'
    char          type;             // 1
    char          sequence_number[20];

  } __attribute__((packed));

} // Nom namespace

#define ITTO_NOM_MSG(x)					\
  x == 'S' ? "SYSTEM_EVENT" :				\
    x == 'R' ? "DIRECTORY"       :			\
    x == 'H' ? "TRADING_ACTION"  :			\
    x == 'O' ? "OPTION_OPEN"     :			\
    x == 'a' ? "ADD_ORDER_SHORT" :			\
    x == 'A' ? "ADD_ORDER_LONG"  :			\
    x == 'j' ? "ADD_QUOTE_SHORT" :			\
    x == 'J' ? "ADD_QUOTE_LONG"  :			\
    x == 'E' ? "SINGLE_SIDE_EXEC"  :			\
    x == 'C' ? "SINGLE_SIDE_EXEC_PRICE"  :		\
    x == 'u' ? "SINGLE_SIDE_REPLACE_SHORT" :		\
    x == 'U' ? "SINGLE_SIDE_REPLACE_LONG"  :		\
    x == 'D' ? "SINGLE_SIDE_DELETE" :			\
    x == 'G' ? "SINGLE_SIDE_UPDATE" :			\
    x == 'k' ? "QUOTE_REPLACE_SHORT" :			\
    x == 'K' ? "QUOTE_REPLACE_LONG" :			\
    x == 'Y' ? "QUOTE_DELETE"  :			\
    x == 'P' ? "TRADE"  :				\
    x == 'Q' ? "CROSS_TRADE"  :				\
    x == 'B' ? "BROKEN_TRADE"  :			\
    x == 'I' ? "NOII"  :				\
    x == 'M' ? "END_OF_SNAPSHOT"  :			\
    "UNKNOWN"

#define ITTO_NOM_TRADING_ACTION(x)					\
  x == 'T' ? EfhTradeStatus::kNormal : EfhTradeStatus::kHalted


#endif

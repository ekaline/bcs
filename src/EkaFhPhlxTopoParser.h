#ifndef EKA_FH_PHLX_MESSAGES_H
#define EKA_FH_PHLX_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

//#define EFH_PHLX_STRIKE_PRICE_SCALE 10
#define EFH_PHLX_STRIKE_PRICE_SCALE 1

namespace TOPO {
  struct GenericHdr { // only used to get nanoseconds
    char     message_type;         // 
    uint32_t time_nano;         // 4
  } __attribute__((packed));

  inline uint32_t getNs(const uint8_t* m) {
    return be32toh(reinterpret_cast<const GenericHdr*>(m)->time_nano);
  }

  struct TimeStamp {
    char     message_type;         // 1 = 'T'
    uint32_t time_seconds;         // 4
  } __attribute__((packed));

  struct topo_system_event {
    char     message_type;         // 1 = 'S'
    uint32_t time_nano;            // 4
    uint8_t  event_code;           // 1 
    /* “O” -- Start of Messages.  */
    /*        This is always the first message sent in any trading day.  */
    /*        After ~2:00am */
    /* “S” -- Start of System Hours.  */
    /*        This message indicates that the options system is open and ready to start accepting orders.  */
    /*        7:00am */
    /* “Q” -- Start of Opening Process.  */
    /*        This message is intended to indicate that the options system has started its opening process.  */
    /*        9:30:00am */
    /* “N” -- End of Normal Hours Processing.  */
    /*        This message is intended to indicate that the options system will no longer generate new executions  */
    /*        for options that trade during normal hours.  */
    /*        4:00:00pm */
    /* “L” -- End of Late Hours Processing.  */
    /*        This message is intended to indicate that the options system will no longer generate new executions  */
    /*        for options that trade during normal hours.  */
    /*        4:15:00pm */
    /* “E” -- End of System Hours.  */
    /*        This message indicates that the options system is now closed.  */
    /*        ~5:15pm */
    /* “C” -- End of Messages.  */
    /*        This is always the last message sent in any trading day.  */
    /*        ~5:20pm */
    /* “W”  -- End of WCO Early closing.  */
    /*        This message is intended to indicate that the exchange will no longer accept any new orders or  */
    /*        changes to existing Orders on last trading date of WCO options.  */
    /*        12:00 Noon */
    uint8_t  version;              // 1 = 3
    uint8_t  sub_version;          // 1 = 0
  } __attribute__((packed));

  struct Directory { // TOPO OPTIONS
    char     message_type;         // 1 = 'D'
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    char     security_symbol[6];   // 6
    uint8_t  expiration_year;      // 1 (last 2 digits of the year)
    uint8_t  expiration_month;     // 1 (1-12)
    uint8_t  expiration_day;       // 1 (1-31)
    uint32_t strike_price;         // 4
    char     option_type;          // 1 ('P' or 'C')
    uint8_t  source;               // 1
    char     underlying_symbol[13];// 13
    char     option_closing_type;  // 1 ('N' - Normal hours, 'L' - Late hours, 'W' - WCO)
    char     tradable;             // 1 ('Y' or 'N' = yes or no)
    char     mpv;                  // 1 ('E' - penny Everywhere, 'S' - Scaled, 'P' - penny Pilot)
  } __attribute__((packed));

  struct trading_action {
    char     message_type;         // 1 = 'H'
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    char     current_trading_state;// 1 'H' - halt, 'T' - trading resumed
  } __attribute__((packed));

  struct security_open_closed {
    char     message_type;         // 1 = 'O'
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    char     open_state;           // 1 'Y' - open for auto execution, 'N' - closed for auto execution
  } __attribute__((packed));

  struct best_bid_and_ask_update_short {
    char     message_type;         // 1 = 'q'
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    char     quote_condition;      // 1 
    // <space>=regular quote/autox eligible
    // “F” = Non-Firm Quote on both bid/ask sides
    // “R” = Rotational Quote
    // “X” = Ask side not firm; bid side firm
    // “Y” = Bid side not firm; ask side firm
    uint16_t bid_price;            // 2
    uint16_t bid_size;             // 2
    uint16_t ask_price;            // 2
    uint16_t ask_size;             // 2
  } __attribute__((packed));

  struct best_bid_and_ask_update_long {
    char     message_type;         // 1 = 'Q'
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    char     quote_condition;      // 1 
    // <space>=regular quote/autox eligible
    // “F” = Non-Firm Quote on both bid/ask sides
    // “R” = Rotational Quote
    // “X” = Ask side not firm; bid side firm
    // “Y” = Bid side not firm; ask side firm
    uint32_t bid_price;            // 4
    uint32_t bid_size;             // 4
    uint32_t ask_price;            // 4
    uint32_t ask_size;             // 4
  } __attribute__((packed));

  struct best_bid_or_ask_update_short {
    char     message_type;         // 1 = 'a' or 'b'
    // 'b' = Quote update bid side 
    // 'a' = Quote update ask side
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    char     quote_condition;      // 1 
    // <space>=regular quote/autox eligible
    // “F” = Non-Firm Quote on both bid/ask sides
    // “R” = Rotational Quote
    // “X” = Ask side not firm; bid side firm
    // “Y” = Bid side not firm; ask side firm
    uint16_t price;                // 2
    uint16_t size;                 // 2
  } __attribute__((packed));

  struct best_bid_or_ask_update_long {
    char     message_type;         // 1 = 'A' or 'B'
    // 'B' = Quote update bid side 
    // 'A' = Quote update ask side
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    char     quote_condition;      // 1 
    // <space>=regular quote/autox eligible
    // “F” = Non-Firm Quote on both bid/ask sides
    // “R” = Rotational Quote
    // “X” = Ask side not firm; bid side firm
    // “Y” = Bid side not firm; ask side firm
    uint32_t price;                // 4
    uint32_t size;                 // 4
  } __attribute__((packed));

  struct trade_report {
    char     message_type;         // 1 = 'R'
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    uint32_t cross_id;             // 4 
    char     trade_condition;      // 1 “Last Sale” or “Transaction”

    uint32_t price;                // 4
    uint32_t size;                 // 4
  } __attribute__((packed));

  struct broken_trade_report {
    char     message_type;         // 1 = 'X'
    uint32_t time_nano;            // 4
    uint32_t option_id;            // 4
    uint32_t original_cross_id;    // 4 
    uint32_t price;                // 4
    uint32_t size;                 // 4
  } __attribute__((packed));


  /* struct topo_order_directory { */
  /*   char     message_type;         // 1 */
  /*   uint32_t time_seconds;         // 4 */
  /*   uint32_t time_nano;            // 4 */
  /*   uint32_t option_id;            // 4 */
  /*   char     security_symbol[5];   // 5 */
  /*   uint16_t expiration;           // 2   */
  /*                                       /\* Bits 0-6 = Year (0-99) *\/ */
  /*                                       /\* Bits 7-10 = Month (1-12) *\/ */
  /*                                       /\* Bits 11-15 = Day (1-31) *\/ */
  /*                                       /\* Bit 15 is least significant bit *\/ */
  /*   uint32_t strike_price;         // 4 */
  /*   char     option_type;          // 1  Put or Call */
  /*   uint8_t  source;               // 1 */
  /*   char     underlying_symbol[13];// 13 */
  /*   uint8_t  option_closing_type;  // 1 */
  /*   uint8_t  tradable;             // 1 */
  /* } __attribute__((packed)); */

} // namespace TOPO

#endif

#ifndef EKA_FH_PHLX_MESSAGES_H
#define EKA_FH_PHLX_MESSAGES_H
#include <stdint.h>
#include <unistd.h>

//#define EFH_PHLX_STRIKE_PRICE_SCALE 10
#define EFH_PHLX_STRIKE_PRICE_SCALE 1

struct phlx_ord_generic_hdr { // only used to get nanoseconds
  char     message_type;         // 1 = 'D'
  uint32_t time_seconds;         // 4
  uint32_t time_nano;            // 4
} __attribute__((packed));

struct phlx_ord_system_event {
  char     message_type;         // 1 = 'S'
  uint32_t time_seconds;         // 4
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
  uint8_t  version;              // Version of PHLX Orders. Currently set to 1
} __attribute__((packed));

struct phlx_ord_option_directory { // TOPO ORDERS
  char     message_type;         // 1 = 'D'
  uint32_t time_seconds;         // 4
  uint32_t time_nano;            // 4
  uint32_t option_id;            // 4
  char     security_symbol[5];   // 5
  uint16_t expiration;           // 2     // Denotes the explicit expiration date of the option.
                                          // Bits 0-6 = Year (0-99)
                                          // Bits 7-10 = Month (1-12)
                                          // Bits 11-15 = Day (1-31)
                                          // Bit 15 is least significant bit
  uint32_t strike_price;         // 4
  char     option_type;          // 1 ('P' or 'C')
  uint8_t  source;               // 1
  char     underlying_symbol[13];// 13
  char     option_closing_type;  // 1 ('N' - Normal hours, 'L' - Late hours, 'W' - WCO)
  char     tradable;             // 1 ('Y' or 'N' = yes or no)
} __attribute__((packed));

struct phlx_ord_option_trading_action {
  char     message_type;         // 1 = 'H'
  uint32_t time_seconds;         // 4
  uint32_t time_nano;            // 4
  uint32_t option_id;            // 4
  char     symbol[5];            // 5
  uint16_t expiration;           // 2 Bits 0-6 = Year (0-99)
                                 //   Bits 7-10 = Month (1-12)
                                 //   Bits 11-15 = Day (1-31)
  uint32_t strike_price;         // 4
  char     option_type;          // 1 “C” = Call, “P” = Put
  char     current_trading_state;// 1 'H' - halt, 'T' - trading resumed
} __attribute__((packed));

struct phlx_ord_security_open_closed {
  char     message_type;         // 1 = 'p'
  uint32_t time_seconds;         // 4
  uint32_t time_nano;            // 4
  uint32_t option_id;            // 4
  char     symbol[5];            // 5
  uint16_t expiration;           // 2 Bits 0-6 = Year (0-99)
                                 //   Bits 7-10 = Month (1-12)
                                 //   Bits 11-15 = Day (1-31)
  uint32_t strike_price;         // 4
  char     option_type;          // 1 “C” = Call, “P” = Put
  char     open_state;           // 1 'Y' - open for auto execution, 'N' - closed for auto execution
} __attribute__((packed));


struct phlx_ord_simple_order {
  char     message_type;         // 1 = 'O'
  uint32_t time_seconds;         // 4
  uint32_t time_nano;            // 4
  uint32_t option_id;            // 4
  char     symbol[5];            // 5
  uint16_t expiration;           // 2 Bits 0-6 = Year (0-99)
                                 //   Bits 7-10 = Month (1-12)
                                 //   Bits 11-15 = Day (1-31)
  uint32_t strike_price;         // 4
  char     option_type;          // 1 “C” = Call, “P” = Put

  uint32_t order_id;             // 4

  char     side;                 // 1 'B' - Buy, 'S' - Sell
  uint32_t size;                 // 4 Original Order Volume
  uint32_t exec_size;            // 4 Executable Order Volume
  char     order_status;         // 1  
                                 //    “O” = Open, 
                                 //    “F” = Filled, 
                                 //    “C” = Cancelled

  char     order_type;           // 1  
                                 //    “M” = Market, 
                                 //    “L” = Limit

  char     market_qualifier;     // 1 
                                 //     “O” = Opening Order, 
                                 //     “I” = Implied Order, 
                                 //     “ ” = N/A (field is space char)

  uint32_t price;                // 4 Limit Price
  char     all_or_none;          // 1 
                                 //     “Y” = Order is All or None Order, 
                                 //     “N” = Order is not All or None Order
  char     time_in_force;        // 1 “D” = Day Order, “G” = Good till cancelled (GTC) Order
  char     customer_indicator;   // 1 
                                 //      “C” = Customer Order
                                 //      “F” = Firm Order
                                 //      “M” = On-floor Market Maker
                                 //      “B” = Broker Dealer Order
                                 //      “P” = Professional Order
                                 //      “ ” = N/A (For Implied Order)
  char    open_close_indicator;  // 1 
                                 //      “O” = Opens position
                                 //      “C” = Closes position
                                 //      “ ” = N/A (For Implied Order)
} __attribute__((packed));


#endif

#ifndef _ULTRAFAST_TCP_DEF_H_
#define _ULTRAFAST_TCP_DEF_H_

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ether.h>

//typedef int32_t session_id_t;

enum {
    MAX_CORES           = 6,
    MAX_SESS_PER_CORE   = 24,
};

enum exc_error {
    TCP_STATE_CLOSED                    = -1,
    TCP_STATE_LISTEN                    = -2,
    TCP_STATE_SYN_SENT                  = -3,
    TCP_STATE_SYN_RECEIVED              = -4,
    TCP_STATE_FINWAIT1                  = -5,
    TCP_STATE_FINWAIT2                  = -6,
    TCP_STATE_CLOSE_WAIT                = -7,
    TCP_STATE_CLOSING                   = -8,
    TCP_STATE_LAST_ACK                  = -9,
    TCP_STATE_TIME_WAIT                 = -10,
    TERMINATED_VALID_RST_RCVD           = -15,
    TERMINATED_ILLEGAL_PCKT_RST         = -16,
    TERMINATED_TX_TIMEOUT               = -17,
    TERMINATED_TX_BUFFER_OVERFLOW       = -18,
    UNABLE_TO_GET_LOCK                  = -20,
    CORE_DECODE_FAILURE                 = -100,
    SESS_DECODE_FAILURE                 = -101,
};

struct exc_init_attrs {
    in_addr_t           host_ip;
    in_addr_t           netmask;
    in_addr_t           gateway;
    /* nexthop_mac - 
     * if unset, exc_connect will send an ARP request to fill nexthop_mac */
    struct ether_addr   nexthop_mac; 
    /* src_mac_addr - 
     * if unset, src_mac will be set from eeprom */
    struct ether_addr   src_mac_addr; 
    /* dont_garp - 
     * set to 1, card will NOT send Gratuitous ARP packets */
    uint8_t             dont_garp;
    // deprecated --- MAX SESSIONS IS SET TO 48 PER CORE - max buffer 8K
    //uint16_t            number_of_sessions; //PER TOE CORE!
    /* default will be set to 32 - 
     * affects the TX buffer size per session.
     * size will be 64K / number_of_sessions (for 32 = 341B)
     * TX buffer size = maximum packet size per single exc_send per session
     */
};

#endif //_ULTRAFAST_TCP_DEF_H_

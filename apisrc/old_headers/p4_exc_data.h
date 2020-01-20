#ifndef _P4_EXC_DATA_H_
#define _P4_EXC_DATA_H_

#include <stdint.h>

#include <ultrafast/p4_data.h>

struct miax_sec_ctx {
    struct sec_ctx common;
} __attribute__((packed));

struct nasdaq_sec_ctx {
    struct sec_ctx  common;
} __attribute__((packed));

struct sqf_quote_sec_ctx {
    struct sec_ctx  common;
} __attribute__((packed));

/* up to 128 session_ctx are possible */
struct miax_session_ctx {
    struct common_session_ctx csc;
    union {
        uint32_t mpid_num;
        char mpid[4];
    } mpid;
} __attribute__((packed));

struct sqf_session_ctx {
    struct common_session_ctx   csc;
    uint32_t                    sqf_badge;
} __attribute__((packed));

/* Fields are based on MIAX_Express_Interface_MEI_v1_9.pdf p17-18
*/

struct miax_equote_message {
    char        message_type[2];
    uint32_t    client_message_id;
    char        mpid[4];
    uint32_t    product_id;
    char        equote_action;
    char        equote_type;
    uint32_t    event_id;
    uint32_t    target_message_id;
    uint32_t    price;
    uint32_t    size;
    char        side;
} __attribute__ ((packed));

#endif /* _P4_EXC_DATA_H_ */

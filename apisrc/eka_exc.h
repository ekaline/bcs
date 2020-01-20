#ifndef EKA_EXC_H
#define EKA_EXC_H

#include <pthread.h>
#include <signal.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/api.h"

class eka_user_channel;
class EkaDev;

/* #include <pico_icmp4.h> */
/* #include <pico_dev_tap.h> */
/* #include <pico_socket.h> */
/* #include "pico_config.h" */
/* #include "pico_device.h" */
/* #include "pico_stack.h" */

/* typedef err_t(* netif_linkoutput_fn) (struct netif *netif, struct pbuf *p); */

/* struct EkaPicoTcpDev { */
/*   struct pico_device*       picoDev; */
/*   struct pico_ip4           ip; */
/*   struct pico_ip4           bcast; */
/*   struct pico_ip4           nm; */
/*   struct pico_eth           mac; */
/* }; */

struct LwipNetifState {
  EkaDev* pEkaDev;
  uint8_t        lane;
  uint8_t        pad[7];
};

typedef union exc_table_desc {
        uint64_t desc;
        struct fields {
            uint8_t source_bank;
            uint8_t source_thread;
            uint32_t target_idx : 24;
            uint8_t pad[3];
        } __attribute__((packed)) td;
} __attribute__((packed)) exc_table_desc_t;

#endif //EKA_EXC_H


/**
 * @file
 *
 * lwIP Options Configuration
 */


#if !defined _EKALINE_LWIPOPT_H
#define _EKALINE_LWIPOPT_H

#define NO_SYS                          0

/**
 * LWIP_TIMERS==0: Drop support for sys_timeout and lwip-internal cyclic timers.
 * (the array of lwip-internal cyclic timers is still provided)
 * (check NO_SYS_NO_TIMERS for compatibility to old versions)
 */

#define LWIP_TIMERS                     1

/*
   ------------------------------------
   ----------- Core locking -----------
   ------------------------------------
*/


/**
 * LWIP_TCPIP_CORE_LOCKING
 * Creates a global mutex that is held during TCPIP thread operations.
 * Can be locked by client code to perform lwIP operations without changing
 * into TCPIP thread using callbacks. See LOCK_TCPIP_CORE() and
 * UNLOCK_TCPIP_CORE().
 * Your system should provide mutexes supporting priority inversion to use this.
 */

#define LWIP_TCPIP_CORE_LOCKING         1
#ifdef __cplusplus
extern "C" {
#endif
void sys_lock_tcpip_core();
void sys_unlock_tcpip_core();
void sys_check_core_locking();
void sys_mark_tcpip_thread();
#ifdef __cplusplus
} // End of extern "C"
#endif

#define LOCK_TCPIP_CORE() sys_lock_tcpip_core()
#define UNLOCK_TCPIP_CORE() sys_unlock_tcpip_core()
#define LWIP_MARK_TCPIP_THREAD() sys_mark_tcpip_thread()
#define LWIP_ASSERT_CORE_LOCKED() sys_check_core_locking()

/**
 * LWIP_TCPIP_CORE_LOCKING_INPUT: when LWIP_TCPIP_CORE_LOCKING is enabled,
 * this lets tcpip_input() grab the mutex for input packets as well,
 * instead of allocating a message and passing it to tcpip_thread.
 *
 * ATTENTION: this does not work when tcpip_input() is called from
 * interrupt context!
 */

#define LWIP_TCPIP_CORE_LOCKING_INPUT   1

/**
 * SYS_LIGHTWEIGHT_PROT==1: enable inter-task protection (and task-vs-interrupt
 * protection) for certain critical regions during buffer allocation, deallocation
 * and memory allocation and deallocation.
 * ATTENTION: This is required when using lwIP from more than one context! If
 * you disable this, you must be sure what you are doing!
 */

#define SYS_LIGHTWEIGHT_PROT            1


/*
   ------------------------------------
   ---------- Memory options ----------
   ------------------------------------
*/
/**
 * @defgroup lwip_opts_mem Heap and memory pools
 * @ingroup lwip_opts_infrastructure
 * @{
 */
/**
 * MEM_LIBC_MALLOC==1: Use malloc/free/realloc provided by your C-library
 * instead of the lwip internal allocator. Can save code size if you
 * already use it.
 */

#define MEM_LIBC_MALLOC                 0


/**
 * MEMP_MEM_MALLOC==1: Use mem_malloc/mem_free instead of the lwip pool allocator.
 * Especially useful with MEM_LIBC_MALLOC but handle with care regarding execution
 * speed (heap alloc can be much slower than pool alloc) and usage from interrupts
 * (especially if your netif driver allocates PBUF_POOL pbufs for received frames
 * from interrupt)!
 * ATTENTION: Currently, this uses the heap for ALL pools (also for private pools,
 * not only for internal pools defined in memp_std.h)!
 */

#define MEMP_MEM_MALLOC                 0


/**
 * MEMP_MEM_INIT==1: Force use of memset to initialize pool memory.
 * Useful if pool are moved in uninitialized section of memory. This will ensure
 * default values in pcbs struct are well initialized in all conditions.
 */

#define MEMP_MEM_INIT                   1

/**
 * MEM_SIZE: the size of the heap memory. If the application will send
 * a lot of data that needs to be copied, this should be set high.
 */

#define MEM_SIZE                        (16 * 1024 * 1024)


/*
   ------------------------------------------------
   ---------- Internal Memory Pool Sizes ----------
   ------------------------------------------------
*/
/**
 * @defgroup lwip_opts_memp Internal memory pools
 * @ingroup lwip_opts_infrastructure
 * @{
 */
/**
 * MEMP_NUM_PBUF: the number of memp struct pbufs (used for PBUF_ROM and PBUF_REF).
 * If the application sends a lot of data out of ROM (or other static memory),
 * this should be set high.
 */

#define MEMP_NUM_PBUF                   (16 * 1024)


/**
 * MEMP_NUM_RAW_PCB: Number of raw connection PCBs
 * (requires the LWIP_RAW option)
 */

#define MEMP_NUM_RAW_PCB                64


/**
 * MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
 * per active UDP "connection".
 * (requires the LWIP_UDP option)
 */

#define MEMP_NUM_UDP_PCB                64


/**
 * MEMP_NUM_TCP_PCB: the number of simultaneously active TCP connections.
 * (requires the LWIP_TCP option)
 */

#define MEMP_NUM_TCP_PCB                64


/**
 * MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP connections.
 * (requires the LWIP_TCP option)
 */

#define MEMP_NUM_TCP_PCB_LISTEN         8

/**
 * MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP segments.
 * (requires the LWIP_TCP option)
 */

#define MEMP_NUM_TCP_SEG                256

/**
 * MEMP_NUM_TCPIP_MSG_API: the number of struct tcpip_msg, which are used
 * for callback/timeout API communication.
 * (only needed if you use tcpip.c)
 */

#define MEMP_NUM_TCPIP_MSG_API          256

/**
 * MEMP_NUM_REASSDATA: the number of IP packets simultaneously queued for
 * reassembly (whole packets, not fragments!)
 */

#define MEMP_NUM_REASSDATA              256


/**
 * MEMP_NUM_TCPIP_MSG_INPKT: the number of struct tcpip_msg, which are used
 * for incoming packets.
 * (only needed if you use tcpip.c)
 */

#define MEMP_NUM_TCPIP_MSG_INPKT        256


/**
 * MEMP_NUM_IGMP_GROUP: The number of multicast groups whose network interfaces
 * can be members at the same time (one per netif - allsystems group -, plus one
 * per netif membership).
 * (requires the LWIP_IGMP option)
 */

#define MEMP_NUM_IGMP_GROUP             64


/**
 * The number of sys timeouts used by the core stack (not apps)
 * The default number of timeouts is calculated here for all enabled modules.
 */
#if 0
#define LWIP_NUM_SYS_TIMEOUT_INTERNAL   (LWIP_TCP + IP_REASSEMBLY + LWIP_ARP + (2*LWIP_DHCP) + LWIP_AUTOIP + LWIP_IGMP + LWIP_DNS + PPP_NUM_TIMEOUTS + (LWIP_IPV6 * (1 + LWIP_IPV6_REASS + LWIP_IPV6_MLD)))
#define LWIP_NUM_SYS_TIMEOUT_INTERNAL   1
#endif

/**
 * MEMP_NUM_SYS_TIMEOUT: the number of simultaneously active timeouts.
 * The default number of timeouts is calculated here for all enabled modules.
 * The formula expects settings to be either '0' or '1'.
 */
#if !defined MEMP_NUM_SYS_TIMEOUT || defined __DOXYGEN__
#define MEMP_NUM_SYS_TIMEOUT            LWIP_NUM_SYS_TIMEOUT_INTERNAL
#endif

/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 */

#define MEMP_NUM_NETCONN                64


/**
 * MEMP_NUM_SELECT_CB: the number of struct lwip_select_cb.
 * (Only needed if you have LWIP_MPU_COMPATIBLE==1 and use the socket API.
 * In that case, you need one per thread calling lwip_select.)
 */

#define MEMP_NUM_SELECT_CB              64


/**
 * PBUF_POOL_SIZE: the number of buffers in the pbuf pool.
 */

#define PBUF_POOL_SIZE                  (16 * 1024)


/*
   ---------------------------------
   ---------- ARP options ----------
   ---------------------------------
*/
/**
 * @defgroup lwip_opts_arp ARP
 * @ingroup lwip_opts_ipv4
 * @{
 */

#define LWIP_ARP                        1

/**
 * ARP_TABLE_SIZE: Number of active MAC-IP address pairs cached.
 */
#define ARP_TABLE_SIZE                  64

/** the time an ARP entry stays valid after its last update,
 *  for ARP_TMR_INTERVAL = 1000, this is
 *  (60 * 5) seconds = 5 minutes.
 */

#define ARP_MAXAGE                      (10 * 60 * 60)


/**
 * ARP_QUEUEING==1: Multiple outgoing packets are queued during hardware address
 * resolution. By default, only the most recent packet is queued per IP address.
 * This is sufficient for most protocols and mainly reduces TCP connection
 * startup time. Set this to 1 if you know your application sends more than one
 * packet in a row to an IP address that is not in the ARP cache.
 */
#define ARP_QUEUEING                    1


/**
 * ETHARP_SUPPORT_VLAN==1: support receiving and sending ethernet packets with
 * VLAN header. See the description of LWIP_HOOK_VLAN_CHECK and
 * LWIP_HOOK_VLAN_SET hooks to check/set VLAN headers.
 * Additionally, you can define ETHARP_VLAN_CHECK to an u16_t VLAN ID to check.
 * If ETHARP_VLAN_CHECK is defined, only VLAN-traffic for this VLAN is accepted.
 * If ETHARP_VLAN_CHECK is not defined, all traffic is accepted.
 * Alternatively, define a function/define ETHARP_VLAN_CHECK_FN(eth_hdr, vlan)
 * that returns 1 to accept a packet or 0 to drop a packet.
 */

#define ETHARP_SUPPORT_VLAN             0


/** LWIP_ETHERNET==1: enable ethernet support even though ARP might be disabled
 */

#define LWIP_ETHERNET                   LWIP_ARP


/** ETH_PAD_SIZE: number of bytes added before the ethernet header to ensure
 * alignment of payload after that header. Since the header is 14 bytes long,
 * without this padding e.g. addresses in the IP header will not be aligned
 * on a 32-bit boundary, so setting this to 2 can speed up 32-bit-platforms.
 */

#define ETH_PAD_SIZE                    0

/** ETHARP_SUPPORT_STATIC_ENTRIES==1: enable code to support static ARP table
 * entries (using etharp_add_static_entry/etharp_remove_static_entry).
 */

#define ETHARP_SUPPORT_STATIC_ENTRIES   1


/** ETHARP_TABLE_MATCH_NETIF==1: Match netif for ARP table entries.
 * If disabled, duplicate IP address on multiple netifs are not supported
 * (but this should only occur for AutoIP).
 */

//#define ETHARP_TABLE_MATCH_NETIF        !LWIP_SINGLE_NETIF
#define ETHARP_TABLE_MATCH_NETIF        0

/**
 * @}
 */

/*
   --------------------------------
   ---------- IP options ----------
   --------------------------------
*/
/**
 * @defgroup lwip_opts_ipv4 IPv4
 * @ingroup lwip_opts
 * @{
 */
/**
 * LWIP_IPV4==1: Enable IPv4
 */

#define LWIP_IPV4                       1


/**
 * IP_FORWARD==1: Enables the ability to forward IP packets across network
 * interfaces. If you are going to run lwIP on a device with only one network
 * interface, define this to 0.
 */

//#define IP_FORWARD                      1
#define IP_FORWARD                      0

/**
 * IP_REASSEMBLY==1: Reassemble incoming fragmented IP packets. Note that
 * this option does not affect outgoing packet sizes, which can be controlled
 * via IP_FRAG.
 */

#define IP_REASSEMBLY                   1


/**
 * IP_FRAG==1: Fragment outgoing IP packets if their size exceeds MTU. Note
 * that this option does not affect incoming packet sizes, which can be
 * controlled via IP_REASSEMBLY.
 */

#define IP_FRAG                         0


/**
 * IP_OPTIONS_ALLOWED: Defines the behavior for IP options.
 *      IP_OPTIONS_ALLOWED==0: All packets with IP options are dropped.
 *      IP_OPTIONS_ALLOWED==1: IP options are allowed (but not parsed).
 */

#define IP_OPTIONS_ALLOWED              0


/**
 * IP_REASS_MAXAGE: Maximum time (in multiples of IP_TMR_INTERVAL - so seconds, normally)
 * a fragmented IP packet waits for all fragments to arrive. If not all fragments arrived
 * in this time, the whole packet is discarded.
 */

#define IP_REASS_MAXAGE                 1


/**
 * IP_REASS_MAX_PBUFS: Total maximum amount of pbufs waiting to be reassembled.
 * Since the received pbufs are enqueued, be sure to configure
 * PBUF_POOL_SIZE > IP_REASS_MAX_PBUFS so that the stack is still able to receive
 * packets even if the maximum amount of fragments is enqueued for reassembly!
 * When IPv4 *and* IPv6 are enabled, this even changes to
 * (PBUF_POOL_SIZE > 2 * IP_REASS_MAX_PBUFS)!
 */

#define IP_REASS_MAX_PBUFS              256


/*
   ----------------------------------
   ---------- ICMP options ----------
   ----------------------------------
*/


/*
   ---------------------------------
   ---------- RAW options ----------
   ---------------------------------
*/


/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/
/**
 * @defgroup lwip_opts_dhcp DHCP
 * @ingroup lwip_opts_ipv4
 * @{
 */
/**
 * LWIP_DHCP==1: Enable DHCP module.
 */
#define LWIP_DHCP                       0

/*
   ------------------------------------
   ---------- AUTOIP options ----------
   ------------------------------------
*/

/**
 * LWIP_AUTOIP==1: Enable AUTOIP module.
 */
#define LWIP_AUTOIP                     0


/*
   ----------------------------------
   ----- SNMP MIB2 support      -----
   ----------------------------------
*/

/**
 * LWIP_MIB2_CALLBACKS==1: Turn on SNMP MIB2 callbacks.
 * Turn this on to get callbacks needed to implement MIB2.
 * Usually MIB2_STATS should be enabled, too.
 */
#define LWIP_MIB2_CALLBACKS             0

/*
   ----------------------------------
   -------- Multicast options -------
   ----------------------------------
*/

/**
 * LWIP_MULTICAST_TX_OPTIONS==1: Enable multicast TX support like the socket options
 * IP_MULTICAST_TTL/IP_MULTICAST_IF/IP_MULTICAST_LOOP, as well as (currently only)
 * core support for the corresponding IPv6 options.
 */

#define LWIP_MULTICAST_TX_OPTIONS       1

/**
 * @}
 */

/*
   ----------------------------------
   ---------- IGMP options ----------
   ----------------------------------
*/

/**
 * LWIP_IGMP==1: Turn on IGMP module.
 */

#define LWIP_IGMP                       1


/*
   ----------------------------------
   ---------- DNS options -----------
   ----------------------------------
*/
/**
 * @defgroup lwip_opts_dns DNS
 * @ingroup lwip_opts_callback
 * @{
 */
/**
 * LWIP_DNS==1: Turn on DNS module. UDP must be available for DNS
 * transport.
 */

#define LWIP_DNS                        1


/*
   ---------------------------------
   ---------- UDP options ----------
   ---------------------------------
*/

/**
 * LWIP_UDP==1: Turn on UDP.
 */
#define LWIP_UDP                        1

/**
 * LWIP_UDPLITE==1: Turn on UDP-Lite. (Requires LWIP_UDP)
 */

#define LWIP_UDPLITE                    1



/**
 * LWIP_NETBUF_RECVINFO==1: append destination addr and port to every netbuf.
 */

#define LWIP_NETBUF_RECVINFO            1


/*
   ---------------------------------
   ---------- TCP options ----------
   ---------------------------------
*/

/**
 * LWIP_TCP==1: Turn on TCP.
 */

#define LWIP_TCP                        1

#define LWIP_TCP_SACK_OUT               0

/**
 * LWIP_TCP_MAX_SACK_NUM: The maximum number of SACK values to include in TCP segments.
 * Must be at least 1, but is only used if LWIP_TCP_SACK_OUT is enabled.
 * NOTE: Even though we never send more than 3 or 4 SACK ranges in a single segment
 * (depending on other options), setting this option to values greater than 4 is not pointless.
 * This is basically the max number of SACK ranges we want to keep track of.
 * As new data is delivered, some of the SACK ranges may be removed or merged.
 * In that case some of those older SACK ranges may be used again.
 * The amount of memory used to store SACK ranges is LWIP_TCP_MAX_SACK_NUM * 8 bytes for each TCP PCB.
 */

#define LWIP_TCP_MAX_SACK_NUM           1


/**
 * TCP_MSS: TCP Maximum segment size. (default is 536, a conservative default,
 * you might want to increase this.)
 * For the receive side, this MSS is advertised to the remote side
 * when opening a connection. For the transmit size, this MSS sets
 * an upper limit on the MSS advertised by the remote host.
 */

#define TCP_MSS                         (1536)

#define TCP_WND                         (32 * TCP_MSS)

/* #define TCP_SNDLOWAT                    (4 * TCP_MSS) */


/**
 * TCP_CALCULATE_EFF_SEND_MSS: "The maximum size of a segment that TCP really
 * sends, the 'effective send MSS,' MUST be the smaller of the send MSS (which
 * reflects the available reassembly buffer size at the remote host) and the
 * largest size permitted by the IP layer" (RFC 1122)
 * Setting this to 1 enables code that checks TCP_MSS against the MTU of the
 * netif used for a connection and limits the MSS if it would be too big otherwise.
 */
#if !defined TCP_CALCULATE_EFF_SEND_MSS || defined __DOXYGEN__
#define TCP_CALCULATE_EFF_SEND_MSS      1
#endif


/**
 * TCP_SND_BUF: TCP sender buffer space (bytes).
 * To achieve good performance, this should be at least 2 * TCP_MSS.
 */

#define TCP_SND_BUF                     (32 * TCP_MSS)


/**
 * TCP_OVERSIZE: The maximum number of bytes that tcp_write may
 * allocate ahead of time in an attempt to create shorter pbuf chains
 * for transmission. The meaningful range is 0 to TCP_MSS. Some
 * suggested values are:
 *
 * 0:         Disable oversized allocation. Each tcp_write() allocates a new
              pbuf (old behaviour).
 * 1:         Allocate size-aligned pbufs with minimal excess. Use this if your
 *            scatter-gather DMA requires aligned fragments.
 * 128:       Limit the pbuf/memory overhead to 20%.
 * TCP_MSS:   Try to create unfragmented TCP packets.
 * TCP_MSS/4: Try to create 4 fragments or less per TCP packet.
 */
#define TCP_OVERSIZE                    TCP_MSS


/**
 * LWIP_TCP_TIMESTAMPS==1: support the TCP timestamp option.
 * The timestamp option is currently only used to help remote hosts, it is not
 * really used locally. Therefore, it is only enabled when a TS option is
 * received in the initial SYN packet from a remote host.
 */

#define LWIP_TCP_TIMESTAMPS             0


/*
   ----------------------------------
   ---------- Pbuf options ----------
   ----------------------------------
*/


/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/
/**
 * @defgroup lwip_opts_netif NETIF
 * @ingroup lwip_opts
 * @{
 */
/**
 * LWIP_SINGLE_NETIF==1: use a single netif only. This is the common case for
 * small real-life targets. Some code like routing etc. can be left out.
 */

#define LWIP_SINGLE_NETIF               0


/**
 * LWIP_NETIF_API==1: Support netif api (in netifapi.c)
 */

#define LWIP_NETIF_API                  1


/**
 * LWIP_NETIF_TX_SINGLE_PBUF: if this is set to 1, lwIP *tries* to put all data
 * to be sent into one single pbuf. This is for compatibility with DMA-enabled
 * MACs that do not support scatter-gather.
 * Beware that this might involve CPU-memcpy before transmitting that would not
 * be needed without this flag! Use this only if you need to!
 *
 * ATTENTION: a driver should *NOT* rely on getting single pbufs but check TX
 * pbufs for being in one piece. If not, @ref pbuf_clone can be used to get
 * a single pbuf:
 *   if (p->next != NULL) {
 *     struct pbuf *q = pbuf_clone(PBUF_RAW, PBUF_RAM, p);
 *     if (q == NULL) {
 *       return ERR_MEM;
 *     }
 *     p = q; ATTENTION: do NOT free the old 'p' as the ref belongs to the caller!
 *   }
 */

#define LWIP_NETIF_TX_SINGLE_PBUF       1


/**
 * LWIP_NUM_NETIF_CLIENT_DATA: Number of clients that may store
 * data in client_data member array of struct netif (max. 256).
 */

#define LWIP_NUM_NETIF_CLIENT_DATA      16


/*
   ------------------------------------
   ---------- LOOPIF options ----------
   ------------------------------------
*/

/**
 * LWIP_NETIF_LOOPBACK_MULTITHREADING: Indicates whether threading is enabled in
 * the system, as netifs must change how they behave depending on this setting
 * for the LWIP_NETIF_LOOPBACK option to work.
 * Setting this is needed to avoid reentering non-reentrant functions like
 * tcp_input().
 *    LWIP_NETIF_LOOPBACK_MULTITHREADING==1: Indicates that the user is using a
 *       multithreaded environment like tcpip.c. In this case, netif->input()
 *       is called directly.
 *    LWIP_NETIF_LOOPBACK_MULTITHREADING==0: Indicates a polling (or NO_SYS) setup.
 *       The packets are put on a list and netif_poll() must be called in
 *       the main application loop.
 */

#define LWIP_NETIF_LOOPBACK_MULTITHREADING    1

/**
 * @}
 */

/*
   ------------------------------------
   ---------- Thread options ----------
   ------------------------------------
*/


/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/

/** 
 * LWIP_TCPIP_TIMEOUT==1: Enable tcpip_timeout/tcpip_untimeout to create
 * timers running in tcpip_thread from another thread.
 */

#define LWIP_TCPIP_TIMEOUT              0


/** LWIP_NETCONN_SEM_PER_THREAD==1: Use one (thread-local) semaphore per
 * thread calling socket/netconn functions instead of allocating one
 * semaphore per netconn (and per select etc.)
 * ATTENTION: a thread-local semaphore for API calls is needed:
 * - LWIP_NETCONN_THREAD_SEM_GET() returning a sys_sem_t*
 * - LWIP_NETCONN_THREAD_SEM_ALLOC() creating the semaphore
 * - LWIP_NETCONN_THREAD_SEM_FREE() freeing the semaphore
 * The latter 2 can be invoked up by calling netconn_thread_init()/netconn_thread_cleanup().
 * Ports may call these for threads created with sys_thread_new().
 */

#define LWIP_NETCONN_SEM_PER_THREAD     0


/** LWIP_NETCONN_FULLDUPLEX==1: Enable code that allows reading from one thread,
 * writing from a 2nd thread and closing from a 3rd thread at the same time.
 * ATTENTION: This is currently really alpha! Some requirements:
 * - LWIP_NETCONN_SEM_PER_THREAD==1 is required to use one socket/netconn from
 *   multiple threads at once
 * - sys_mbox_free() has to unblock receive tasks waiting on recvmbox/acceptmbox
 *   and prevent a task pending on this during/after deletion
 */

#define LWIP_NETCONN_FULLDUPLEX         1

/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/

/**
 * LWIP_SOCKET==1: Enable Socket API (require to use sockets.c)
 */

#define LWIP_SOCKET                     1


/**
 * LWIP_COMPAT_SOCKETS==1: Enable BSD-style sockets functions names through defines.
 * LWIP_COMPAT_SOCKETS==2: Same as ==1 but correctly named functions are created.
 * While this helps code completion, it might conflict with existing libraries.
 * (only used if you use sockets.c)
 */

#define LWIP_COMPAT_SOCKETS             0


/**
 * LWIP_POSIX_SOCKETS_IO_NAMES==1: Enable POSIX-style sockets functions names.
 * Disable this option if you use a POSIX operating system that uses the same
 * names (read, write & close). (only used if you use sockets.c)
 */

#define LWIP_POSIX_SOCKETS_IO_NAMES     0


/**
 * LWIP_TCP_KEEPALIVE==1: Enable TCP_KEEPIDLE, TCP_KEEPINTVL and TCP_KEEPCNT
 * options processing. Note that TCP_KEEPIDLE and TCP_KEEPINTVL have to be set
 * in seconds. (does not require sockets.c, and will affect tcp.c)
 */

#define LWIP_TCP_KEEPALIVE              0


/**
 * LWIP_SO_SNDTIMEO==1: Enable send timeout for sockets/netconns and
 * SO_SNDTIMEO processing.
 */

#define LWIP_SO_SNDTIMEO                0


/**
 * LWIP_SO_RCVTIMEO==1: Enable receive timeout for sockets/netconns and
 * SO_RCVTIMEO processing.
 */

#define LWIP_SO_RCVTIMEO                1


/**
 * LWIP_SO_SNDRCVTIMEO_NONSTANDARD==1: SO_RCVTIMEO/SO_SNDTIMEO take an int
 * (milliseconds, much like winsock does) instead of a struct timeval (default).
 */

#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD 0

/**
 * LWIP_SO_RCVBUF==1: Enable SO_RCVBUF processing.
 */
#define LWIP_SO_RCVBUF                  1


/**
 * LWIP_SO_LINGER==1: Enable SO_LINGER processing.
 */

#define LWIP_SO_LINGER                  1


/**
 * By default, TCP socket/netconn close waits 20 seconds max to send the FIN
 */

/* 300ms */

#define LWIP_TCP_CLOSE_TIMEOUT_MS_DEFAULT 3


/**
 * SO_REUSE==1: Enable SO_REUSEADDR option.
 */
#define SO_REUSE                        1


/**
 * LWIP_SOCKET_SELECT==1 (default): enable select() for sockets (uses a netconn
 * callback to keep track of events).
 * This saves RAM (counters per socket) and code (netconn event callback), which
 * should improve performance a bit).
 */

#define LWIP_SOCKET_SELECT              1


/**
 * LWIP_SOCKET_POLL==1 (default): enable poll() for sockets (including
 * struct pollfd, nfds_t, and constants)
 */

#define LWIP_SOCKET_POLL                1

/**
 * LWIP_SOCKET_OFFSET==n: Increases the file descriptor number created by LwIP with n.
 * This can be useful when there are multiple APIs which create file descriptors.
 * When they all start with a different offset and you won't make them overlap you can
 * re implement read/write/close/ioctl/fnctl to send the requested action to the right
 * library (sharing select will need more work though).
 */

#define LWIP_SOCKET_OFFSET              100


/**
 * @}
 */

/*
   ----------------------------------------
   ---------- Statistics options ----------
   ----------------------------------------
*/
/**
 * @defgroup lwip_opts_stats Statistics
 * @ingroup lwip_opts_debug
 * @{
 */
/**
 * LWIP_STATS==1: Enable statistics collection in lwip_stats.
 */

#define LWIP_STATS                      0


/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/


/*
   ---------------------------------------
   ---------- IPv6 options ---------------
   ---------------------------------------
*/

/**
 * LWIP_IPV6==1: Enable IPv6
 */

#define LWIP_IPV6                       0


/*
   ---------------------------------------
   ---------- Hook options ---------------
   ---------------------------------------
*/

/**
 * @defgroup lwip_opts_hooks Hooks
 * @ingroup lwip_opts_infrastructure
 * Hooks are undefined by default, define them to a function if you need them.
 * @{
 */

/**
 * LWIP_HOOK_FILENAME: Custom filename to \#include in files that provide hooks.
 * Declare your hook function prototypes in there, you may also \#include all headers
 * providing data types that are need in this file.
 */
#define LWIP_HOOK_FILENAME "lwiphooks.h"
#define LWIP_HOOK_IP4_ROUTE_SRC(SRC, DST) eka_route_ipv4_src(SRC, DST)


/*
   ---------------------------------------
   ---------- Debugging options ----------
   ---------------------------------------
*/
/**
 * @defgroup lwip_opts_debugmsg Debug messages
 * @ingroup lwip_opts_debug
 * @{
 */
/**
 * LWIP_DBG_MIN_LEVEL: After masking, the value of the debug is
 * compared against this value. If it is smaller, then debugging
 * messages are written.
 * @see debugging_levels
 */

#define LWIP_DEBUG                      1
/* #define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL */

/* #define ETHARP_DEBUG                    LWIP_DBG_ON */

#if 0
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL

#define TCP_INPUT_DEBUG                 LWIP_DBG_ON
#define TCP_OUTPUT_DEBUG                LWIP_DBG_ON
#define TIMERS_DEBUG                    LWIP_DBG_ON


#define API_LIB_DEBUG                   LWIP_DBG_OFF
#define NETIF_DEBUG                     LWIP_DBG_OFF
#define API_MSG_DEBUG                   LWIP_DBG_OFF

#define SOCKETS_DEBUG                   LWIP_DBG_ON
#define ETHARP_DEBUG                    LWIP_DBG_OFF

#define INET_DEBUG                      LWIP_DBG_OFF
#define IP_DEBUG                        LWIP_DBG_OFF
#define SYS_DEBUG                       LWIP_DBG_ON
#endif

#if 0
#define TCP_INPUT_DEBUG                 LWIP_DBG_ON
#define TCP_OUTPUT_DEBUG                LWIP_DBG_ON

#define TCPIP_DEBUG                     LWIP_DBG_ON
#define TCP_RTO_DEBUG                   LWIP_DBG_ON
#define TCP_CWND_DEBUG                  LWIP_DBG_ON
#define TCP_WND_DEBUG                   LWIP_DBG_ON
#define TCP_QLEN_DEBUG                  LWIP_DBG_ON
#define TIMERS_DEBUG                    LWIP_DBG_ON
#define TCP_DEBUG                       LWIP_DBG_ON

#define TIMERS_DEBUG                    LWIP_DBG_ON

#endif

/*
   --------------------------------------------------
   ---------- Performance tracking options ----------
   --------------------------------------------------
*/

#endif /* _EKALINE_LWIPOPT_H */

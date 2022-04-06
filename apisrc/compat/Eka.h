/*
 * Eka.h
 *      This is the header file for some common functionality between the
 *      various Eka APIs. It also contains forward declarations from the
 *      various Eka APIs to allow users to work with the public Eka types in
 *      their own headers without publically exposing the full API.
 *
 * As general api policy, all ekaline functions should kill the application if we break an api invariant
 * (such as calling a function at the wrong time.  See comments for the api for individual invariants
 * about calling each function.)
 */

#ifndef __EKA_H__
#define __EKA_H__

#include <stddef.h>
#include <stdint.h>

/* #include <net/ethernet.h> */
/* #include <netinet/in.h> */
 
#include "EkaMacro.h"
#include "EkaCtxs.h"

extern "C" {

  /* ****************************************
   * Caps API
   * ****************************************/
  /**
   * These enums each represent a queryable feature of the current implementation of Eka.
   */
  enum EkaCapType {
    kEkaCapsMaxEkaHandle  = 0,      /** This is the max value that an EkaSecCtxHandle can hold. */
    kEkaCapsExchange,               /** This returns an EfhExchange value. */
    kEkaCapsMaxSecCtxs,
    kEkaCapsMaxSesCtxs,
    kEkaCapsMaxCores,
    kEkaCapsMaxSessPerCore,
    kEkaCapsNumWriteChans,          /** This is the number of channels that can be used to write
                                        to the card (used for setting Efc ctxs). */
    kEkaCapsSendWarmupFlag,         /** If this value is passed to excSend()'s flag parameter, then
                                        the packet should not actually be put on the wire, but
                                        instead all of the software code possible should run to
                                        warm the cache.
                                        $$NOTE$$ - Make sure this doesnt use an existing linux flag...
					or better yet, replace a flag we wont be using. */
  };

  typedef uint64_t EkaCapsResult;

  struct EkaDev;

  /**
   * This will return queried caps value.
   */
  EkaCapsResult ekaGetCapsResult(EkaDev* pEkaDev, enum EkaCapType ekaCapType );
  /* ****************************************/
 
  /* ****************************************
   * Use EkaProps to pass arbitrary string properties to ekaline.
   * ****************************************/
  struct EkaProp {
    const char* szKey;
    const char* szVal;
  };

  struct EkaProps {
    size_t numProps;
    EkaProp* props;
  };

  enum EkaOpResult : int {
    EKA_OPRESULT__OK    = 0,        /** General success message */
      EKA_OPRESULT__ALREADY_INITIALIZED = 1,
      EKA_OPRESULT__END_OF_SESSION = 2,
      EKA_OPRESULT__ERR_A = -100,     /** Temporary filler error message.  Replace 'A' with actual error code. */
      EKA_OPRESULT__ERR_DOUBLE_SUBSCRIPTION = -101,     // returned by efhSubscribeStatic() if trying to subscribe to same security again
      EKA_OPRESULT__ERR_BAD_ADDRESS = -102,     // returned if you pass NULL for something that can't be NULL, similar to EFAULT
      EKA_OPRESULT__ERR_SYSTEM_ERROR = -103,     // returned when a system call fails and errno is set
      EKA_OPRESULT__ERR_NOT_IMPLEMENTED = -104,     // returned when an API call is not implemented
      EKA_OPRESULT__ERR_GROUP_NOT_AVAILABLE = -105, // returned by test feed handler when group not present in capture
      EKA_OPRESULT__ERR_EXCHANGE_RETRANSMIT_CONNECTION = -106, // returned if exchange retransmit connection failed
      EKA_OPRESULT__ERR_EFC_SET_CTX_ON_UNSUBSCRIBED_SECURITY = -107,
      EKA_OPRESULT__ERR_STRIKE_PRICE_OVERFLOW = -108,
    
      // EPM specific
      EKA_OPRESULT__ERR_EPM_DISABLED = -201,
      EKA_OPRESULT__ERR_INVALID_CORE = -202,
      EKA_OPRESULT__ERR_EPM_UNINITALIZED = -203,
      EKA_OPRESULT__ERR_INVALID_STRATEGY = -204,
      EKA_OPRESULT__ERR_INVALID_ACTION = -205,
      EKA_OPRESULT__ERR_NOT_CONNECTED = -206,
      EKA_OPRESULT__ERR_INVALID_OFFSET = -207,
      EKA_OPRESULT__ERR_INVALID_ALIGN = -208,
      EKA_OPRESULT__ERR_INVALID_LENGTH = -209,
      EKA_OPRESULT__ERR_UNKNOWN_FLAG = -210,
      EKA_OPRESULT__ERR_MAX_STRATEGIES = -211,

      
      // EFC specific
      EKA_OPRESULT__ERR_EFC_DISABLED = -301,
      EKA_OPRESULT__ERR_EFC_UNINITALIZED = -302,

      // EFH recovery specific
      EKA_OPRESULT__RECOVERY_IN_PROGRESS = -400, 
      EKA_OPRESULT__ERR_RECOVERY_FAILED  = -401,

      // EFH TCP/Protocol Handshake specific
      EKA_OPRESULT__ERR_TCP_SOCKET  = -501,
      EKA_OPRESULT__ERR_UDP_SOCKET  = -502,
      EKA_OPRESULT__ERR_PROTOCOL  = -503,

      };

  struct ExcCtx;
  struct EkaCoreInitAttrs;

/*
 * Forward declarations of the other API types so they can be used opaquely
 * in downstream headers without exposing the API.
 */
 struct EfhCtx;
 struct EfhRunCtx;
 struct EfcCtx;

/* This will be returned to us with every marketdata update.  It's value will be
 * specified in efhSubscribe(). */
 using EfhSecUserData = uintptr_t;

/* This will be passed to efhRun() and returned with each callback. */
 using EfhRunUserData = uintptr_t;

/** Positive values indicate success, negative indicate an error. */
 using ExcConnHandle = int16_t;

  enum class EkaSource : uint8_t {
       #define EkaSource_ENUM_ITER( _x )		\
             _x( Invalid,  0 )				\
             _x( NOM_ITTO    )				\
             _x( GEM_TQF     )				\
             _x( ISE_TQF     )				\
             _x( MRX_TQF     )				\
             _x( PHLX_TOPO   )				\
             _x( PHLX_ORD    )				\
             _x( MIAX_TOM    )				\
             _x( EMLD_TOM    )				\
             _x( PEARL_TOM   )				\
             _x( ARCA_XDP    )				\
             _x( AMEX_XDP    )				\
             _x( ARCA_PLR    )				\
             _x( AMEX_PLR    )				\
             _x( EDGX_PITCH  )				\
             _x( BZX_PITCH   )				\
             _x( C1_PITCH    )				\
             _x( C2_PITCH    )				\
             _x( BOX_HSVF    )				\
             _x( CME_SBE    )
            EkaSource_ENUM_ITER( EKA__ENUM_DEF )
  };

  typedef int8_t EkaLSI;

  /** @brief Descriptor for a single multicast traffic flow provided by one
   * exchange operator, using a particular market data protocol.
   *
   * Consider the following scenario, listening to NOM and MIAX market data:
   *
   *   NASDAQ Options Market (NOM) using ITTO        MIAX using TOM
   *
   *   MCAST0  MCAST1  MCAST2  MCAST3                MCAST4  MCAST5 ...many more
   *     ||      ||      ||      ||                    ||      ||
   *     VV      VV      VV      VV                    VV      VV
   *
   * Each distinct network flow above is called a "group", and is represented
   * in code by the EkaGroup structure. A group is combination of a source
   * enumeration constant (e.g., NOM_ITTO or MIAX_TOM above) which describes
   * the operator doing the publishing together with the protocol that defines
   * the packet content, and a "local source id" (or "LSI"), a numeric id
   * (starting at zero) that identifies a network flow within a set of flows
   * used by that operator. For example, MCAST0 above is { NOM_ITTO, 0 } and
   * MCAST4 is { MIAX_TOM, 0 }.
   *
   * EkaLSI is a type alias for a signed integer (currently an int32_t) in case
   * it later makes sense to use negative numbers for special values, e.g., -1
   * could mean "all local ids", should the situation arise where that is needed
   * at the API level. Currently a negative id is an error.
   *
   * NOTE: techically an MCAST-N "group" above represents a single *socket
   * address*. A related idea is a "multicast group", an IPv4 concept that
   * exists only at layer 3 (the IP protocol), and may carry multiple
   * independent network flows on different ports. By contrast, we call the full
   * socket address (one specific network flow) a "group". It is related to, but
   * entirely the same as, an IPv4 multicast group.
   */

  struct EkaGroup {
    EkaSource source;
    EkaLSI localId;
  };


  inline bool operator==(const EkaGroup &lhs, const EkaGroup &rhs) noexcept {
    return lhs.source == rhs.source && lhs.localId == rhs.localId;
  }

  inline bool operator!=(const EkaGroup &lhs, const EkaGroup &rhs) noexcept {
    return !operator==(lhs, rhs);
  }


/****************************************
 * Ekaline Core Types and Functions
 ****************************************/

/** Only positive values should be used except for special cases like efcEnableControllerexcept for special(). */
typedef int8_t EkaCoreId;

/* typedef int (*EkaLogCallback) (void* ctx, const char* function, const char* file, */
/*                                int line, int priority, const char* format, ...); */

typedef int (*EkaLogCallback) (void* ctx, const char* function, const char* file,
                               int line, int priority, const char* format, ...)
                               __attribute__ ((format (printf, 6, 7)));

  struct eka_ether_addr { // replicated to avoid #include <net/ethernet.h>
    uint8_t ether_addr_octet[6];
  } __attribute__ ((__packed__));

  typedef uint32_t eka_in_addr_t; // replicated to avoid #include <netinet/in.h>

struct EkaCoreInitAttrs {
    #define EkaCoreInitAttrs_FIELD_ITER( _x )                                  \
                _x( eka_in_addr_t,  host_ip )                                      \
                _x( eka_in_addr_t,  netmask )                                      \
                _x( eka_in_addr_t,  gateway )				\
                _x( eka_ether_addr, nexthop_mac )                                  \
                _x( eka_ether_addr, src_mac_addr)                                  \
                _x( uint8_t,        dont_garp  )
        EkaCoreInitAttrs_FIELD_ITER( EKA__FIELD_DEF )
};
 
enum class EkaCredentialType {
  #define EkaCredentialType_ENUM_ITER( _x )	\
      _x( Unknown, 0 )       \
      _x( MarketData )       \
      _x( Snapshot   )       \
      _x( Recovery   )
  EkaCredentialType_ENUM_ITER( EKA__ENUM_DEF )
};


struct EkaCoreInitCtx {
    #define EkaCoreInitCtx_FIELD_ITER( _x )                                             \
                /** coreId This is the id of the core that we will be initializing. */  \
                _x( EkaCoreId,       coreId )                                           \
                _x( EkaCoreInitAttrs,    attrs )
        EkaCoreInitCtx_FIELD_ITER( EKA__FIELD_DEF )
};

struct EkaCredentialLease;

typedef int (*EkaAcquireCredentialsFn)(EkaCredentialType credType,
					 EkaGroup group,
					 const char *user,
					 const struct timespec *leaseTime,
					 const struct timespec *timeout,
					 void *context,
					 EkaCredentialLease **lease);

typedef int (*EkaReleaseCredentialsFn)(EkaCredentialLease *lease, void* context);

enum class EkaServiceType : uint8_t {
  #define EkaServiceType_ENUM_ITER(_x)       \
    _x ( Unspecified, 0 )                   \
    _x ( FeedSnapshot )                     \
    _x ( FeedRecovery )                     \
    _x ( IGMP )                             \
    _x ( PacketIO )                         \
    _x ( LiveMarketData )                   \
    _x ( Heartbeat )
  EkaServiceType_ENUM_ITER( EKA__ENUM_DEF )
};

typedef int (*EkaThreadCreateFn)(const char* name, EkaServiceType type, void *(*start_routine)(void*),void *arg, void *context, uintptr_t *handle);

struct EkaDevInitCtx {
    #define EkaDevInitCtx_FIELD_ITER( _x )                              \
                _x( EkaLogCallback,                    logCallback )    \
                _x( void*,                             logContext  )	\
                _x( EkaAcquireCredentialsFn,           credAcquire )	\
                _x( EkaReleaseCredentialsFn,           credRelease )	\
                _x( void*,                             credContext )	\
                _x( EkaThreadCreateFn,                 createThread )	\
                _x( void*,                             createThreadContext )
        EkaDevInitCtx_FIELD_ITER( EKA__FIELD_DEF )
};

/**
 * This will initialize an Ekaline device.  All calls to this must be made
 * before any other initialization function, e.g., efhInit, efcInit, etc.
 *
 * @param initCtx This is a pointer to the initialization parameter object.
 * @retval [See EkaOpResult].
 */
EkaOpResult ekaDevInit( EkaDev** ppEkaDev, const EkaDevInitCtx *pInitCtx );

/**
 * This will shut down a device that was initialized via an earlier call to
 * ekaDevInit.
 *
 * @retval [See EkaOpResult].
 */
EkaOpResult ekaDevClose( EkaDev* pEkaDev );

/**
 * Configure a port on a device previously opened via @ref ekaDevInit.
 *
 * Ports must be configured prior to being used by a service init function,
 * e.g., specifying the port in efhInit.
 *
 * @retval [See EkaOpResult].
 */
EkaOpResult ekaDevConfigurePort( EkaDev *pEkaDev, const EkaCoreInitCtx *pCoreInit );
 
/*
 * Values for these must always be >= 0.  If the api returns a negative value, it should
 * be interpreted as an error EkaOpResult.
 */
typedef int64_t EfcSecCtxHandle;
typedef int64_t EfcSesCtxHandle;

} // End of extern "C"

#endif // __EKA_H__

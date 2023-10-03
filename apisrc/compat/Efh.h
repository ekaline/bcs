/*
 * EFh.h
 *     This header file covers the api for the ekaline feedhandler
 */

#ifndef __EKALINE_EFH_H__
#define __EKALINE_EFH_H__

#include "Eka.h"
#include "EkaMsgs.h"
#include "EfhMsgs.h"

extern "C" {

enum class EfhFeedVer {
    #define EfhFeedVer_ENUM_ITER( _x )                  \
           _x( Invalid,       0 )			\
           _x( NASDAQ           )			\
	   _x( MIAX             )			\
	   _x( PHLX             )			\
	   _x( GEMX             )			\
	   _x( BATS             )			\
	   _x( XDP              )			\
	   _x( PLR              )			\
	   _x( BOX              )			\
	   _x( CME              )			\
	   _x( QED              )			\
	   _x( NEWS             )			\
	   _x( CBOE             )                       \
	   _x( ITCHFS           )                       \
	   _x( MRX2_TOP         )                       \
	   _x( BX               )                                
	 EfhFeedVer_ENUM_ITER( EKA__ENUM_DEF )
};

typedef int (*EfhGetTradeTimeFn)(const EfhDateComponents *, uint32_t* iso8601Date,
                                 time_t *epochTime, void* ctx);

/**
 * This is passed to EfhInit().  This will replace the eka_conf values that we passed in the old api.  
 * This function must be called before efcInit().
 */
  struct EfhInitCtx {
    EkaProps* ekaProps;
    size_t numOfGroups;

    EkaCoreId coreId; // what port to work on
    EfhGetTradeTimeFn getTradeTime; // Used to map a CME-style expiration to a trading date/time
    void* getTradeTimeCtx;          // Context object passed into getTradeTime
    bool printParsedMessages;       // used for Ekaline internal testing

    /* This is true if we expect to receive marketdata updates when we run efhRun().  If this is false, 
     * we dont expect updates, and so Ekaline can save memory and avoid creating structures that arent needed.  */
    bool recvSoftwareMd;

/* Used for Ekaline lab tests only */
    bool subscribe_all; 

    /* This is true if we expect to receive raw relevant market data for EFC testing.
     * noTob = false for regular EFH use   */
    bool noTob = false;
  };

/**
 * This will initialize the Ekaline feedhandler.
 *
 * @oaram ppEfhCtx This is an output parameter that will contain an initialized EfhCtx* if this
 *                 function is successful.
 * @param ekaDev   EkaDev of the device used to host the feed handler
 * @param initCtx  This is a list of key value parameters that replaces the old eka_conf values 
 *                 that were passed in the old api.
  * @return        This will return an appropriate EkalineOpResult indicating success or an error code.
 */
EkaOpResult efhInit( EfhCtx** ppEfhCtx, EkaDev* ekaDev, const EfhInitCtx* initCtx );

/**
 * This will return the supported parameters that are supported in this implementation.
 * they szKey values in pEntries are the names of the supported keys, and szValue
 * will contain some documentation about each parameter.
 *
 * @return This will return a filled out EfhInitCtx.
 */
const EfhInitCtx* efhGetSupportedParams( );

/**
 * This function will play back all EfhOptionDefinitionMsg to our callback from EfhRunCtx.  This
 * function will block until the last callback has returned.
 *
 * @param efhCtx 
 * @retval [See EkaOpResult].
 */
EkaOpResult efhGetDefs( EfhCtx* efhCtx, const struct EfhRunCtx* efhRunCtx, const EkaGroup* group, void** retval );

/**
 * This function will tell the Ekaline feedhandler that we are interested in updates for a specific static
 * security.  This function must be called before efhDoneStaticSubscriptions().
 *
 * @param efhCtx 
 * @param securityId      This is the exchange specific value of the security.  
 *                        $$TODO$$ - Should this be an arbitrary handle?
 * @param efhSecurityType This is a type of EfhSecurityType.
 * @param efhUserData     This is the value that we want returned to us with every
 *                        marketdata update for this security.
 * @retval [See EkaOpResult].
 */
/* EkaOpResult efhSubscribeStatic( EfhCtx*         efhCtx, */
/*                                 EkaGroup*       group, */
/*                                 uint64_t        securityId,  */
/*                                 EfhSecurityType efhSecurityType, */
/*                                 EfhSecUserData  efhSecUserData ); */

EkaOpResult efhSubscribeStatic( EfhCtx*         efhCtx,
                                const EkaGroup* group,
                                uint64_t        securityId,
                                EfhSecurityType efhSecurityType,
                                EfhSecUserData  efhSecUserData,
				uint64_t        opaqueAttrA,
				uint64_t        opaqueAttrB );

/**
 * This function is not needed for EFH!!!
 * Call this function when you are done with all your efhSubscribeStatic() calls.
 */
EkaOpResult efhDoneStaticSubscriptions( EfhCtx* efhCtx );

EkaOpResult efhSubscribeDynamic(EfhCtx*         efhCtx,
                                const EkaGroup* group,
                                uint64_t        securityId,
                                EfhSecurityType efhSecurityType,
                                EfhSecUserData  efhSecUserData,
				uint64_t        opaqueAttrA,
				uint64_t        opaqueAttrB );

EkaOpResult efhSetTradeTimeCtx( EfhCtx* efhCtx,
                                void*   tradeTimeCtx );

/**
 * EfhMd functionality allows to get callback on every raw market data event
 *
 */
enum class EfhMdType : uint16_t {
  Invalid       = 0,
    Generic,
    Time,
    Definition,
    NewOrder,
    NewQuote,
    ModifyOrder,
    ModifyQuote,
    ReplaceOrder,
    ReplaceQuote,
    DeleteOrder,
    DeleteQuote,
    NewPlevel,
    ChangePlevel,
    DeletePlevel,
    };

#define DecodeMdType(x)					\
  x == EfhMdType::Invalid        ? "Invalid"      :	\
    x == EfhMdType::Generic      ? "Generic"      :	\
    x == EfhMdType::Time         ? "Time"         :	\
    x == EfhMdType::Definition   ? "Definition"   :	\
    x == EfhMdType::NewOrder     ? "NewOrder"     :	\
    x == EfhMdType::NewQuote     ? "NewQuote"     :	\
    x == EfhMdType::ModifyOrder  ? "ModifyOrder"  :	\
    x == EfhMdType::ModifyQuote  ? "ModifyQuote"  :	\
    x == EfhMdType::ReplaceOrder ? "ReplaceOrder" :	\
    x == EfhMdType::ReplaceQuote ? "ReplaceQuote" :	\
    x == EfhMdType::DeleteOrder  ? "DeleteOrder"  :	\
    x == EfhMdType::DeleteQuote  ? "DeleteQuote"  :	\
    x == EfhMdType::NewPlevel    ? "NewPlevel"    :	\
    x == EfhMdType::ChangePlevel ? "ChangePlevel" :	\
    x == EfhMdType::DeletePlevel ? "DeletePlevel" :	\
    "UNKNOWN"
    
typedef struct {
  EfhMdType mdMsgType;
  uint16_t  mdRawMsgType;
  EkaGroup  group;
  uint64_t  securityId;
  uint64_t  sequenceNumber;
  uint64_t  timeStamp;
  uint16_t  mdMsgSize; // not including size of EfhMdHeader
} EfhMdHeader;

typedef struct {
  EfhMdHeader  hdr;
  uint64_t     attr;    // raw attribute if relevant
  uint64_t     orderId; // if relevant
  EfhOrderSide side;
  uint64_t     price;
  uint32_t     size;
} MdNewOrder;

typedef struct {
  EfhMdHeader  hdr;
  uint8_t      pLvl;
  EfhOrderSide side;
  uint64_t     price;
  uint32_t     size;
} MdNewPlevel;

typedef struct {
  EfhMdHeader  hdr;
  uint8_t      pLvl;
  EfhOrderSide side;
  uint64_t     price;
  uint32_t     size;
} MdChangePlevel;

typedef struct {
  EfhMdHeader  hdr;
  uint8_t      pLvl;
  EfhOrderSide side;
} MdDeletePlevel;
  
typedef void* (*onEfhMdMsgCb) (const EfhMdHeader* msg, EfhRunUserData efhRunUserData);

  
/*
 *
 */
struct EfhRunCtx {
    const EkaGroup* groups;
    size_t  numGroups;

    EfhRunUserData efhRunUserData;

    /**************************************
     * Callbacks.
     **************************************/
    #define _DECL_CALLBACK( _msg, ... )                                         \
                void *                                                          \
                ( EKA__DELAYED_CAT3( *onEfh, _msg, MsgCb ) )                    \
                (                                                               \
                    const EKA__DELAYED_CAT3( Efh, _msg, Msg )* msg,             \
                    EfhSecUserData                             efhSecUserData,  \
                    EfhRunUserData                             efhRunUserData   \
                );
        EfhMsgType_ENUM_ITER( _DECL_CALLBACK )
       
        void ( *onEkaExceptionReportCb )( EkaExceptionReport* msg, 
                                          EfhRunUserData      efhRunUserData );
    #undef _DECL_CALLBACK

  onEfhMdMsgCb onEfhMdCb;
    /**************************************/
};

/**
 * This function will run the ekaline Fh on the current thread and make callbacks for messages processed.
 * This function should not return until we shut the Ekaline system down via ekaClose().
 *
 * @param pEfhCtx 
 * @param pEfhRunCtx This is a pointer to the callbacks (and possibly other information needed) that
 *                   will be called as the Ekaline feedhandler processes messages.
 * @retval [See EkaOpResult].
 */
EkaOpResult efhRunGroups( EfhCtx* efhCtx, const EfhRunCtx* efhRunCtx, void** retval );

/**
 * This function will stop all internal loops in the ekaline Fh.
 * Usually usefull in the INThandlers for graceful app termination. It does not replace ekaClose().
 *
 * @param pEfhCtx 

 * @retval [See EkaOpResult].
 */
EkaOpResult efhStopGroups( EfhCtx* efhCtx );

/**
 * This function prints out state of internal resources currently used by FH Group
 * Used for tests and diagnostics only
 */

  EkaOpResult efhMonitorFhGroupState( EfhCtx* efhCtx, EkaGroup* group, bool verbose=false );

/**
 * This will destroy an Ekaline feedhandler created with efhInit.
 *
 * @oaram efhCtx  An initialized EfhCtx* obtained from efhInit.
 * @return        This will return an appropriate EkalineOpResult indicating success or an error code.
 */
EkaOpResult efhDestroy( EfhCtx* pEfhCtx );



  
} // End of extern "C"

#endif //  __EKALINE_EFH_H__

/*
 * Exc.h
 *     This header file covers the api for the Ekaline socket implementation.
 */

#ifndef __EKALINE_EXC_H__
#define __EKALINE_EXC_H__

#include <unistd.h>

#include "Eka.h"

#ifdef __cplusplus
    extern "C" {
#endif

/** This is the value passed to set_group_session. */
typedef int8_t  ExcSessionId;

/** Positive values indicate success, negative indicate an error. */
typedef int32_t ExcSocketHandle;

/** Positive values indicate success, negative indicate an error. */
typedef int16_t ExcConnHandle;

/**
 * This is a utility function that will return the ExcSessionId from the result of exc_connect.
 *
 * @param hConn This is the value returned from exc_connect()
 * @return This will return the ExcSessionId value that corresponds to excSessionId.
 */
ExcSessionId excGetSessionId( ExcConnHandle hConn );

/**
 * This is a utility function that will return the coreId from the result of exc_connect.
 *
 * @param hConn This is the value returned from exc_connect()
 * @return This will return the ExcCoreid that corresponds to excSessionId.
 */
EkaCoreId excGetCoreId( ExcConnHandle hConn );

/*
 *
 */
ExcSocketHandle excSocket( EkaDev* pEkaDev, EkaCoreId coreId , int domain, int type, int protocol );

/*
 *
 */
ExcConnHandle excConnect( EkaDev* pEkaDev, ExcSocketHandle hSocket, const struct sockaddr *dst, socklen_t addrlen );

/**
 * This will be called when we need to reconnect to a socket that has already been disconnected.
 * Behavior is undefined if the firing controller is set up to fire on this socket when this is
 * called.
 */
ExcConnHandle excReconnect( EkaDev* pEkaDev, ExcConnHandle hConn );

/**
 * $$NOTE$$ This is mutexed to handle single session at a time.
 *
 * @param hConn
 * @param buffer
 * @param size
 * @param flag    This should either be a standard linux flag, or the result of
 *                ekaGetCapsResult( kEkaCapsSendWarmupFlag ).  If it's the latter,
 *                then the packet shouldnt actually go on the wire, but the software
 *                path should be warmed up.
 * @return This will return the values that exhibit the same behavior of linux's send fn.
 */
ssize_t excSend( EkaDev* pEkaDev, ExcConnHandle hConn, const void* buffer, size_t size );

/**
 * $$NOTE$$ - This is mutexed to handle single session at a time.
 */
ssize_t excRecv( EkaDev* pEkaDev, ExcConnHandle hConn, void *buffer, size_t size );

/*
 *
 */
int excClose( EkaDev* pEkaDev, ExcConnHandle hConn );


/**
 * @param hConn
 * @return This will return true if hConn has data ready to be read.
 */
int excReadyToRecv( EkaDev* pEkaDev, ExcConnHandle hConn );


/**
 * Help functions for the tests
 */

int excUdpSocket(EkaDev* pEkaDev);
int excSendTo (EkaDev* dev, int sock, const void *dataptr, size_t size, const struct sockaddr *to);

#ifdef __cplusplus
    } // extern "C"
#endif

#endif // __EKALINE_EXC_H__


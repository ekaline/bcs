/*
 * Exc.h
 *     This header file covers the api for the Ekaline socket implementation.
 */

#ifndef __EKALINE_EXC_H__
#define __EKALINE_EXC_H__

#include <unistd.h>

#include "Eka.h"

struct pollfd;

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

/**
 * Close an embryonic socket that did not succesfully connect.
 */
int excSocketClose( EkaDev* pEkaDev, ExcSocketHandle hSocket );

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
ssize_t excSend( EkaDev* pEkaDev, ExcConnHandle hConn, const void* buffer, size_t size, int flags );

/**
 * $$NOTE$$ - This is mutexed to handle single session at a time.
 */
ssize_t excRecv( EkaDev* pEkaDev, ExcConnHandle hConn, void *buffer, size_t size, int flags );

/*
 *
 */
int excClose( EkaDev* pEkaDev, ExcConnHandle hConn );

int excPoll( EkaDev* pEkaDev, struct pollfd *fds, int nfds, int timeout );

int excGetSockOpt( EkaDev* pEkaDev, ExcSocketHandle hSock, int level, int optname,
                   void* optval, socklen_t* optlen );

int excSetSockOpt( EkaDev* pEkaDev, ExcSocketHandle hSock, int level, int optname,
                   const void* optval, socklen_t optlen );

int excIoctl( EkaDev* pEkaDev, ExcSocketHandle hSock, long cmd, void *argp );

int excGetSockName( EkaDev* pEkaDev, ExcSocketHandle hSock, sockaddr *addr,
                    socklen_t *addrlen );

int excGetPeerName( EkaDev* pEkaDev, ExcSocketHandle hSock, sockaddr *addr,
                    socklen_t *addrlen );

// 0 -> non-blocking, 1 -> blocking, -1 -> errno is set
int excGetBlocking( EkaDev* pEkaDev, ExcSocketHandle hConn );

int excSetBlocking( EkaDev* pEkaDev, ExcSocketHandle hConn, bool blocking );

int excShutdown( EkaDev* pEkaDev, ExcConnHandle hConn, int how );

/**
 * Help functions for the tests
 */

int excUdpSocket(EkaDev* pEkaDev);
int excSendTo (EkaDev* dev, int sock, const void *dataptr, size_t size, const struct sockaddr *to);

#ifdef __cplusplus
    } // extern "C"
#endif

#endif // __EKALINE_EXC_H__


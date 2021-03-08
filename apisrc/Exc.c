#include <alloca.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <errno.h>
#include <thread>
#include <sys/poll.h>

#include "Efc.h"

//#include "eka_data_structs.h"
#include "EkaDev.h"
#include "EkaSnDev.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"
#include "EkaEfc.h"

// Coped from <lwip/sockets.h>, needed to get the LWIP definition of FIONREAD.
#define LWIP_IOCPARM_MASK    0x7fU           /* parameters must be < 128 bytes */
#define LWIP_IOC_OUT         0x40000000UL    /* copy out parameters */
#define LWIP__IOR(x,y,t)     ((long)(LWIP_IOC_OUT|((sizeof(t)&LWIP_IOCPARM_MASK)<<16)|((x)<<8)|(y)))

#define FIONREAD_LWIP LWIP__IOR('f', 127, unsigned long) /* get # bytes to read */

extern "C" {

int lwip_poll(struct pollfd* fds, int nfds, int timeout);
int lwip_getsockopt(int fd, int level, int optname, void* optval, socklen_t *optlen);
int lwip_setsockopt(int fd, int level, int optname, const void* optval, socklen_t optlen);
int lwip_ioctl(int fd, long cmd, void *argp);
int lwip_getsockname(int fd, sockaddr *, socklen_t *);
int lwip_getpeername(int fd, sockaddr *, socklen_t *);
int lwip_fcntl(int fd, int cmd, int val);
int lwip_shutdown(int fd, int how);

}

// Linux and lwIP number their socket options differently. In general the
// levels are the same, except for SOL_SOCKET.
// NOTE: you *cannot* include <lwip/socket.h> here or it will re#define
// all the important macros. If lwIP is ever upgraded, this mapping
// must be kept in sync by hand.
constexpr int mapLinuxSocketOptionLevelToLWIP(int linuxLevel) {
  return linuxLevel == SOL_SOCKET ? 0xfff : linuxLevel;
}

// Linux and lwIP use very different numbering for option names. We only
// map support options that we care about.
constexpr int mapLinuxSocketOptionNameToLWIP(int linuxLevel, int linuxName) {
  switch (linuxLevel) {
  case SOL_SOCKET:
    switch (linuxName) {
    case SO_LINGER: return 0x0080;
    case SO_RCVBUF: return 0x1002;
    case SO_SNDTIMEO: return 0x1005;
    case SO_RCVTIMEO: return 0x1006;
    case SO_ERROR: return 0x1007;
    case SO_TYPE: return 0x1008;
    case SO_BINDTODEVICE: return 0x100b;
    default: return -1;
    }

  case IPPROTO_TCP:
    switch (linuxName) {
    case TCP_NODELAY: return 0x01;
    case TCP_INFO: return 0x06;
    default: return -1;
    }

  default:
    return -1;
  }
}

// Linux and LWIP also map their poll(2) constants differently.
constexpr int mapLinuxPollFlagToLWIP(int linuxFlag) {
  switch (linuxFlag) {
  case POLLIN: return 0x1;
  case POLLPRI: return 0x40;
  case POLLOUT: return 0x2;
  case POLLERR: return 0x4;
  case POLLHUP: return 0x200;
  case POLLNVAL: return 0x8;
  case POLLRDNORM: return 0x10;
  case POLLRDBAND: return 0x20;
  case POLLWRNORM: return 0x80;
  case POLLWRBAND: return 0x100;
  default: return 0;
  }
}

constexpr int mapLWIPPollFlagToLinux(int lwipFlag) {
  switch (lwipFlag) {
  case 0x1: return POLLIN;
  case 0x2: return POLLOUT;
  case 0x4: return POLLERR;
  case 0x8: return POLLNVAL;
  case 0x10: return POLLRDNORM;
  case 0x20: return POLLRDBAND;
  case 0x40: return POLLPRI;
  case 0x80: return POLLWRNORM;
  case 0x100: return POLLWRBAND;
  case 0x200: return POLLHUP;
  default: return 0;
  }
}

constexpr bool isLinuxPollFlagSupported(int linuxFlag) {
  return mapLinuxPollFlagToLWIP(linuxFlag) <= 0x8;
}

int linuxPollEventsToLWIP(int linuxEvents, int *unsupported) {
  int lwipEvents = 0;

  for (int b = 0; linuxEvents; ++b) {
    const int linuxFlag = (1 << b);
    if (linuxEvents & linuxFlag) {
      const auto lwipFlag = mapLinuxPollFlagToLWIP(linuxFlag);
      if (lwipFlag == 0)
        on_error("lwip does not map define poll flag %x", linuxFlag);
      else if (!isLinuxPollFlagSupported(linuxFlag) && unsupported)
        *unsupported |= linuxFlag;
      lwipEvents |= lwipFlag;
      linuxEvents &= ~linuxFlag;
    }
  }

  return lwipEvents;
}

constexpr int lwipPollEventsToLinux(int lwipEvents) {
  int linuxEvents = 0;

  for (int b = 0; lwipEvents; ++b) {
    const int lwipFlag = (1 << b);
    if (lwipEvents & lwipFlag) {
      const auto linuxFlag = mapLWIPPollFlagToLinux(lwipFlag);
      linuxEvents |= linuxFlag;
      lwipEvents &= ~lwipFlag;
    }
  }

  return linuxEvents;
}

constexpr int O_NONBLOCK_LWIP = 0x1;

/**
 * This is a utility function that will return the ExcSessionId from the result of exc_connect.
 *
 * @param hConnection This is the value returned from exc_connect()
 * @return This will return the ExcSessionId value that corresponds to excSessionId.
 */
ExcSessionId excGetSessionId( ExcConnHandle hConn ) {
  return (ExcSessionId) hConn % 128;
}

/**
 * This is a utility function that will return the coreId from the result of exc_connect.
 *
 * @param hConnection This is the value returned from exc_connect()
 * @return This will return the ExcCoreid that corresponds to excSessionId.
 */
EkaCoreId excGetCoreId( ExcConnHandle hConn ) {
  return (EkaCoreId) hConn / 128;
}

inline EkaDev *checkDevice(EkaDev* dev) {
  if (!dev) {
    errno = EFAULT;
    return nullptr;
  }
  else if (!dev->epmEnabled) {
    errno = ENOSYS;
    return nullptr;
  }
  return dev;
}

inline EkaCore *getEkaCore(EkaDev* dev, EkaCoreId coreId) {
  if (!checkDevice(dev))
    return nullptr;
  else if (coreId < 0 || unsigned(coreId) >= std::size(dev->core)) {
    errno = EBADF;
    return nullptr;
  }
  EkaCore *const core = dev->core[coreId];
  if (!core) {
    errno = ENODEV;
    return nullptr;
  }
  return core;
}

inline EkaTcpSess *getEkaTcpSess(EkaDev* dev, ExcConnHandle hConn) {
  if (EkaCore *const core = getEkaCore(dev, excGetCoreId(hConn))) {
    const auto sessionId = excGetSessionId(hConn);
    if (sessionId < 0 || unsigned(sessionId) >= std::size(core->tcpSess)) {
      errno = EBADF;
      return nullptr;
    }
    EkaTcpSess *const s = core->tcpSess[sessionId];
    if (!s)
      errno = ENOTCONN;
    return s;
  }
  return nullptr;
}

/*
 *
 */  
ExcSocketHandle excSocket( EkaDev* dev, EkaCoreId coreId , int domain, int type, int protocol ) {
  EkaCore *const core = getEkaCore(dev, coreId);
  if (!core)
    return -1;

  // int domain, int type, int protocol parameters are ignored
  // always used: socket(AF_INET, SOCK_STREAM, 0)
  const auto sessId = core->addTcpSess();
  return core->tcpSess[sessId]->sock;
}

int excSocketClose( EkaDev* dev, ExcSocketHandle hSocket ) {
  if (!checkDevice(dev))
    return -1;

  EkaTcpSess *const sess = dev->findTcpSess(hSocket);
  if (!sess) {
    EKA_WARN("ExcSocketHandle %d not found", hSocket);
    errno = EBADF;
    return -1;
  }
  else if (sess->isEstablished()) {
    errno = EISCONN;
    return -1;
  }

  dev->core[sess->coreId]->tcpSess[sess->sessId] = nullptr;
  delete sess;
  return 0;
}

/*
 *
 */
ExcConnHandle excConnect( EkaDev* dev, ExcSocketHandle hSocket, const struct sockaddr *dst, socklen_t addrlen ) {
  if (!checkDevice(dev))
    return -1;

  EkaTcpSess *const sess = dev->findTcpSess(hSocket);
  if (!sess) {
    EKA_WARN("ExcSocketHandle %d not found",hSocket);
    errno = EBADF;
    return -1;
  }
  else if (sess->isEstablished()) {
    errno = EISCONN;
    return -1;
  }

  sess->dstIp   = ((sockaddr_in*)dst)->sin_addr.s_addr;
  sess->dstPort = be16toh(((sockaddr_in*)dst)->sin_port);

  sess->bind();
  dev->snDev->set_fast_session(sess->coreId,sess->sessId,
			       sess->srcIp,sess->srcPort,
			       sess->dstIp,sess->dstPort);

  /* EKA_LOG("on coreId=%u, sessId=%u, sock=%d, %s:%u --> %s:%u", */
  /* 	  sess->coreId,sess->sessId,sess->sock, */
  /* 	  EKA_IP2STR(sess->srcIp),sess->srcPort, */
  /* 	  EKA_IP2STR(sess->dstIp),sess->dstPort); */


  if (sess->connect() == -1)
    return -1;

  sess->preloadNwHeaders();
  return sess->getConnHandle();
}

/**
 * This will be called when we need to reconnect to a socket that has already been disconnected.
 * Behavior is undefined if the firing controller is set up to fire on this socket when this is
 * called.
 */
ExcConnHandle excReconnect( EkaDev* pEkaDev, ExcConnHandle hConn ) {
  // TBD
  return 0;
}

/**
 * $$NOTE$$ This is mutexed to handle single session at a time.
 * 
 * @param hConn
 * @param pBuffer
 * @param size
 * @param flag    This should either be a standard linux flag, or the result of 
 *                ekaGetCapsResult( kEkaCapsSendWarmupFlag ).  If it's the latter, 
 *                then the packet shouldnt actually go on the wire, but the software
 *                path should be warmed up.
 * @return This will return the values that exhibit the same behavior of linux's send fn.
 */
ssize_t excSend( EkaDev* dev, ExcConnHandle hConn, const void* pBuffer, size_t size ) {
  if (EkaTcpSess *const s = getEkaTcpSess(dev, hConn))
    return s->sendPayload(s->sessId/* thrId */, (void*) pBuffer, size);
  return -1;
}

/**
 * $$NOTE$$ - This is mutexed to handle single session at a time.
 */
ssize_t excRecv( EkaDev* dev, ExcConnHandle hConn, void *pBuffer, size_t size ) {
  if (EkaTcpSess *const s = getEkaTcpSess(dev, hConn))
    return s->recv(pBuffer,size);
  return -1;
}

/*
 *
 */
int excClose( EkaDev* dev, ExcConnHandle hConn ) {
  if (EkaTcpSess *const s = getEkaTcpSess(dev, hConn)) {
    dev->core[s->coreId]->tcpSess[s->sessId] = nullptr;
    delete s;
    return 0;
  }
  return -1;
}

int excPoll( EkaDev *dev, struct pollfd *fds, int nfds, int timeout ) {
  if (!checkDevice(dev))
    return -1;

  auto *const lwipPollFds = static_cast<pollfd *>(alloca(sizeof(pollfd) * nfds));

  for (int i = 0; i < nfds; ++i) {
    if (fds[i].fd < 0)
      lwipPollFds[i].fd = -1;
    else {
      int unsupported = 0;
      lwipPollFds[i].fd = fds[i].fd;
      lwipPollFds[i].events = linuxPollEventsToLWIP(fds[i].events, &unsupported);
    }
  }

  const int rc = lwip_poll(lwipPollFds, nfds, timeout);
  if (rc != -1) {
    for (int i = 0; i < nfds; ++i)
      fds[i].revents = lwipPollEventsToLinux(lwipPollFds[i].revents);
  }
  return rc;
}

int excGetSockOpt( EkaDev* dev, ExcSocketHandle hSock, int level, int optname,
                   void* optval, socklen_t* optlen ) {
  if (checkDevice(dev)) {
    optname = mapLinuxSocketOptionNameToLWIP(level, optname); // Must be first because
    level = mapLinuxSocketOptionLevelToLWIP(level);           // we change level here
    return lwip_getsockopt(hSock, level, optname, optval, optlen);
  }
  return -1;
}

int excSetSockOpt( EkaDev* dev, ExcSocketHandle hSock, int level, int optname,
                   const void* optval, socklen_t optlen ) {
  if (checkDevice(dev)) {
    optname = mapLinuxSocketOptionNameToLWIP(level, optname);
    level = mapLinuxSocketOptionLevelToLWIP(level);
    return lwip_setsockopt(hSock, level, optname, optval, optlen);
  }
  return -1;
}

int excIoctl( EkaDev* dev, ExcSocketHandle hSock, long cmd, void *argp ) {
  if (cmd != FIONREAD) {
    // We only support FIONREAD
    errno = ENOTTY;
    return -1;
  }
  else if (checkDevice(dev))
    return lwip_ioctl(hSock, FIONREAD_LWIP, argp);
  return -1;
}

int excGetSockName( EkaDev* dev, ExcSocketHandle hSock, sockaddr* addr,
                    socklen_t* addrlen ) {
  if (checkDevice(dev))
    return lwip_getsockname(hSock, addr, addrlen);
  return -1;
}

int excGetPeerName( EkaDev* dev, ExcSocketHandle hSock, sockaddr* addr,
                    socklen_t* addrlen ) {
  if (checkDevice(dev))
    return lwip_getpeername(hSock, addr, addrlen);
  return -1;
}

int excGetBlocking( EkaDev* dev, ExcSocketHandle hSock ) {
  if (checkDevice(dev)) {
    const int flags = lwip_fcntl(hSock, F_GETFL, 0);
    return flags != -1
        ? (flags & O_NONBLOCK_LWIP) ? 0 : 1
        : -1;
  }
  return -1;
}

int excSetBlocking( EkaDev* dev, ExcSocketHandle hSock, bool blocking ) {
  if (checkDevice(dev)) {
    int flags = lwip_fcntl(hSock, F_GETFL, 0);
    if (flags == -1)
      return -1;
    else if (blocking)
      flags &= ~O_NONBLOCK_LWIP;
    else
      flags |= O_NONBLOCK_LWIP;
    return lwip_fcntl(hSock, F_SETFL, flags);
  }
  return -1;
}

int excShutdown( EkaDev* dev, ExcConnHandle hConn, int how ) {
  if (const EkaTcpSess *const s = getEkaTcpSess(dev, hConn))
    return lwip_shutdown(s->sock, how);
  return -1;
}

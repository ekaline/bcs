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

extern "C" {

int lwip_poll(struct pollfd* fds, int nfds, int timeout);
int lwip_getsockopt(int fd, int level, int optname, void* optval, socklen_t *optlen);
int lwip_setsockopt(int fd, int level, int optname, const void* optval, socklen_t optlen);
int lwip_ioctl(int fd, long cmd, void *argp);

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

/*
 *
 */  
ExcSocketHandle excSocket( EkaDev* dev, EkaCoreId coreId , int domain, int type, int protocol ) {
  assert (dev != NULL);

  if (! dev->epmEnabled) {
    EKA_WARN("FPGA TCP is disabled for this Application instance");
    return -1;
  }
  // int domain, int type, int protocol parameters are ignored
  // always used: socket(AF_INET, SOCK_STREAM, 0)

  if (dev->core[coreId] == NULL) on_error("core %u is not connected",coreId);
  uint sessId = dev->core[coreId]->addTcpSess();
  //  return (ExcSocketHandle) (coreId * 128 + sessId);
  return dev->core[coreId]->tcpSess[sessId]->sock;
}

/*
 *
 */
ExcConnHandle excConnect( EkaDev* dev, ExcSocketHandle hSocket, const struct sockaddr *dst, socklen_t addrlen ) {
  assert (dev != NULL);

  if (! dev->epmEnabled) {
    on_error("FPGA TCP is disabled for this Application instance");
  }

  EkaTcpSess* sess = dev->findTcpSess(hSocket);
  if (sess == NULL) {
    EKA_WARN("ExcSocketHandle %d not found",hSocket);
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


  if (sess->connect() == 0) {
    sess->preloadNwHeaders();
    return sess->getConnHandle();
  }

  return -1;
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
  assert (dev != NULL);
  if (! dev->epmEnabled) {
    on_error("FPGA TCP is disabled for this Application instance");
  }
  uint coreId = excGetCoreId(hConn);
  if (dev->core[coreId] == NULL) {
    EKA_WARN("Core %u of hConn %u is not connected",coreId,hConn);
    return -1;
  }

  uint sessId = excGetSessionId(hConn);

  if (dev->core[coreId]->tcpSess[sessId] == NULL) {
    EKA_WARN("Session %u on Core %u of hConn %u is not connected",sessId,coreId,hConn);
    return -1;
  }

  //  EKA_LOG("Sending on coreId=%u, sessId=%u",coreId,sessId);
  //  return dev->core[coreId]->tcpSess[sessId]->sendPayload(0/* thrId */, (void*) pBuffer, size);
  return dev->core[coreId]->tcpSess[sessId]->sendPayload(sessId/* thrId */, (void*) pBuffer, size);
}

/**
 * $$NOTE$$ - This is mutexed to handle single session at a time.
 */
ssize_t excRecv( EkaDev* dev, ExcConnHandle hConn, void *pBuffer, size_t size ) {
  assert (dev != NULL);
  if (! dev->epmEnabled) {
    on_error("FPGA TCP is disabled for this Application instance");
  }
  uint coreId = excGetCoreId(hConn);
  if (dev->core[coreId] == NULL) {
    EKA_WARN("Core %u of hConn %u is not connected",coreId,hConn);
    return -1;
  }

  uint sessId = excGetSessionId(hConn);

  if (dev->core[coreId]->tcpSess[sessId] == NULL) {
    EKA_WARN("Session %u on Core %u of hConn %u is not connected",sessId,coreId,hConn);
    return -1;
  }

  return dev->core[coreId]->tcpSess[sessId]->recv(pBuffer,size);
}

/*
 *
 */
int excClose( EkaDev* dev, ExcConnHandle hConn ) {
  assert (dev != NULL);
  if (! dev->epmEnabled) {
    on_error("FPGA TCP is disabled for this Application instance");
  }
  uint coreId = excGetCoreId(hConn);
  if (dev->core[coreId] == NULL) {
    EKA_WARN("Core %u of hConn %u is not connected",coreId,hConn);
    return -1;
  }

  uint sessId = excGetSessionId(hConn);

  if (dev->core[coreId]->tcpSess[sessId] == NULL) {
    EKA_WARN("Session %u on Core %u of hConn %u is not connected",sessId,coreId,hConn);
    return -1;
  }
  delete dev->core[coreId]->tcpSess[sessId];
  dev->core[coreId]->tcpSess[sessId] = NULL;
  //  return dev->core[coreId]->tcpSess[sessId]->close();
  return 0;
}

int excPoll( EkaDev *dev, struct pollfd *fds, int nfds, int timeout ) {
  auto *const lwipPollFds = static_cast<pollfd *>(alloca(sizeof(pollfd) * nfds));

  for (int i = 0; i < nfds; ++i) {
    const auto hConn = static_cast<ExcConnHandle>(fds[i].fd);
    const auto coreId = excGetCoreId(hConn);
    const auto sessId = excGetSessionId(hConn);

    lwipPollFds[i].fd = dev->core[coreId]->tcpSess[sessId]->sock;
    lwipPollFds[i].events = fds[i].events;
  }

  const int rc = lwip_poll(lwipPollFds, nfds, timeout);
  if (rc != -1) {
    for (int i = 0; i < nfds; ++i)
      fds[i].revents = lwipPollFds[i].revents;
  }
  return rc;
}

int excGetSockOpt( EkaDev* dev, ExcConnHandle hConn, int level, int optname,
                   void* optval, socklen_t* optlen ) {
  const auto coreId = excGetCoreId(hConn);
  const auto sessId = excGetSessionId(hConn);
  const int fd = dev->core[coreId]->tcpSess[sessId]->sock;
  optname = mapLinuxSocketOptionNameToLWIP(level, optname); // Must be first because
  level = mapLinuxSocketOptionLevelToLWIP(level);           // we change level
  return lwip_getsockopt(fd, level, optname, optval, optlen);
}

int excSetSockOpt( EkaDev* dev, ExcConnHandle hConn, int level, int optname,
                   const void* optval, socklen_t optlen ) {
  const auto coreId = excGetCoreId(hConn);
  const auto sessId = excGetSessionId(hConn);
  const int fd = dev->core[coreId]->tcpSess[sessId]->sock;
  optname = mapLinuxSocketOptionNameToLWIP(level, optname);
  level = mapLinuxSocketOptionLevelToLWIP(level);
  return lwip_setsockopt(fd, level, optname, optval, optlen);
}

int excIoctl ( EkaDev* dev, ExcConnHandle hConn, long cmd, void *argp ) {
  const auto coreId = excGetCoreId(hConn);
  const auto sessId = excGetSessionId(hConn);
  const int fd = dev->core[coreId]->tcpSess[sessId]->sock;
  return lwip_ioctl(fd, cmd, argp);
}

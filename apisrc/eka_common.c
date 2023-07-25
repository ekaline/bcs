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
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <sched.h>
#include <sys/resource.h>
#include <stdarg.h>
//#include <syslog.h>

#include "EkaDev.h"
#include "eka_macros.h"

#include "Exc.h"
#include "Efc.h"
void eka_open_sn_channels (EkaDev* dev);
void ekaLwipPollThread (EkaDev* dev);
void ekaExcInitLwip (EkaDev* dev);
void eka_close_tcp ( EkaDev* pEkaDev);

int ekaDefaultLog (void* /*unused*/, const char* function,
                   const char* file, int line, int priority,
                   const char* format, ...) {
  va_list ap;
  const int rc1 = fprintf(g_ekaLogFile, "%s@%s:%u: ",
                          function,file,line);
  va_start(ap, format);
  const int rc2 = vfprintf(g_ekaLogFile, format, ap);
  va_end(ap);
  const int rc3 = fprintf(g_ekaLogFile,"\n");
  return rc1 + rc2 + rc3;
}

int ekaDefaultCreateThread(const char* name, EkaServiceType type,
                           void *(*threadRoutine)(void*), void* arg,
                           void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}

void eka_get_time (char* t) {
  std::chrono::time_point<std::chrono::system_clock> now =
    std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();

  typedef std::chrono::duration<int,
    std::ratio_multiply<std::chrono::hours::period, std::ratio<8>
    >::type> Days; /* UTC: +8:00 */

  Days days = std::chrono::duration_cast<Days>(duration);
  duration -= days;
  auto hours = std::chrono::duration_cast<std::chrono::hours
    >(duration);
  duration -= hours;
  auto minutes = std::chrono::duration_cast<std::chrono::minutes
    >(duration);
  duration -= minutes;
  auto seconds = std::chrono::duration_cast<std::chrono::seconds
    >(duration);
  duration -= seconds;
  auto milliseconds = std::chrono::duration_cast<
    std::chrono::milliseconds>(duration);
  duration -= milliseconds;
  auto microseconds = std::chrono::duration_cast<
    std::chrono::microseconds>(duration);
  duration -= microseconds;
  auto nanoseconds = std::chrono::duration_cast<
    std::chrono::nanoseconds>(duration);

  sprintf(t,"%02ju:%02ju:%02ju.%03ju.%03ju.%03ju",
          (uint64_t)hours.count(),(uint64_t)minutes.count(),
          (uint64_t)seconds.count(),(uint64_t)milliseconds.count(),
          (uint64_t)microseconds.count(),
          (uint64_t)nanoseconds.count());
}
/* ########################################################### */

int ekaTcpConnect(uint32_t ip, uint16_t port) {
#ifdef FH_LAB
  TEST_LOG("Dummy FH_LAB TCP connect to %s:%u",
           EKA_IP2STR(ip),port);
  return -1;
#else
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    on_error("failed to open TCP socket");

  struct sockaddr_in remote_addr = {};
  remote_addr.sin_addr.s_addr = ip;
  remote_addr.sin_port = port;
  remote_addr.sin_family = AF_INET;
  if (connect(sock,(struct sockaddr*)&remote_addr,
              sizeof(struct sockaddr_in)) != 0) 
    on_error("socket connect failed %s:%u",
             EKA_IP2STR(*(uint32_t*)&remote_addr.sin_addr),
             be16toh(remote_addr.sin_port));
  return sock;
#endif
}
/* ########################################################### */
int recvTcpSegment(int sock, void* buf, int segSize) {
  auto d = static_cast<uint8_t*>(buf);
  int received = 0;
  do {
    int r = recv(sock,d,segSize - received,MSG_WAITALL);
    if (r <= 0)
      return r;
    d += r;
    received += r;
  } while (received != segSize);
  return received;
}

/* ########################################################### */
uint32_t getIfIp(const char* ifName) {
  int sck = socket(AF_INET, SOCK_DGRAM, 0);
  if(sck < 0)
    on_error ("failed on socket(AF_INET, SOCK_DGRAM, 0)");

  char          buf[1024] = {};

  struct ifconf ifc = {};
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;
  if(ioctl(sck, SIOCGIFCONF, &ifc) < 0)
    on_error ("failed on ioctl(sck, SIOCGIFCONF, &ifc)");

  struct ifreq* ifr = ifc.ifc_req;
  int nInterfaces   = ifc.ifc_len / sizeof(struct ifreq);

  for(int i = 0; i < nInterfaces; i++) {
    struct ifreq *item = &ifr[i];
    if (strncmp(item->ifr_name,ifName,strlen(ifName)) != 0)
      continue;
    return ((struct sockaddr_in *)&item->ifr_addr)->sin_addr.s_addr;
  }
  return 0;
}

/* ########################################################### */

int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port) {
  // UDP port is already 16b swapped!

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    on_error("failed to open UDP socket");

  EKA_LOG("Subscribing on Kernel UDP MC group %s:%u from %s (%s)",
          EKA_IP2STR(ip),be16toh(port),
          dev->genIfName,EKA_IP2STR(dev->genIfIp));

  const int const_one = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &const_one, sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEADDR) failed");

  struct sockaddr_in mcast = {};
  mcast.sin_family = AF_INET;
  mcast.sin_addr.s_addr = ip;
  mcast.sin_port = port;

  if (bind(sock,(struct sockaddr*) &mcast, sizeof(mcast)) < 0)
    on_error("Failed to bind to %d", be16toh(mcast.sin_port));

  struct ip_mreq mreq = {};
  mreq.imr_interface.s_addr = dev->genIfIp; //INADDR_ANY;
  mreq.imr_multiaddr.s_addr = ip;

  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    on_error("Failed to join  %s", EKA_IP2STR(mreq.imr_multiaddr.s_addr));

  EKA_LOG("Kernel joined MC group %s:%u from %s (%s)",
          EKA_IP2STR(mreq.imr_multiaddr.s_addr),
          be16toh(mcast.sin_port),
          dev->genIfName,EKA_IP2STR(dev->genIfIp));
  return sock;
}

/* ########################################################### */

void errno_decode(int errsv, char* reason) {
  switch (errsv) {
  case EPIPE:  
    strcpy(reason,"Broken PIPE (late with the heartbeats?) "
           "(errno=EPIPE)");
    break;
  case EIO:
    strcpy(reason,"A low-level I/O error occurred while "
           "modifying the inode (errno=EIO)");
    break;
  case EINTR:
    strcpy(reason,"The call was interrupted by a signal "
           "before any data was written (errno=EINTR)");
    break;
  case EAGAIN:
    strcpy(reason,"The file descriptor fd refers to a "
           "file other than a socket and has been "
           "marked nonblocking (O_NONBLOCK), and the "
           "write would block.(errno=EAGAIN)");
    break;
  default:   
    strcpy(reason,"Unknown errno");
  }
  return;
}

int convert_ts(char* dst, uint64_t ts) {
  uint ns = ts % 1000;
  uint64_t res = (ts - ns) / 1000;

  uint us = res % 1000;
  res = (res - us) / 1000;
  
  uint ms = res % 1000;
  res = (res - ms) / 1000;

  uint s = res % 60;
  res = (res - s) / 60;

  uint m = res % 60;
  res = (res - m) / 60;

  uint h = res % 24;

  sprintf (dst,"%02d:%02d:%02d.%03d.%03d.%03d",h,m,s,ms,us,ns);
  return 0;
}

EkaCapsResult ekaGetCapsResult(EkaDev* pEkaDev,
                               enum EkaCapType ekaCapType ) {
  switch (ekaCapType) {

  case EkaCapType::kEkaCapsMaxSecCtxs :
  case EkaCapType::kEkaCapsMaxEkaHandle :
    return (EkaCapsResult) EkaDev::MAX_SEC_CTX;

  case EkaCapType::kEkaCapsExchange :
    return (EkaCapsResult) pEkaDev->hwFeedVer;

  case EkaCapType::kEkaCapsMaxCores:
    return (EkaCapsResult) EkaDev::MAX_CORES;

  case EkaCapType::kEkaCapsMaxSesCtxs :
    return (EkaCapsResult) (EkaDev::MAX_CORES * EkaDev::MAX_SESSION_CTX_PER_CORE);

  case EkaCapType::kEkaCapsMaxSessPerCore:
    return (EkaCapsResult) EkaDev::MAX_SESSION_CTX_PER_CORE;

  case EkaCapType::kEkaCapsNumWriteChans:
    return (EkaCapsResult) EkaDev::MAX_CTX_THREADS;

  case EkaCapType::kEkaCapsSendWarmupFlag:

  default:
    return (EkaCapsResult) 0;
  }
}


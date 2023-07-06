#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <iostream>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <thread>
// #include <syslog.h>

#include "EkaDev.h"
#include "eka_macros.h"

#include "Efc.h"
#include "Exc.h"
void eka_open_sn_channels(EkaDev *dev);
void ekaLwipPollThread(EkaDev *dev);
void ekaExcInitLwip(EkaDev *dev);
void eka_close_tcp(EkaDev *pEkaDev);

/* OnEkaExceptionReportCb*
 * efhDefaultOnException(EkaExceptionReport* msg,
 * EfhRunUserData efhRunUserData) { */
/*   printf("%s: Doing nothing\n",__func__); */
/*   return NULL; */
/* } */

/* OnEfcFireReportCb* efcDefaultOnFireReportCb (EfcCtx*
 * efcCtx, const EfcFireReport* efcFireReport, size_t size)
 * { */
/*   printf("%s: Doing nothing\n",__func__); */
/*   return NULL; */
/* } */

int ekaDefaultLog(void * /*unused*/, const char *function,
                  const char *file, int line, int priority,
                  const char *format, ...) {
  va_list ap;
  const int rc1 = fprintf(
      g_ekaLogFile, "%s@%s:%u: ", function, file, line);
  va_start(ap, format);
  const int rc2 = vfprintf(g_ekaLogFile, format, ap);
  va_end(ap);
  const int rc3 = fprintf(g_ekaLogFile, "\n");
  return rc1 + rc2 + rc3;
}

int ekaDefaultCreateThread(const char *name,
                           EkaServiceType type,
                           void *(*threadRoutine)(void *),
                           void *arg, void *context,
                           uintptr_t *handle) {
  pthread_create((pthread_t *)handle, NULL, threadRoutine,
                 arg);
  pthread_setname_np((pthread_t)*handle, name);
  return 0;
}

void eka_get_time(char *t) {
  std::chrono::time_point<std::chrono::system_clock> now =
      std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();

  typedef std::chrono::duration<
      int, std::ratio_multiply<std::chrono::hours::period,
                               std::ratio<8>>::type>
      Days; /* UTC: +8:00 */

  Days days = std::chrono::duration_cast<Days>(duration);
  duration -= days;
  auto hours =
      std::chrono::duration_cast<std::chrono::hours>(
          duration);
  duration -= hours;
  auto minutes =
      std::chrono::duration_cast<std::chrono::minutes>(
          duration);
  duration -= minutes;
  auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(
          duration);
  duration -= seconds;
  auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          duration);
  duration -= milliseconds;
  auto microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(
          duration);
  duration -= microseconds;
  auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          duration);

  /* std::cout << hours.count() << ":" */
  /* 	    << minutes.count() << ":" */
  /* 	    << seconds.count() << ":" */
  /* 	    << milliseconds.count() << ":" */
  /* 	    << microseconds.count() << ":" */
  /* 	    << nanoseconds.count() << " : ";// << std::endl;
   */
  sprintf(t, "%02ju:%02ju:%02ju.%03ju.%03ju.%03ju",
          (uint64_t)hours.count(),
          (uint64_t)minutes.count(),
          (uint64_t)seconds.count(),
          (uint64_t)milliseconds.count(),
          (uint64_t)microseconds.count(),
          (uint64_t)nanoseconds.count());
}
/* #####################################################################
 */

int ekaTcpConnect(uint32_t ip, uint16_t port) {
#ifdef FH_LAB
  TEST_LOG("Dummy FH_LAB TCP connect to %s:%u",
           EKA_IP2STR(ip), port);
  return -1;
#else
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    on_error("failed to open TCP socket");

  struct sockaddr_in remote_addr = {};
  remote_addr.sin_addr.s_addr = ip;
  remote_addr.sin_port = port;
  remote_addr.sin_family = AF_INET;
  if (connect(sock, (struct sockaddr *)&remote_addr,
              sizeof(struct sockaddr_in)) != 0)
    on_error("socket connect failed %s:%u",
             EKA_IP2STR(*(uint32_t *)&remote_addr.sin_addr),
             be16toh(remote_addr.sin_port));
  return sock;
#endif
}
/* #####################################################################
 */
int recvTcpSegment(int sock, void *buf, int segSize) {
  auto d = static_cast<uint8_t *>(buf);
  int received = 0;
  do {
    int r = recv(sock, d, segSize - received, MSG_WAITALL);
    if (r <= 0)
      return r;
    d += r;
    received += r;
  } while (received != segSize);
  return received;
}

/* #####################################################################
 */
uint32_t getIfIp(const char *ifName) {
  int sck = socket(AF_INET, SOCK_DGRAM, 0);
  if (sck < 0)
    on_error(
        "%s: failed on socket(AF_INET, SOCK_DGRAM, 0) -> ",
        __func__);

  char buf[1024] = {};

  struct ifconf ifc = {};
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;
  if (ioctl(sck, SIOCGIFCONF, &ifc) < 0)
    on_error(
        "%s: failed on ioctl(sck, SIOCGIFCONF, &ifc)  -> ",
        __func__);

  struct ifreq *ifr = ifc.ifc_req;
  int nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

  for (int i = 0; i < nInterfaces; i++) {
    struct ifreq *item = &ifr[i];
    if (strncmp(item->ifr_name, ifName, strlen(ifName)) !=
        0)
      continue;
    return ((struct sockaddr_in *)&item->ifr_addr)
        ->sin_addr.s_addr;
  }
  return 0;
}

/* #####################################################################
 */

int ekaUdpMcConnect(EkaDev *dev, uint32_t ip, uint16_t port,
                    uint32_t srcNicIp) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    on_error("failed to open UDP socket");

  auto ifName = srcNicIp ? "User provided" : dev->genIfName;
  auto srcIp = srcNicIp ? srcNicIp : dev->genIfIp;

  EKA_LOG("Subscribing on Kernel UDP MC "
          "group %s:%u from "
          "%s (%s)",
          EKA_IP2STR(ip), be16toh(port), ifName,
          EKA_IP2STR(srcIp));

  int const_one = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &const_one,
                 sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &const_one,
                 sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEPORT) failed");

  struct sockaddr_in mcast = {};
  mcast.sin_family = AF_INET;
  mcast.sin_addr.s_addr = ip; // INADDR_ANY
  mcast.sin_port = port;
  if (bind(sock, (struct sockaddr *)&mcast,
           sizeof(struct sockaddr)) < 0)
    on_error("Failed to bind to %d",
             be16toh(mcast.sin_port));

  struct ip_mreq mreq = {};
  mreq.imr_interface.s_addr = srcIp; // INADDR_ANY;
  mreq.imr_multiaddr.s_addr = ip;

  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
                 sizeof(mreq)) < 0)
    on_error("Failed to join  %s",
             EKA_IP2STR(mreq.imr_multiaddr.s_addr));

  EKA_LOG("Kernel joined MC group %s:%u from %s (%s)",
          EKA_IP2STR(mreq.imr_multiaddr.s_addr),
          be16toh(mcast.sin_port), ifName,
          EKA_IP2STR(srcIp));
  return sock;
}

int ekaUdpMcConnect(EkaDev *dev, uint32_t ip,
                    uint16_t port) {
  return ekaUdpMcConnect(dev, ip, port, 0);
}
/* #####################################################################
 */

/* int ekaUdpConnect(EkaDev* dev, uint32_t ip, uint16_t
 * port) { */
/*   int sock = socket(AF_INET, SOCK_DGRAM, 0); */
/*   if (sock < 0) on_error("failed to open UDP socket"); */

/*   int const_one = 1; */
/*   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
 * &const_one, sizeof(int)) < 0)
 * on_error("setsockopt(SO_REUSEADDR) failed"); */
/*   if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
 * &const_one, sizeof(int)) < 0)
 * on_error("setsockopt(SO_REUSEPORT) failed"); */

/*   struct sockaddr_in local2bind = {}; */
/*   local2bind.sin_family=AF_INET; */
/*   local2bind.sin_addr.s_addr = INADDR_ANY; */
/*   local2bind.sin_port = port; */
/*   if (bind(sock,(struct sockaddr*) &local2bind,
 * sizeof(struct sockaddr)) < 0) on_error("Failed to bind to
 * %d",be16toh(local2bind.sin_port)); */

/*   struct ip_mreq mreq = {}; */
/*   mreq.imr_interface.s_addr = INADDR_ANY; */
/*   mreq.imr_multiaddr.s_addr = ip; */

/*   if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
 * &mreq, sizeof(mreq)) < 0) on_error("Failed to join
 * %s",EKA_IP2STR(mreq.imr_multiaddr.s_addr)); */

/*   EKA_LOG("Joined MC group
 * %s:%u",EKA_IP2STR(mreq.imr_multiaddr.s_addr),be16toh(local2bind.sin_port));
 */
/*   return sock; */
/* } */

/* uint8_t normalize_bats_symbol_char(char c) { */
/*   if (c >= '0' && c <= '9') return c - '0'; // 0..9 */
/*   if (c >= 'A' && c <= 'Z') return c - 'A'; // 10..35 */
/*   if (c >= 'a' && c <= 'z') return c - 'a'; // 36..61 */
/*   on_error ("Unexpected symbol |%c|",c); */
/* } */

/* uint32_t bats_symbol2optionid (const char* s, uint
 * symbol_size) { */
/*   uint32_t compacted_id = 0; */
/*   if (s[0] != '0' || s[1] != '1') on_error("%s doesnt
 * have \"01\" prefix",s+'\0'); */
/*   for (uint i = 0; i < symbol_size - 2; i++) { */
/*     compacted_id |=
 * normalize_bats_symbol_char(s[symbol_size-i-1]) << (i *
 * symbol_size); */
/*   } */
/*   return compacted_id; */
/* } */

/* EfhTradeCond eka_opra_trade_cond_decode (char tc) { */
/*   switch (tc) { */
/*   case ' ': return EfhTradeCond::kReg; */
/*   case 'A': return EfhTradeCond::kCanc; */
/*   case 'B': return EfhTradeCond::kOseq; */
/*   case 'C': return EfhTradeCond::kCncl; */
/*   case 'D': return EfhTradeCond::kLate; */
/*   case 'F': return EfhTradeCond::kOpen; */
/*   case 'G': return EfhTradeCond::kCnol; */
/*   case 'H': return EfhTradeCond::kOpnl; */
/*   case 'I': return EfhTradeCond::kAuto; */
/*   case 'J': return EfhTradeCond::kReop; */
/*   case 'K': return EfhTradeCond::kAjst; */
/*   case 'L': return EfhTradeCond::kSprd; */
/*   case 'M': return EfhTradeCond::kStdl; */
/*   case 'N': return EfhTradeCond::kStdp; */
/*   case 'O': return EfhTradeCond::kCstp; */
/*   case 'Q': return EfhTradeCond::kCmbo; */
/*   case 'R': return EfhTradeCond::kSpim; */
/*   case 'S': return EfhTradeCond::kIsoi; */
/*   case 'T': return EfhTradeCond::kBnmt; */
/*   case 'X': return EfhTradeCond::kXmpt; */
/*   default: return EfhTradeCond::kUnmapped; */
/*   } */
/*   return EfhTradeCond::kUnmapped; */
/* } */

#if 0
void eka_write(EkaDev* dev, uint64_t addr, uint64_t val) {
  dev->snDev->write(addr,val);
}

uint64_t eka_read(EkaDev* dev, uint64_t addr) {
  return dev->snDev->read(addr);
}

bool eka_is_all_zeros (const void* buf, ssize_t size) {
  uint8_t* b = (uint8_t*)buf;
  for (int i=0; i<size; i++) if (b[i] != 0) return false;
  return true;
}

int decode_session_id (uint16_t id, uint8_t* core, uint8_t* sess) {
    *core = (uint8_t) id / 128;
    *sess = (uint8_t) id % 128;
    return 0;  
}

uint8_t session2core (uint16_t id) {
  return (uint8_t) id / 128;
}

uint8_t session2sess (uint16_t id) {
  return (uint8_t) id % 128;
}

uint16_t encode_session_id (uint8_t core, uint8_t sess) {
    return sess + core*128;
}

uint16_t socket2session (EkaDev* dev, int sock_fd) {
  for (int c=0;c<dev->hw.enabled_cores;c++) {
    for (int s=0;s<dev->core[c].tcp_sessions;s++) {
      if (sock_fd == dev->core[c].tcp_sess[s].sock_fd) {
	return encode_session_id(c,s);
      }
    }
  }
  on_error("SocketFD %d is not found",sock_fd);
  return 0xFFFF;
}
#endif

void errno_decode(int errsv, char *reason) {
  switch (errsv) {
  case EPIPE:
    strcpy(reason, "Broken PIPE (late with the "
                   "heartbeats?) (errno=EPIPE)");
    break;
  case EIO:
    strcpy(reason, "A low-level I/O error occurred while "
                   "modifying the inode (errno=EIO)");
    break;
  case EINTR:
    strcpy(reason,
           "The call was interrupted by a signal before "
           "any data was written (errno=EINTR)");
    break;
  case EAGAIN:
    strcpy(reason, "The file descriptor fd refers to a "
                   "file other than a socket and has been "
                   "marked nonblocking (O_NONBLOCK), and "
                   "the write would block.(errno=EAGAIN)");
    break;
  default:
    strcpy(reason, "Unknown errno");
  }
  return;
}

int convert_ts(char *dst, uint64_t ts) {
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

  sprintf(dst, "%02d:%02d:%02d.%03d.%03d.%03d", h, m, s, ms,
          us, ns);
  return 0;
}

void eka_enable_cores(EkaDev *dev) {
  /* uint64_t fire_rx_tx_en = dev->snDev->read(ENABLE_PORT);
   */
  /* for (int c=0; c<dev->hw.enabled_cores; c++) { */
  /*   if (! dev->core[c].connected) continue; */
  /*   if (dev->core[c].tcp_sessions != 0) fire_rx_tx_en |=
   * 1ULL << (16+c); //fire core enable */
  /*   if (dev->core[c].udp_sessions != 0) fire_rx_tx_en |=
   * 1ULL << c; // RX (Parser) core enable */
  /*   EKA_LOG("fire_rx_tx_en = 0x%016jx",fire_rx_tx_en); */
  /* } */
  /* dev->snDev->write(ENABLE_PORT,fire_rx_tx_en); */
}

/* void eka_disable_cores(EkaDev* dev) { */
/*   dev->snDev->write( ENABLE_PORT, 0); */
/* } */

/* void hexDump (const char* desc, void *addr, int len) { */
/*     int i; */
/*     unsigned char buff[17]; */
/*     unsigned char *pc = (unsigned char*)addr; */
/*     if (desc != NULL) printf("%s:\n", desc); */
/*     if (len == 0) { printf("  ZERO LENGTH\n"); return; }
 */
/*     if (len < 0)  { printf("  NEGATIVE LENGTH:
 * %i\n",len); return; } */
/*     for (i = 0; i < len; i++) { */
/*         if ((i % 16) == 0) { */
/*             if (i != 0) printf("  %s\n", buff); */
/*             printf("  %04x ", i); */
/*         } */
/*         printf(" %02x", pc[i]); */
/*         if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i %
 * 16] = '.'; */
/*         else buff[i % 16] = pc[i]; */
/*         buff[(i % 16) + 1] = '\0'; */
/*     } */
/*     while ((i % 16) != 0) { printf("   "); i++; } */
/*     printf("  %s\n", buff); */
/* } */

EkaCapsResult ekaGetCapsResult(EkaDev *pEkaDev,
                               enum EkaCapType ekaCapType) {
  switch (ekaCapType) {

  case EkaCapType::kEkaCapsMaxSecCtxs:
  case EkaCapType::kEkaCapsMaxEkaHandle:
    return (EkaCapsResult)EkaDev::MAX_SEC_CTX;

  case EkaCapType::kEkaCapsExchange:
    return (EkaCapsResult)pEkaDev->hwFeedVer;

  case EkaCapType::kEkaCapsMaxCores:
    return (EkaCapsResult)EkaDev::MAX_CORES;

  case EkaCapType::kEkaCapsMaxSesCtxs:
    return (
        EkaCapsResult)(EkaDev::MAX_CORES *
                       EkaDev::MAX_SESSION_CTX_PER_CORE);

  case EkaCapType::kEkaCapsMaxSessPerCore:
    return (EkaCapsResult)EkaDev::MAX_SESSION_CTX_PER_CORE;

  case EkaCapType::kEkaCapsNumWriteChans:
    return (EkaCapsResult)EkaDev::MAX_CTX_THREADS;

  case EkaCapType::kEkaCapsSendWarmupFlag:

  default:
    return (EkaCapsResult)0;
  }
}

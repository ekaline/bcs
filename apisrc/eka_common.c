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

#include "ekaline.h"
#include "eka_sn_dev.h"
#include "eka_user_channel.h"

#include "Exc.h"
void eka_open_sn_channels (EkaDev* dev);
void ekaLwipPollThread (EkaDev* dev);
void ekaExcInitLwip (EkaDev* dev);
void eka_close_tcp ( EkaDev* pEkaDev);

OnEkaExceptionReportCb* efhDefaultOnException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData) {
  printf("%s: Doing nothing\n",__func__);
  return NULL;
}

OnEfcFireReportCb* efcDefaultOnFireReportCb (EfcCtx* efcCtx, const EfcFireReport* efcFireReport, size_t size) {
  printf("%s: Doing nothing\n",__func__);
  return NULL;
}

int ekaDefaultLog (void* /*unused*/, const char* function, const char* file, int line, int priority, const char* format, ...) {
  va_list ap;
  const int rc1 = fprintf(stderr, "%s@%s:%u: ",function,file,line);
  va_start(ap, format);
  const int rc2 = vfprintf(stderr, format, ap);
  va_end(ap);
  const int rc3 = fprintf(stderr,"\n");
  return rc1 + rc2 + rc3;
}

uint64_t eka_read(eka_dev_t* dev, uint64_t addr) {
  return dev->sn_dev->read(addr);
}

void eka_get_time (char* t) {
  std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();

  typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<8>
    >::type> Days; /* UTC: +8:00 */

  Days days = std::chrono::duration_cast<Days>(duration);
  duration -= days;
  auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
  duration -= hours;
  auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
  duration -= minutes;
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  duration -= seconds;
  auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  duration -= milliseconds;
  auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
  duration -= microseconds;
  auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

  /* std::cout << hours.count() << ":" */
  /* 	    << minutes.count() << ":" */
  /* 	    << seconds.count() << ":" */
  /* 	    << milliseconds.count() << ":" */
  /* 	    << microseconds.count() << ":" */
  /* 	    << nanoseconds.count() << " : ";// << std::endl; */
  sprintf(t,"%02ju:%02ju:%02ju.%03ju.%03ju.%03ju",(uint64_t)hours.count(),(uint64_t)minutes.count(),(uint64_t)seconds.count(),(uint64_t)milliseconds.count(),(uint64_t)microseconds.count(),(uint64_t)nanoseconds.count());
}
/* ##################################################################### */

std::string ts_ns2str(uint64_t ts) {
  char dst[32] = {};
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
  return std::string(dst);
}

/* ##################################################################### */

int ekaTcpConnect(int* sock, uint32_t ip, uint16_t port) {
#ifndef FH_LAB
  if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) on_error("failed to open TCP socket");

  struct sockaddr_in remote_addr = {};
  remote_addr.sin_addr.s_addr = ip;
  remote_addr.sin_port = port;
  remote_addr.sin_family = AF_INET;
  if (connect(*sock,(struct sockaddr*)&remote_addr,sizeof(struct sockaddr_in)) != 0) 
    on_error("socket connect failed %s:%u",EKA_IP2STR(*(uint32_t*)&remote_addr.sin_addr),be16toh(remote_addr.sin_port));
#endif
  return 0;
}
/* ##################################################################### */

int ekaUdpConnect(EkaDev* dev, int* sock, uint32_t ip, uint16_t port) {
  if ((*sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) on_error("failed to open UDP socket");

  int const_one = 1;
  if (setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &const_one, sizeof(int)) < 0) on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, &const_one, sizeof(int)) < 0) on_error("setsockopt(SO_REUSEPORT) failed");

  struct sockaddr_in local2bind = {};
  local2bind.sin_family=AF_INET;
  local2bind.sin_addr.s_addr = INADDR_ANY;
  local2bind.sin_port = port;
  if (bind(*sock,(struct sockaddr*) &local2bind, sizeof(struct sockaddr)) < 0) on_error("Failed to bind to %d",be16toh(local2bind.sin_port));

  struct ip_mreq mreq = {};
  mreq.imr_interface.s_addr = INADDR_ANY;
  mreq.imr_multiaddr.s_addr = ip;

  if (setsockopt(*sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) on_error("Failed to join  %s",EKA_IP2STR(mreq.imr_multiaddr.s_addr));

  EKA_LOG("Joined MC group %s:%u",EKA_IP2STR(mreq.imr_multiaddr.s_addr),be16toh(local2bind.sin_port));
  return 0;
}


/* uint8_t normalize_bats_symbol_char(char c) { */
/*   if (c >= '0' && c <= '9') return c - '0'; // 0..9 */
/*   if (c >= 'A' && c <= 'Z') return c - 'A'; // 10..35 */
/*   if (c >= 'a' && c <= 'z') return c - 'a'; // 36..61 */
/*   on_error ("Unexpected symbol |%c|",c); */
/* } */

/* uint32_t bats_symbol2optionid (const char* s, uint symbol_size) { */
/*   uint32_t compacted_id = 0; */
/*   if (s[0] != '0' || s[1] != '1') on_error("%s doesnt have \"01\" prefix",s+'\0'); */
/*   for (uint i = 0; i < symbol_size - 2; i++) { */
/*     compacted_id |= normalize_bats_symbol_char(s[symbol_size-i-1]) << (i * symbol_size); */
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

void eka_write(EkaDev* dev, uint64_t addr, uint64_t val) {
  dev->sn_dev->write(addr,val);
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

uint16_t socket2session (eka_dev_t* dev, int sock_fd) {
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

void errno_decode(int errsv, char* reason) {
  switch (errsv) {
  case EPIPE:  
    strcpy(reason,"Broken PIPE (late with the heartbeats?) (errno=EPIPE)");
    break;
  case EIO:
    strcpy(reason,"A low-level I/O error occurred while modifying the inode (errno=EIO)");
    break;
  case EINTR:
    strcpy(reason,"The call was interrupted by a signal before any data was written (errno=EINTR)");
    break;
  case EAGAIN:
    strcpy(reason,"The file descriptor fd refers to a file other than a socket and has been marked nonblocking (O_NONBLOCK), and the write would block.(errno=EAGAIN)");
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

void eka_enable_cores(eka_dev_t* dev) {
  uint64_t fire_rx_tx_en = dev->sn_dev->read(ENABLE_PORT);
  for (int c=0; c<dev->hw.enabled_cores; c++) {
    if (! dev->core[c].connected) continue;
    if (dev->core[c].tcp_sessions != 0) fire_rx_tx_en |= 1ULL << (16+c); //fire core enable
    if (dev->core[c].udp_sessions != 0) fire_rx_tx_en |= 1ULL << c; // RX (Parser) core enable
    EKA_LOG("fire_rx_tx_en = 0x%016jx",fire_rx_tx_en);
  }
  dev->sn_dev->write(ENABLE_PORT,fire_rx_tx_en);
}

/* void eka_disable_cores(eka_dev_t* dev) { */
/*   dev->sn_dev->write( ENABLE_PORT, 0); */
/* } */

void hexDump (const char* desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    if (desc != NULL) printf("%s:\n", desc);
    if (len == 0) { printf("  ZERO LENGTH\n"); return; }
    if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) printf("  %s\n", buff);
            printf("  %04x ", i);
        }
        printf(" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
        else buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) { printf("   "); i++; }
    printf("  %s\n", buff);
}

void hexDumpStderr (const char* desc, const void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    if (desc != NULL) fprintf(stderr,"%s:\n", desc);
    if (len == 0) { fprintf(stderr,"  ZERO LENGTH\n"); return; }
    if (len < 0)  { fprintf(stderr,"  NEGATIVE LENGTH: %i\n",len); return; }
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) fprintf(stderr,"  %s\n", buff);
            fprintf(stderr,"  %04x ", i);
        }
        fprintf(stderr," %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
        else buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) { fprintf(stderr,"   "); i++; }
    fprintf(stderr,"  %s\n", buff);
}

EkaCapsResult ekaGetCapsResult(EkaDev* pEkaDev,  enum EkaCapType ekaCapType ) {
  switch (ekaCapType) {

  case EkaCapType::kEkaCapsMaxSecCtxs :
  case EkaCapType::kEkaCapsMaxEkaHandle :
    return (EkaCapsResult) MAX_SEC_CTX;

  case EkaCapType::kEkaCapsExchange :
    return (EkaCapsResult) pEkaDev->hw.feed_ver;

  case EkaCapType::kEkaCapsMaxCores:
    return (EkaCapsResult) pEkaDev->CONF::MAX_CORES;

  case EkaCapType::kEkaCapsMaxSesCtxs :
    return (EkaCapsResult) (EKA_MAX_CORES * MAX_SESSION_CTX_PER_CORE);

  case EkaCapType::kEkaCapsMaxSessPerCore:
    return (EkaCapsResult) MAX_SESSION_CTX_PER_CORE;

  case EkaCapType::kEkaCapsNumWriteChans:
    return (EkaCapsResult) pEkaDev->CONF::MAX_CTX_THREADS;

  case EkaCapType::kEkaCapsSendWarmupFlag:

  default:
    return (EkaCapsResult) 0;
  }
}

#if 1
eka_add_conf_t eka_conf_parse(eka_dev_t* dev, eka_conf_type_t conf_type, const char *key, const char *value) {
  //  EKA_LOG("%s : %s",key,value);

  char val_buf[200];
  memset (val_buf,'\0',sizeof(val_buf));
  strcpy (val_buf,value);
  int i=0;
  char* v[10];
  v[i] = strtok(val_buf,":");
  while(v[i]!=NULL) v[++i] = strtok(NULL,":");
  // parsing KEY
  char key_buf[200];
  memset (key_buf,'\0',sizeof(key_buf));
  strcpy (key_buf,key);
  i=0;
  char* k[10];

  k[i] = strtok(key_buf,".");
  while(k[i]!=NULL) k[++i] = strtok(NULL,".");

  //-------------- OLD KEY - VALUE VERSION --------------------

  if ((strcmp(k[0],"efh")==0) && (strcmp(k[1],"net")==0)  && (strcmp(k[2],"host")==0)) {//"efh.net.host.X" "IPV4"
    struct in_addr local_addr;
    inet_aton (k[3],&local_addr);
    if (local_addr.s_addr == dev->core[atoi(k[3])].src_ip) return IGNORED;
    return CONFLICTING_CONF;
  }

  if ((strcmp(k[0],"efh")==0) && (strcmp(k[1],"glimpse")==0)  && (strcmp(k[2],"auth")==0)) {//"efh.glimpse.auth" "user:passwd"
    EKA_LOG("setting Glimpse auth credentials: %s:%s",v[0],v[1]);
    memset(&(dev->fh[0]->auth_user),' ',sizeof(dev->fh[0]->auth_user));
    memset(&(dev->fh[0]->auth_passwd),' ',sizeof(dev->fh[0]->auth_passwd));
    memcpy(&(dev->fh[0]->auth_user),v[0],strlen(v[0]));
    memcpy(&(dev->fh[0]->auth_passwd),v[1],strlen(v[1]));
    return CONF_SUCCESS;
  }

  if (strcmp(k[0],"efh") || strcmp(k[1],"group")) return UNKNOWN_KEY; // on_error("%s.%s is not supported",k[0],k[1]);
  for (char* c = k[2]; *c != '\0' ; c++) if (*c < '0' || *c >'9') on_error("group number %c in %s is not numeric",*c,k[2]);
  uint8_t gr = (uint8_t) atoi(k[2]);
  if ((strcmp(k[3],"recovery")==0) && (strcmp(k[4],"addr")==0)) {//"efh.group.X.recovery.addr", "IPV4:port"    
    for (int c=0; c<dev->hw.enabled_cores; c++) {
      if (! dev->core[c].connected) continue;
      if (gr >= dev->fh[0]->groups) on_error("group_id %d >= dev->fh[0]->groups (=%u)",gr,dev->fh[0]->groups);
      inet_aton (v[0],(struct in_addr*)&(dev->fh[0]->b_gr[gr]->recovery_ip));
      dev->fh[0]->b_gr[gr]->recovery_port = htons((uint16_t)atoi(v[1]));
      dev->fh[0]->b_gr[gr]->recovery_set = true;
    }
    EKA_LOG("setting Mold Recovery %s:%d for FH gr %u",inet_ntoa(* (struct in_addr*) & dev->fh[0]->b_gr[gr]->recovery_ip),be16toh(dev->fh[0]->b_gr[gr]->recovery_port),gr);
    return CONF_SUCCESS;
  }

  if ((strcmp(k[3],"mcast")==0) && (strcmp(k[4],"addr")==0)) {//"efh.group.X.mcast.addr", "IPV4:port"    
    if (conf_type == EFC_CONF) {
      for (int c=0; c<dev->hw.enabled_cores; c++) {
	if (! dev->core[c].connected) continue;
	if (gr >= EKA_MAX_UDP_SESSIONS_PER_CORE) on_error("group_id %d >= EKA_MAX_UDP_SESSIONS_PER_CORE",gr);
	inet_aton (v[0],(struct in_addr*)&(dev->core[c].udp_sess[gr].mcast_ip));
	dev->core[c].udp_sess[gr].mcast_port =  htons((uint16_t)atoi(v[1]));
	dev->core[c].udp_sess[gr].mcast_set = true;
	EKA_LOG("setting MCAST %s:%d for gr %d, core %d",v[0],atoi(v[1]),gr,c);
      }
    } else { // EFH_CONF
      if (gr >= dev->fh[0]->groups) on_error("group_id %d >= dev->fh[0]->groups (=%u)",gr,dev->fh[0]->groups);
      inet_aton (v[0],(struct in_addr*)&(dev->fh[0]->b_gr[gr]->mcast_ip));
      dev->fh[0]->b_gr[gr]->mcast_port =  htons((uint16_t)atoi(v[1]));
      dev->fh[0]->b_gr[gr]->mcast_set = true;
    }
    return CONF_SUCCESS;
  }

  if ((strcmp(k[3],"first")==0) && (strcmp(k[4],"session")==0)) {//"efh.group.X.first.session",gr
    //    EKA_LOG("efh.group.X.first.session");
    eka_set_group_session(dev,gr,atoi(v[0]));
    /* for (int c=0; c<dev->hw.enabled_cores; c++) { */
    /*   if (! dev->core[c].connected) continue; */
    /*   if (gr >= EKA_MAX_UDP_SESSIONS_PER_CORE) on_error("group_id %d >= EKA_MAX_UDP_SESSIONS_PER_CORE",gr); */
    /*   dev->core[c].udp_sess[gr].first_session_id = atoi(v[0]); */
    /*   EKA_LOG("setting %s.%s : %d for gr %d, core %d",k[3],k[4],atoi(v[0]),gr,c); */
    /* } */
    return CONF_SUCCESS;
  }

  if ((strcmp(k[3],"local")==0) && (strcmp(k[4],"addr")==0)) {
    struct in_addr local_addr;
    inet_aton (v[0],&local_addr);
    for (int c=0; c<dev->hw.enabled_cores; c++) if (local_addr.s_addr == dev->core[c].src_ip) return IGNORED;
    return CONFLICTING_CONF;
  }

  if (strcmp(k[3],"disable_igmp")==0) {
    for (int c=0; c<dev->hw.enabled_cores; c++) {
      /* dev->core[c].udp_sess[gr].disable_igmp = true; */
      /* int fd = SN_GetFileDescriptor(dev->dev_id); */
      /* eka_ioctl_t state; */
      /* memset(&state,0,sizeof(eka_ioctl_t)); */
      /* state.cmd = EKA_DROP_IGMP_ON; */
      /* state.nif_num = c; */
      /* int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state); */
      /* if (rc < 0) on_error("error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_DROP_IGMP for core %u",c); */
      /* EKA_LOG("EKA_DROP_IGMP = ON for core %u",c); */
    }
    return CONF_SUCCESS;
  }
  EKA_LOG("UNKNOWN_KEY: k=%s v=%s",key,value);
  return UNKNOWN_KEY;
}

#endif

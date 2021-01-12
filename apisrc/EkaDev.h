#ifndef _EKA_DEV_H
#define _EKA_DEV_H


// SHURIK
#include <mutex>
#include <thread>

#include "eka_hw_conf.h"
#include "EkaHwInternalStructs.h"
#include "Eka.h"
#include "Efc.h"
#include "Efh.h"

#include "eka_sn_addr_space.h"

class EkaFhRunGroup;
class EkaFhGroup;
class EkaFh;
class EkaEfc;
class EkaEpm;
class EkaCore;
class EkaTcpSess;
class EkaSnDev;
class EkaUserReportQ;
class EkaHwCaps;
class EkaUserChannel;

class EkaDev {

 public:
  EkaDev(const EkaDevInitCtx* initCtx);
  ~EkaDev();
  int clearHw();

  void     eka_write(uint64_t addr, uint64_t val);
  uint64_t eka_read(uint64_t addr);

  bool        openEpm();

  EkaTcpSess* findTcpSess(uint32_t ipSrc, uint16_t udpSrc, uint32_t ipDst, uint16_t udpDst);
  EkaTcpSess* findTcpSess(int sock);
  EkaTcpSess* getControlTcpSess(uint8_t coreId);
  uint8_t     findCoreByMacSa(const uint8_t* macSa);

  int      configurePort(const EkaCoreInitCtx* pCoreInit);
  uint8_t  getNumFh();

  static const uint MAX_FEED_HANDLERS       = 8;
  static const int8_t MAX_CORES             = 8;
  static const uint MAX_RUN_GROUPS          = 32;
  static const uint MAX_SESS_PER_CORE       = 31;
  static const uint CONTROL_SESS_ID         = MAX_SESS_PER_CORE;
  static const uint TOTAL_SESSIONS_PER_CORE = MAX_SESS_PER_CORE + 1;
  static const uint MAX_PKT_SIZE            = 1536;
  static const uint MAX_UDP_CHANNELS        = 32;

  static const uint MAX_SEC_CTX             = 768*1024;
  static const uint MAX_CTX_THREADS         = 16;
  static const uint MAX_SESSION_CTX_PER_CORE = 128;

  EkaDev*                   dev = NULL; // pointer to myself
  EkaSnDev*                 snDev = NULL;

  bool                      epmEnabled = false;
  EkaUserChannel*           epmReport = NULL;
  EkaUserChannel*           lwipPath = NULL;

  uint8_t                   hwEnabledCores = 0;
  EfhFeedVer                hwFeedVer = EfhFeedVer::kInvalid;
  uint16_t                  hwRawFireSize = 0;

  EkaCore*                  core[MAX_CORES] = {};
  uint64_t                  totalNumTcpSess = 0; // for sn_state only

  EkaEpm*                   epm = NULL;
  EpmThreadState            epmThr[MAX_CTX_THREADS] = {};

  bool                      use_vlan = false;

  volatile bool             exc_active = false;

  int64_t                   lwip_sem;
  volatile uint8_t*         txPktBuf = NULL;

  pthread_mutex_t           tcp_send_mutex;
  pthread_mutex_t           tcp_socket_open_mutex;

  std::mutex                addTcpSessMtx;
  std::mutex                lwipConnectMtx;

  struct EfcCtx*            pEfcCtx                    = NULL;
  struct EfcRunCtx*         pEfcRunCtx                 = NULL;
  struct EfhRunCtx*         pEfhRunCtx                 = NULL;

  void*                     credContext                = NULL;
  void*                     createThreadContext        = NULL;

  volatile bool             servThreadActive           = false;
  std::thread               servThread;
  volatile bool             servThreadTerminated       = true;

  volatile bool             fireReportThreadActive     = false;
  std::thread               fireReportThread;
  volatile bool             fireReportThreadTerminated = true;


  volatile bool             exc_inited                 = false;
  volatile bool             lwip_inited                = false;
  //  volatile bool             ekaLwipPollThreadIsUp;
  /* volatile bool             efc_run_threadIsUp = false; */
  /* volatile bool             efc_fire_report_threadIsUp = false; */

  volatile uint8_t          numFh                      = 0;

  EkaFh*                    fh[MAX_FEED_HANDLERS]      = {};

  volatile uint8_t          numRunGr = 0;
  EkaFhRunGroup*            runGr[MAX_RUN_GROUPS]      = {};
  std::mutex                mtx;   // mutex to protect concurrent dev->numRunGr++

  EkaLogCallback            logCB;
  void*                     logCtx                     = NULL;

  EkaAcquireCredentialsFn   credAcquire;
  EkaReleaseCredentialsFn   credRelease;
  EkaThreadCreateFn         createThread;

  bool                      print_parsed_messages = false;

  /* static const uint64_t     statSwVersion          = SW_SCRATCHPAD_BASE; */
  /* static const uint64_t     statGlobalCoreAddrBase = statSwVersion + 8; */
  /* static const uint64_t     statMcCoreAddrBase     = statGlobalCoreAddrBase + 8 * MAX_CORES; */
  volatile int              statNumUdpSess[MAX_CORES] = {};
  volatile uint32_t         statMcGrCore[EKA_MAX_UDP_SESSIONS_PER_CORE][MAX_CORES] = {};

  EkaHwCaps*                ekaHwCaps = NULL;

  EkaEfc*                   efc = NULL;

#ifdef TEST_PRINT_DICT
  FILE* testDict;
#endif
  EkaUserReportQ*           userReportQ = NULL;

};


/* ######################################################################## */

inline void eka_write(EkaDev* dev, uint64_t addr, uint64_t val) { 
#ifdef _VERILOG_SIM
  //  if (addr!=0xf0300 && addr!=0xf0308 && addr!=0xf0310 && addr!=0xf0608) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val); //general writes
  //  if ((addr>=0x70000 && addr<=0x80000) || addr==0xf0410) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val); //tob+depth
  if ((addr>=0x89000 && addr<0x8a000) || addr==0xf0238) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val); //epm action
  if (addr==0xf0230) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                                    //epm trigger
  if ((addr>=0x81000 && addr<0x82000)) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //epm data thread window start
  if ((addr>=0x82000 && addr<0x83000)) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //epm region
  if ((addr>=0x85000 && addr<0x86000)) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //epm region strategy enables
  if ((addr>=0xd0000 && addr<0xe0000)) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //epm data
  if ((addr>=0xc0000 && addr<0xd0000)) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //epm template
  if ((addr>=0x88000 && addr<0x89000)) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //epm tcpcs template
  if ((addr==0xf0020                )) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //ENABLE_PORT
  if ((addr==0xf0500                )) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val);                  //FH_GROUP_IPPORT

  //  if ((addr>=0x50000 && addr<0x60000) || addr==0xf0200) printf ("efh_write(20'h%jx,64'h%jx);\n",addr,val); //fastpath data and desc

#endif

  return dev->eka_write(addr,val);
}

inline uint64_t eka_read (EkaDev* dev, uint64_t addr)               { 
  return dev->eka_read(addr);      
}

inline bool eka_is_all_zeros (const void* buf, ssize_t size) {
  uint8_t* b = (uint8_t*)buf;
  for (int i=0; i<size; i++) if (b[i] != 0) return false;
  return true;
}

inline void copyBuf2Hw(EkaDev* dev,uint64_t dstAddr,uint64_t* srcAddr,uint msgSize) {
  //  EKA_LOG("dstAddr=0x%jx, srcAddr=%p, msgSize=%u",dstAddr,srcAddr,msgSize);
  uint words2write = msgSize / 8 + !!(msgSize % 8);
  for (uint w = 0; w < words2write; w++)
    eka_write(dev, dstAddr + w * 8, *srcAddr++); 
}

inline void copyHw2Buf(EkaDev* dev,void* bufAddr,uint64_t srcAddr,uint bufSize) {
  uint64_t* dstAddr = (uint64_t*) bufAddr;
  //  EKA_LOG("dstAddr=%p, srcAddr=0x%jx, bufSize=%u",dstAddr,srcAddr,msgSize);
  uint words2read = bufSize / 8 + !!(bufSize % 8);
  for (uint w = 0; w < words2read; w++)
    *(dstAddr + w) = eka_read(dev,srcAddr + w * 8); 
}


inline void bufSwap4(uint32_t* dst, uint32_t* src, uint size) {
  uint words2write = size / 4 + !!(size % 4);
  for (uint w = 0; w < words2write; w++) {
    (*dst++) = be32toh(*src++);
  }
}

inline void copyBuf2Hw_swap4(EkaDev* dev,uint64_t dstAddr,uint64_t* srcAddr,uint msgSize) {
  //  EKA_LOG("dstAddr=0x%jx, srcAddr=%p, msgSize=%u",dstAddr,srcAddr,msgSize);
  uint words2write = msgSize / 8 + !!(msgSize % 8);
  for (uint w = 0; w < words2write; w++) {
    uint32_t dataLO =  *(uint32_t*) srcAddr;
    uint32_t dataHI =  *((uint32_t*) srcAddr + 1);
    uint32_t dataLO_swapped = be32toh(dataLO);
    uint32_t dataHI_swapped = be32toh(dataHI);
    uint64_t res = (((uint64_t)dataHI_swapped) << 32 ) | (uint64_t)dataLO_swapped;

    eka_write(dev, dstAddr + w * 8, res); 
    srcAddr++;
  }
}

// dstLogicalAddr : Window pointer in Heap
//                  should be taken by getHeapWndAddr()
inline void copyIndirectBuf2HeapHw_swap4(EkaDev* dev, uint64_t dstLogicalAddr,uint64_t* srcAddr,uint8_t thrId, uint msgSize) {
  //  EKA_LOG("dstLogicalAddr=0x%jx, srcAddr=%p, msgSize=%u",dstLogicalAddr,srcAddr,msgSize);
  if (thrId >= (int)EkaDev::MAX_CTX_THREADS) on_error("thrId %u >= MAX_CTX_THREADS %d",thrId,EkaDev::MAX_CTX_THREADS);

  if ((! dev->epmThr[thrId].valid) || (dev->epmThr[thrId].threadWindBase != dstLogicalAddr)) {
    // Configuring window base WINDOW_START_POINTER (to configure it, write the pointer to address 0x81000+THREAD_ID*8)
    eka_write(dev, 0x81000+thrId*8, dstLogicalAddr); 

    dev->epmThr[thrId].valid = true;
    dev->epmThr[thrId].threadWindBase = dstLogicalAddr;
  }
  uint64_t dstAddr = EpmHeapHwBaseAddr + thrId * 0x01000;

  uint words2write = msgSize / 8 + !!(msgSize % 8);
  for (uint w = 0; w < words2write; w++) {
    uint32_t dataLO =  *(uint32_t*) srcAddr;
    uint32_t dataHI =  *((uint32_t*) srcAddr + 1);
    uint32_t dataLO_swapped = be32toh(dataLO);
    uint32_t dataHI_swapped = be32toh(dataHI);
    uint64_t res = (((uint64_t)dataHI_swapped) << 32 ) | (uint64_t)dataLO_swapped;

    eka_write(dev, dstAddr + w * 8, res); 
    srcAddr++;
  }
}


inline void atomicIndirectBufWrite(EkaDev* dev, uint64_t addr, uint8_t bank, uint8_t threadId, uint32_t idx, uint8_t target_table) {
  union scratchpad_table_desc {
    uint64_t lt_desc;
    struct ltd {
      uint8_t src_bank;
      uint8_t src_thread;
      uint32_t target_idx : 24;
      uint8_t pad[2];
      uint8_t target_table; // bit[0]: buy/sell book, bit[7:1]: security index when subscribing
    }__attribute__((packed)) ltd;
  }__attribute__((packed));

  union scratchpad_table_desc desc = {};
  desc.ltd.src_bank     = bank;
  desc.ltd.src_thread   = threadId;
  desc.ltd.target_idx   = idx;
  desc.ltd.target_table = target_table;

  eka_write (dev,addr,desc.lt_desc);
}

inline void indirectWrite(EkaDev* dev,uint64_t addr,uint64_t data) {
  eka_write(dev,0xf0100,addr);
  eka_write(dev,0xf0108,data); 
}


inline uint64_t indirectRead(EkaDev* dev,uint64_t addr) {
  eka_write(dev,0xf0100,addr);
  return eka_read(dev,0xf0108); 
}

inline void hexDump (const char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char*)addr;
  if (desc != NULL) printf ("%s:\n", desc);
  if (len == 0) { printf("  ZERO LENGTH\n"); return; }
  if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
  for (i = 0; i < len; i++) {
    if ((i % 16) == 0) {
      if (i != 0) printf ("  %s\n", buff);
      printf ("  %04x ", i);
    }
    printf (" %02x", pc[i]);
    if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
    else buff[i % 16] = pc[i];
    buff[(i % 16) + 1] = '\0';
  }
  while ((i % 16) != 0) { printf ("   "); i++; }
  printf ("  %s\n", buff);
}

inline void hexDump (const char *desc, const void *addr, int len) {
  hexDump(desc,(uint8_t*)addr,len);
}

inline void checkScratchPadAddr(uint64_t addr) {
  if (addr >= SW_SCRATCHPAD_BASE + SW_SCRATCHPAD_SIZE)
    on_error("Out of Scratchpad address space: %jx > SW_SCRATCHPAD_BASE %jx + SW_SCRATCHPAD_SIZE %jx",
	     addr,(uint64_t)SW_SCRATCHPAD_BASE,(uint64_t)SW_SCRATCHPAD_SIZE);
}

inline void saveMcStat(EkaDev* dev, uint8_t coreId, uint32_t mcast_ip) {
  if (dev->statNumUdpSess[coreId] == EKA_MAX_UDP_SESSIONS_PER_CORE) 
    on_error("cannot subscribe on UDP %s sess %d",EKA_IP2STR(mcast_ip),coreId);
  int currSess = dev->statNumUdpSess[coreId];
  dev->statMcGrCore[coreId][currSess] = /* (volatile uint32_t) */mcast_ip;
  uint globalSessId = coreId * EKA_MAX_UDP_SESSIONS_PER_CORE + currSess;
  uint64_t statMcIpAddr = SCRPAD_CORE_MC_IP_BASE + globalSessId * 8;
  eka_write(dev,statMcIpAddr,mcast_ip);

  dev->statNumUdpSess[coreId]++;
  uint64_t statNumUdpSessAddr = SCRPAD_CORE_BASE + 8 * coreId;
  checkScratchPadAddr(statNumUdpSessAddr);

  eka_write(dev,statNumUdpSessAddr,dev->statNumUdpSess[coreId]);
}

#endif

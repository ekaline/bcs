#ifndef _EKA_DEV_H
#define _EKA_DEV_H

// SHURIK
#include <mutex>
#include <thread>

#include "eka_hw_conf.h"
#include "EkaHwInternalStructs.h"
#include "Eka.h"
#include "Efc.h"
#include "eka_sn_addr_space.h"

class FhRunGr;
class FhGroup;
class EkaFh;
class EkaEpm;
class EkaCore;
class EkaTcpSess;
class EkaSnDev;

class EkaDev {

 public:
  EkaDev(const EkaDevInitCtx* initCtx);
  ~EkaDev();

  void     eka_write(uint64_t addr, uint64_t val);
  uint64_t eka_read(uint64_t addr);

  int         getHwCaps(hw_capabilities_t* caps);

  EkaTcpSess* findTcpSess(uint32_t ipSrc, uint16_t udpSrc, uint32_t ipDst, uint16_t udpDst);
  EkaTcpSess* findTcpSess(int sock);
  EkaTcpSess* getControlTcpSess(uint8_t coreId);
  uint8_t     findCoreByMacSa(const uint8_t* macSa);

  int      configurePort(const EkaCoreInitCtx* pCoreInit);
  uint8_t  getNumFh();

  static const uint MAX_FEED_HANDLERS       = 8;
  static const int8_t MAX_CORES               = 8;
  static const uint MAX_RUN_GROUPS          = 32;
  static const uint MAX_SESS_PER_CORE       = 64;
  static const uint CONTROL_SESS_ID         = MAX_SESS_PER_CORE;
  static const uint TOTAL_SESSIONS_PER_CORE = MAX_SESS_PER_CORE + 1;
  static const uint MAX_PKT_SIZE            = 1536;
  
  static const uint MAX_SEC_CTX             = 768*1024;
  static const uint MAX_CTX_THREADS         = 16;
  static const uint MAX_SESSION_CTX_PER_CORE = 128;

  EkaDev*                   dev = NULL; // pointer to myself
  EkaSnDev*                 snDev = NULL;

  uint8_t                   hwEnabledCores = 0;
  uint8_t                   hwFeedVer = 0;
  uint16_t                  hwRawFireSize = 0;

  EkaCore*                  core[MAX_CORES] = {};
  uint64_t                  totalNumTcpSess = 0; // for sn_state only

  EkaEpm*                   epm = NULL;

  bool                      use_vlan = false;

  volatile bool             exc_active = false;

  int64_t                   lwip_sem;
  volatile uint8_t*         txPktBuf;

  /* service_thread_params_t   serv_thr; */
  /* eka_ctx_thread_params_t   thread[EKA_MAX_CTX_THREADS]; */
  /* p4_fire_params_t          fire_params; */
  /* eka_subscr_params_t       subscr; */
  //    volatile bool             continue_sending_igmps;
  pthread_mutex_t           tcp_send_mutex;
  pthread_mutex_t           tcp_socket_open_mutex;

  std::mutex                addTcpSessMtx;
  std::mutex                lwipConnectMtx;

  struct EfcCtx*            pEfcCtx;
  struct EfcRunCtx*         pEfcRunCtx;
  struct EfhRunCtx*         pEfhRunCtx;

  void*                     credContext;
  void*                     createThreadContext;

  volatile bool             servThreadActive;
  std::thread               servThread;


  volatile bool             exc_inited = false;
  volatile bool             lwip_inited = false;
  //  volatile bool             ekaLwipPollThreadIsUp;
  volatile bool             efc_run_threadIsUp = false;
  volatile bool             efc_fire_report_threadIsUp = false;
  volatile bool             igmp_thread_active = false;

  volatile bool             io_thread_active = false;

  volatile uint8_t          numFh = 0;

  EkaFh*                    fh[MAX_FEED_HANDLERS] = {};

  volatile uint8_t          numRunGr = 0;
  FhRunGr*                  runGr[MAX_RUN_GROUPS] = {};
  std::mutex mtx;   // mutex to protect concurrent dev->numRunGr++

  EkaLogCallback            logCB;
  void*                     logCtx;

  EkaAcquireCredentialsFn   credAcquire;
  EkaReleaseCredentialsFn   credRelease;
  EkaThreadCreateFn         createThread;

  //  exc_debug_module*         exc_debug = NULL;

  bool                      print_parsed_messages = false;

  hw_capabilities_t         hw_capabilities = {};

#ifdef TEST_PRINT_DICT
  FILE* testDict;
#endif

};


/* ######################################################################## */

inline void     eka_write(EkaDev* dev, uint64_t addr, uint64_t val) { 
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
inline void copyIndirectBuf2HeapHw_swap4(EkaDev* dev, uint64_t dstLogicalAddr,uint64_t* srcAddr,uint8_t threadId, uint msgSize) {
  //  EKA_LOG("dstLogicalAddr=0x%jx, srcAddr=%p, msgSize=%u",dstLogicalAddr,srcAddr,msgSize);

  // Configuring window base WINDOW_START_POINTER (to configure it, write the pointer to address 0x81000+THREAD_ID*8)
  eka_write(dev, 0x81000+threadId*8, dstLogicalAddr); 

  uint64_t dstAddr = EpmHeapHwBaseAddr + threadId * 0x01000;

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

#endif

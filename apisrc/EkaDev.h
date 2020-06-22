#ifndef _EKA_DEV_H
#define _EKA_DEV_H

// SHURIK

#include "eka_data_structs.h"
#include "Eka.h"
#include "Efc.h"

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

  EkaTcpSess* findTcpSess(uint32_t ipSrc, uint16_t udpSrc, uint32_t ipDst, uint16_t udpDst);
  EkaTcpSess* findTcpSess(int sock);
  EkaTcpSess* getControlTcpSess(uint8_t coreId);
  uint8_t     findCoreByMacSa(const uint8_t* macSa);

  int      configurePort(const EkaCoreInitCtx* pCoreInit);
  uint8_t  getNumFh();


  enum CONF {
    MAX_FEED_HANDLERS = 8,
    MAX_CTX_THREADS   = 16,
    MAX_CORES         = 8,
    MAX_RUN_GROUPS    = 32
  };

  EkaDev*                   dev; // pointer to myself
  EkaSnDev*                 snDev;

  uint8_t                   hwEnabledCores;
  uint8_t                   hwFeedVer;
  uint16_t                  hwRawFireSize;

  EkaCore*                  core[CONF::MAX_CORES];
  uint64_t                  totalNumTcpSess; // for sn_state only

  bool                      use_vlan;

  volatile bool             exc_active;

  int64_t                   lwip_sem;
  volatile uint8_t*         txPktBuf;

  service_thread_params_t   serv_thr;
  eka_ctx_thread_params_t   thread[CONF::MAX_CTX_THREADS];
  p4_fire_params_t          fire_params;
  eka_subscr_params_t       subscr;
  //    volatile bool             continue_sending_igmps;
  pthread_mutex_t           tcp_send_mutex;
  pthread_mutex_t           tcp_socket_open_mutex;

  struct EfcCtx*            pEfcCtx;
  struct EfcRunCtx*         pEfcRunCtx;
  struct EfhRunCtx*         pEfhRunCtx;

  void*                     credContext;
  void*                     createThreadContext;

  volatile bool             servThreadActive;
  std::thread               servThread;


  volatile bool             exc_inited;
  volatile bool             lwip_inited;
  //  volatile bool             ekaLwipPollThreadIsUp;
  volatile bool             efc_run_threadIsUp;
  volatile bool             efc_fire_report_threadIsUp;
  volatile bool             igmp_thread_active;

  volatile bool             io_thread_active;

  volatile uint8_t          numFh;
  //  eka_fh*           fh[CONF::MAX_FEED_HANDLERS];
  EkaFh*                    fh[CONF::MAX_FEED_HANDLERS];

  volatile uint8_t          numRunGr;
  FhRunGr*                  runGr[CONF::MAX_RUN_GROUPS];

  EkaEpm*                   epm;

  EkaLogCallback            logCB;
  void*                     logCtx;

  EkaAcquireCredentialsFn   credAcquire;
  EkaReleaseCredentialsFn   credRelease;
  EkaThreadCreateFn         createThread;

  exc_debug_module*         exc_debug;

  bool                      print_parsed_messages;

};

typedef EkaDev eka_dev_t;  

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

#endif

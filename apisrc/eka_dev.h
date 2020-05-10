#ifndef _EKA_DEV_H
#define _EKA_DEV_H

#include "eka_data_structs.h"
#include "Eka.h"
#include "Efc.h"

class FhRunGr;
//class eka_fh;
class FhGroup;
//template <class FhGroup> 
class EkaFh;
class EkaEpm;

class EkaDev {

 public:
  EkaDev(const EkaDevInitCtx* initCtx);
  int configurePort(const EkaCoreInitCtx* pCoreInit);
  uint8_t getNumFh();
  ~EkaDev();


  enum CONF {
    MAX_FEED_HANDLERS = 8,
    MAX_CTX_THREADS   = 16,
    MAX_CORES         = 8,
    MAX_RUN_GROUPS    = 32
  };

  EkaDev*                   dev; // pointer to myself
  eka_sn_dev*               sn_dev;
  hw_pararams_t             hw;
  eka_core_params_t         core[CONF::MAX_CORES];

  eka_user_channel*         fr_ch; // new concept
  eka_user_channel*         fp_ch; // new concept
  eka_user_channel*         tcp_ch; // new concept

  volatile bool             exc_active;

  //  int                       lwip_sem;
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

#endif

#ifndef _EKA_FH_GROUP_H
#define _EKA_FH_GROUP_H
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>
#include <chrono>


#include "EkaFh.h"


class EkaFhGroup {
 protected:
  EkaFhGroup();
 public:
  virtual ~EkaFhGroup();

  int          stop();
  int          init(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx, EkaFh* fh,uint8_t gr_id,EkaSource exch);

  void         print_q_state();
  void         send_igmp(bool join_leave);

  void         createQ(EfhCtx* pEfhCtx, const uint qsize);

  virtual int  bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) = 0;
  virtual bool parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) = 0;

  void         sendFeedUp();

  //----------------------------------------------------------
  enum class GrpState { UNINIT = 0,INI, GAP, SNAPSHOT_GAP, RETRANSMIT_GAP, NORMAL, PHLX_SNAPSHOT_GAP };
  enum class DIAGNOSTICS : uint64_t { SAMPLE_PERIOD = 10000, PUSH2POP_SLACK = 10 * 1000 * 1000 /* 10 ms */ };

  GrpState              state               = GrpState::UNINIT;
  bool                  dropMe              = false;

  bool                  firstPkt            = false; // to get session_id
  volatile char         session_id[10]      = {}; // Mold Session Id

  EkaLSI                id                  = -1; // MC group ID: 1-4 for ITTO
  uint                  gapNum              = 0; // num of Gaps
  volatile uint32_t     spinImageSeq        = - 1; // used by BATS

  EkaFh*                fh                  = NULL; // parent FH

  volatile bool         thread_active       = false; // FH thread
  EfhFeedVer            feed_ver            = EfhFeedVer::kInvalid;
  EkaSource             exch                = EkaSource::kInvalid;

  volatile uint64_t     seq_after_snapshot  = -1; // set from end of snapshot

  uint64_t              expected_sequence   = -1; 
  volatile bool         heartbeat_active    = false; // Glimpse Heartbeat
  volatile bool         snapshot_active     = false; // Glimpse/Spin recovery thread
  volatile bool         recovery_active     = false; // Mold/Grp/Sesm/... thread

  volatile bool         snapshotThreadDone  = false; // thread join() replacement
  volatile bool         heartbeatThreadDone = false; // thread join() replacement

  volatile bool         gapClosed           = false;

  bool                  no_igmp             = false; // skip IGMPs for the Metamako setup

  uint32_t              mcast_ip            = -1; 
  uint16_t              mcast_port          = -1; 
  bool                  mcast_set           = false;

  uint32_t              snapshot_ip;        = -1;
  uint16_t              snapshot_port       = -1; 
  bool                  snapshot_set        = false;

  uint32_t              recovery_ip         = -1;
  uint16_t              recovery_port       = -1; 
  bool                  recovery_set        = false;

  bool                  auth_set            = false;

  int                   snapshot_sock       = -1;
  int                   recovery_sock       = -1;
  volatile uint64_t     recovery_sequence   = -1;

  uint                  hearbeat_ctr        = 0;     // for Glimse stop condition at PHLX Definition phase

  char                  auth_user[10]       = {};
  char                  auth_passwd[12]     = {};

  char                  line[2]             = -1;

  bool                  market_open         = false;

  /* std::thread           snapshot_thread; */
  /* std::thread           retransmit_thread; */
  pthread_t             snapshot_thread;
  pthread_t             retransmit_thread;


  fh_q*                 q                  = NULL;

  uint64_t              gr_ts              = -1;  // time stamp in nano
  uint64_t              seconds            = -1;  // time stamp in seconds
  EfhTradeStatus        trade_status       = EfhTradeStatus::kUninit;

  uint64_t              upd_ctr            = 0; // used for test periodic printouts

  EfhCtx*               pEfhCtx            = NULL;
  uint8_t               core               = -1;

  fh_book*              book               = NULL;
 private:
  EkaDev*               dev                = NULL;

};

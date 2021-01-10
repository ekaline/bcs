#ifndef _EKA_FH_GROUP_H_
#define _EKA_FH_GROUP_H_
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>
#include <chrono>


#include "EkaFh.h"

class fh_q;
class EkaFhBook;


class EkaFhGroup {
 protected:
  EkaFhGroup() {};

 public:
  virtual ~EkaFhGroup();

  int          stop();
  int          init(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx, EkaFh* fh,uint8_t gr_id,EkaSource exch);

  void         print_q_state();

  uint         getNumSecurities();

  void         createQ(EfhCtx* pEfhCtx, const uint qsize);

  inline void resetNoMdTimer() {
    lastMdReceived = std::chrono::high_resolution_clock::now();
  }

  virtual int  bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) = 0;
  virtual int  subscribeStaticSecurity(uint64_t        secId,
				       EfhSecurityType type,
				       EfhSecUserData  userData,
				       uint64_t        opaqueAttrA,
				       uint64_t        opaqueAttrB) = 0;
  virtual bool parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence,EkaFhMode op) = 0;

  void         sendFeedUp  (const EfhRunCtx* EfhRunCtx);
  void         sendFeedUpInitial  (const EfhRunCtx* EfhRunCtx);
  void         sendFeedDown(const EfhRunCtx* EfhRunCtx);
  void         sendFeedDownInitial(const EfhRunCtx* EfhRunCtx);
  void         sendFeedDownClosed(const EfhRunCtx* EfhRunCtx);

  void         sendNoMdTimeOut(const EfhRunCtx* EfhRunCtx);
  void         sendRetransmitExchangeError(const EfhRunCtx* pEfhRunCtx);
  void         sendRetransmitSocketError(const EfhRunCtx* pEfhRunCtx);

  virtual int closeSnapshotGap(EfhCtx*              pEfhCtx, 
			       const EfhRunCtx*  pEfhRunCtx, 
			       uint64_t          startSeq,
			       uint64_t          endSeq) {
    return 0;
  }

  virtual int closeIncrementalGap(EfhCtx*           pEfhCtx, 
				  const EfhRunCtx*  pEfhRunCtx, 
				  uint64_t          startSeq,
				  uint64_t          endSeq) {
    return 0;
  }

  virtual int processFromQ(const EfhRunCtx* pEfhRunCtx);

  /* virtual bool        processUdpPkt(const EfhRunCtx* pEfhRunCtx, */
  /* 				    const uint8_t*   pkt,  */
  /* 				    uint             msgInPkt,  */
  /* 				    uint64_t         sequence); */

  //----------------------------------------------------------
  enum class GrpState { UNINIT = 0,INIT, GAP, SNAPSHOT_GAP, RETRANSMIT_GAP, NORMAL, PHLX_SNAPSHOT_GAP };
  enum class DIAGNOSTICS : uint64_t { SAMPLE_PERIOD = 10000, PUSH2POP_SLACK = 10 * 1000 * 1000 /* 10 ms */ };

  GrpState              state               = GrpState::INIT;
  bool                  dropMe              = false;

  bool                  firstPkt            = true; // to get session_id
  volatile char         session_id[10]      = {}; // Mold Session Id

  EkaLSI                id                  = -1; // MC group ID: 1-4 for ITTO
  uint                  gapNum              = 0; // num of Gaps
  volatile uint32_t     spinImageSeq        = - 1; // used by BATS

  EkaFh*                fh                  = NULL; // parent FH

  volatile bool         thread_active       = false; // FH thread
  EfhFeedVer            feed_ver            = EfhFeedVer::kInvalid;
  EkaSource             exch                = EkaSource::kInvalid;

  volatile uint64_t     seq_after_snapshot  = -1; // set from end of snapshot

  uint64_t              expected_sequence   = 1; 
  volatile bool         heartbeat_active    = false; // Glimpse Heartbeat
  volatile bool         snapshot_active     = false; // Glimpse/Spin recovery thread
  volatile bool         recovery_active     = false; // Mold/Grp/Sesm/... thread

  volatile bool         snapshotThreadDone  = false; // thread join() replacement
  volatile bool         heartbeatThreadDone = false; // thread join() replacement

  volatile bool         gapClosed           = false;

  uint32_t              mcast_ip            = -1; 
  uint16_t              mcast_port          = -1; 
  bool                  mcast_set           = false;

  uint32_t              snapshot_ip         = -1;
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

  bool                  market_open         = false;

  /* std::thread           snapshot_thread; */
  /* std::thread           retransmit_thread; */
  pthread_t             snapshot_thread;
  pthread_t             retransmit_thread;

  uint                  numSecurities      = 0;

  std::chrono::high_resolution_clock::time_point lastMdReceived;
  bool                  lastMdReceivedValid = false;

  fh_q*                 q                  = NULL;

  uint64_t              gr_ts              = -1;  // time stamp in nano
  uint64_t              seconds            = -1;  // time stamp in seconds
  EfhTradeStatus        trade_status       = EfhTradeStatus::kUninit;

  uint64_t              upd_ctr            = 0; // used for test periodic printouts

  EfhCtx*               pEfhCtx            = NULL;
  uint8_t               core               = -1;

  //  EkaFhBook*            book               = NULL;

  EkaDev*               dev                = NULL;

  FILE*            parser_log = NULL; // used with PRINT_PARSED_MESSAGES define

 private:

};
#endif

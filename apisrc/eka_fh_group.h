#ifndef _EKA_FH_GROUP_H
#define _EKA_FH_GROUP_H
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>
#include <chrono>

#include "eka_sn_addr_space.h"
#include "eka_hw_conf.h"
#include "eka_fh_q.h"
#include "eka_fh_book.h"
#include "Efh.h"

class EkaDev;

class EkaUdpChannel;

//template <class FhGroup> class EkaFh;
class EkaFh;

//class fh_b_group {
class FhGroup {
 public:
  FhGroup();
  virtual ~FhGroup();
  int stop();
  int  init(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx, EkaFh* fh,uint8_t gr_id,EkaSource exch);

  void print_q_state();
  void send_igmp(bool join_leave);
  //  void openUdp(EfhCtx* pEfhCtx);
  void createQ(EfhCtx* pEfhCtx, const uint qsize);

  virtual int  bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) = 0;
  virtual bool parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) = 0;

  //----------------------------------------------------------
  enum class GrpState { INIT = 0, GAP, SNAPSHOT_GAP, RETRANSMIT_GAP, NORMAL, PHLX_SNAPSHOT_GAP };
  enum class DIAGNOSTICS : uint64_t { SAMPLE_PERIOD = 10000, PUSH2POP_SLACK = 10 * 1000 * 1000 /* 10 ms */ };

  GrpState     state;
  bool                  dropMe;

  bool                  firstPkt; // to get session_id
  volatile char         session_id[10]; // Mold Session Id

  uint8_t               id; // MC group ID: 1-4 for ITTO
  uint                  gapNum; // num of Gaps
  volatile uint32_t     spinImageSeq; // used by BATS

  EkaFh*       fh; // parent FH

  volatile bool         thread_active; // FH thread
  EfhFeedVer            feed_ver;
  EkaSource             exch;

  //  EkaUdpChannel*       udp_ch; -- moved to FhRunGr

  volatile uint64_t     seq_after_snapshot; // set from end of snapshot

  //  volatile uint64_t     expected_sequence; // set after every MOLD packet for the next one
  uint64_t              expected_sequence; 
  volatile bool         heartbeat_active; // Glimpse Heartbeat
  volatile bool         snapshot_active; // Glimpse/Spin recovery thread
  volatile bool         recovery_active; // Mold/Grp/Sesm/... thread

  volatile bool         snapshotThreadDone; // thread join() replacement
  volatile bool         heartbeatThreadDone; // thread join() replacement

  volatile bool         gapClosed;

  bool                  no_igmp; // skip IGMPs for the Metamako setup

  uint32_t              mcast_ip; 
  uint16_t              mcast_port; 
  bool                  mcast_set;

  uint32_t              snapshot_ip;         // Glimpse or Spin
  uint16_t              snapshot_port; 
  bool                  snapshot_set;

  uint32_t              recovery_ip;         // Mold or GRP
  uint16_t              recovery_port; 
  bool                  recovery_set;

  bool                  auth_set;

  int                   snapshot_sock;    // Glimpse or Spin
  int                   recovery_sock;    // Mold or GRP
  volatile uint64_t     recovery_sequence;// From SoupBin recovery 

  uint                  hearbeat_ctr;     // for Glimse stop condition at PHLX Definition phase

  /* char                  auth_user[6];     // snapshot */
  /* char                  auth_passwd[10];  // snapshot */

  char                  auth_user[10];     // snapshot
  char                  auth_passwd[12];  // snapshot

  char                  line[2]; // used for BOX

  bool                  market_open; // used for BATS

  /* std::thread           snapshot_thread; */
  /* std::thread           retransmit_thread; */
  pthread_t           snapshot_thread;
  pthread_t           retransmit_thread;


  fh_q*                 q;

  uint64_t              gr_ts;  // time stamp in nano
  uint64_t              seconds;  // time stamp in seconds
  EfhTradeStatus        trade_status;

  uint64_t              upd_ctr; // used for test periodic printouts

  //  volatile uint64_t     exp_seq;
  uint64_t              exp_seq;

  EfhCtx*               pEfhCtx;
  EkaDev*               dev;
  uint8_t               core;

  fh_book*              book;
 private:
};

 /* ##################################################################### */

class FhNasdaqGr : public FhGroup{
 public:
  virtual             ~FhNasdaqGr() {};
};

/* ##################################################################### */

class FhNomGr : public FhNasdaqGr{
  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op);
  virtual              ~FhNomGr() {};
  int                  bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);

  static const bool full_book = true;
};

/* ##################################################################### */

class FhGemGr : public FhNasdaqGr{
  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op);
  virtual              ~FhGemGr() {};
  int                  bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);

  static const bool    full_book = false;
};

/* ##################################################################### */

class FhPhlxGr : public FhNasdaqGr{
  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op);
  virtual               ~FhPhlxGr() {};
  int                   bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);

  static const bool full_book = false;
};

 /* ##################################################################### */

class FhBatsGr : public FhGroup{
 public:
  FhBatsGr();
  virtual               ~FhBatsGr() {};
  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op);
  int                   bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);

  char                  sessionSubID[4];  // for BATS Spin
  uint8_t               batsUnit;
  int                   grp_sock; // MC retransmit socket

  static const bool full_book = true;
};

/* ##################################################################### */

class FhMiaxGr : public FhGroup{
 public:
  FhMiaxGr();
  bool     parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op);
  virtual  ~FhMiaxGr() {};
  int      bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);

  static const bool full_book = false;
};

/* ##################################################################### */

class FhBoxGr : public FhGroup{
 public:
  FhBoxGr();
  bool     parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
    return true;
  }
  bool     parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint* msgLen,EkaFhMode op);
  virtual  ~FhBoxGr() {};
  int      bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);

  static const bool full_book = false;
  uint64_t txSeqNum;
};


/* ##################################################################### */

class FhXdpGr : public FhGroup{
 public:
  static const uint MAX_STREAMS = 10;
  static const uint MAX_UNDERLYINGS = 512;
  static const uint MAX_SERIES = 96000;

  FhXdpGr();
  bool     parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op);
  virtual  ~FhXdpGr() {};
  int      bookInit(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx);

  static const bool full_book = false;

  inline uint     findAndInstallStream(uint streamId, uint32_t curSeq) {
    for (uint i = 0; i < numStreams; i ++) if (stream[i]->getId() == streamId) return i;
    if (numStreams == MAX_STREAMS) on_error("numStreams == MAX_STREAMS (%u), cant add stream %u",numStreams,streamId);
    stream[numStreams] = new Stream(streamId,curSeq);
    return numStreams++;
  }
  inline uint32_t getExpectedSeq(uint streamIdx) {
    return stream[streamIdx]->getExpectedSeq();
  }
  inline uint32_t setExpectedSeq(uint streamIdx,uint32_t seq) {
    return stream[streamIdx]->setExpectedSeq(seq);
  }
  inline void     setGapStart() {
    gapStart = std::chrono::high_resolution_clock::now();
    return;
  }
  inline bool     isGapOver() {
    auto finish = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(finish-gapStart).count() > 120) return true;
    return false;
  }
  inline uint     getId(uint streamIdx) {
    return stream[streamIdx]->getId();
  }
  inline uint32_t resetExpectedSeq(uint streamIdx) {
    return stream[streamIdx]->resetExpectedSeq();
  }
  bool     inGap;

  class Stream {
  public:
    Stream(uint strId,uint32_t seq) {
      id = strId;
      expectedSeq = seq;
    }
    inline uint     getId() {
      return id;
    }
    inline uint32_t getExpectedSeq() {
      return expectedSeq;
    }
    inline uint32_t setExpectedSeq(uint32_t seq) {
      return (expectedSeq = seq);
    }
    inline uint32_t resetExpectedSeq() {
      return (expectedSeq = 2);
    }

  private:
    uint     id;
    uint32_t expectedSeq;
  };

 private:
  Stream*  stream[MAX_STREAMS];
  uint     numStreams;

  std::chrono::high_resolution_clock::time_point gapStart;

  uint32_t underlyingIdx[MAX_UNDERLYINGS];
  uint     numUnderlyings;
  char     symbolStatus[MAX_UNDERLYINGS];
  char     seriesStatus[MAX_SERIES];
};
#endif

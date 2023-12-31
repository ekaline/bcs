#ifndef _EKA_FH_GROUP_H_
#define _EKA_FH_GROUP_H_
#include <chrono>
#include <inttypes.h>
#include <stdint.h>
#include <thread>
#include <unistd.h>

#include "EkaFh.h"
#include "EkaNwParser.h"

#include "EkaFhBook.h"

class fh_q;
class EkaFhBook;
class EkaFhRunGroup;

class EkaFhGroup {
protected:
  EkaFhGroup();

public:
  virtual ~EkaFhGroup();

  int stop();
  int init(EfhCtx *pEfhCtx, const EfhInitCtx *pInitCtx,
           EkaFh *fh, uint8_t gr_id, EkaSource exch);

  virtual void print_q_state();

  uint getNumSecurities();

  void createQ(EfhCtx *pEfhCtx, const uint qsize);

  inline void resetNoMdTimer() {
    //    lastMdReceived =
    //    std::chrono::high_resolution_clock::now();
    gotPkt = true;
    pktCnt++;
  }

  virtual int bookInit() = 0;
  virtual void printBookState() {}
  virtual int subscribeStaticSecurity(
      uint64_t secId, EfhSecurityType type,
      EfhSecUserData userData, uint64_t opaqueAttrA,
      uint64_t opaqueAttrB) = 0;
  virtual bool
  parseMsg(const EfhRunCtx *pEfhRunCtx,
           const unsigned char *m, uint64_t sequence,
           EkaFhMode op,
           std::chrono::high_resolution_clock::time_point
               startTime = {}) = 0;

  void sendFeedUp(const EfhRunCtx *EfhRunCtx);
  void sendProgressingFeedUp(const EfhRunCtx *EfhRunCtx);
  void sendFeedUpInitial(const EfhRunCtx *EfhRunCtx);
  void sendFeedDown(const EfhRunCtx *EfhRunCtx);
  void sendFeedDownInitial(const EfhRunCtx *EfhRunCtx);
  void sendFeedDownStaleData(const EfhRunCtx *EfhRunCtx);
  void sendFeedDownClosed(const EfhRunCtx *EfhRunCtx);

  void sendNoMdTimeOut(const EfhRunCtx *EfhRunCtx);
  void
  sendRetransmitExchangeError(const EfhRunCtx *pEfhRunCtx,
                              bool sleepAfterCb = true);
  void
  sendRetransmitSocketError(const EfhRunCtx *pEfhRunCtx,
                            bool sleepAfterCb = true);
  void sendBackInTimeEvent(const EfhRunCtx *pEfhRunCtx,
                           uint64_t badSequence);

  virtual int closeSnapshotGap(EfhCtx *pEfhCtx,
                               const EfhRunCtx *pEfhRunCtx,
                               uint64_t startSeq,
                               uint64_t endSeq) {
    return 0;
  }

  virtual int
  closeIncrementalGap(EfhCtx *pEfhCtx,
                      const EfhRunCtx *pEfhRunCtx,
                      uint64_t startSeq, uint64_t endSeq) {
    return 0;
  }

  virtual int processFromQ(const EfhRunCtx *pEfhRunCtx);

  int credentialAcquire(EkaCredentialType credType,
                        const char *credName,
                        size_t credNameSize,
                        EkaCredentialLease **lease);
  int credentialRelease(EkaCredentialLease *lease);

  inline bool skipSnapshot() const {
    // Dont get initial recovery for pure Trades or Auction
    // groups
    if (productMask == ProductMask::PM_VanillaTrades ||
        productMask == ProductMask::PM_ComplexTrades ||
        productMask == ProductMask::PM_VanillaAuction ||
        productMask == ProductMask::PM_ComplexAuction)
      return true;
    return false;
  }
  inline bool skipDefinitions() const {
#if 0
    // Dont get Definitions for pure Trades or Auction groups
    if (productMask == ProductMask::PM_VanillaTrades  ||
	productMask == ProductMask::PM_ComplexTrades  ||
	productMask == ProductMask::PM_VanillaAuction ||
	productMask == ProductMask::PM_ComplexAuction)
      return true;
#endif
    return false;
  }

  virtual int printConfig() {
    char productNamesBuf[MaxProductMaskNameBufSize];
    EKA_LOG(
        "%s:%u : "
        "productMask: \'%s\' (0x%x) "
        "MCAST: %s:%u, "
        "SNAPSHOT: %s:%u, "
        "RECOVERY: %s:%u, "
        "AUTH: %s:%s, "
        "connectRetryDelayTime=%d, "
        "staleDataNsThreshold=%ju",
        EKA_EXCH_DECODE(exch), id,
        lookupProductMaskNames(productMask,
                               productNamesBuf),
        productMask, EKA_IP2STR(mcast_ip), mcast_port,
        EKA_IP2STR(snapshot_ip), be16toh(snapshot_port),
        EKA_IP2STR(recovery_ip), be16toh(recovery_port),
        auth_set ? std::string(auth_user, sizeof(auth_user))
                       .c_str()
                 : "NOT SET",
        auth_set
            ? std::string(auth_passwd, sizeof(auth_passwd))
                  .c_str()
            : "NOT SET",
        connectRetryDelayTime, staleDataNsThreshold);
    return 0;
  }

  inline const char *printGrpState() const {
    switch (state) {
    case GrpState::UNINIT:
      return "UNINIT";
    case GrpState::INIT:
      return "INIT";
    case GrpState::GAP:
      return "GAP";
    case GrpState::SNAPSHOT_GAP:
      return "SNAPSHOT_GAP";
    case GrpState::RETRANSMIT_GAP:
      return "RETRANSMIT_GAP";
    case GrpState::NORMAL:
      return "NORMAL";
    case GrpState::PHLX_SNAPSHOT_GAP:
      return "PHLX_SNAPSHOT_GAP";
    case GrpState::GAP_IN_GAP:
      return "GAP_IN_GAP";
    default:
      return "UNKNOWN";
    }
  }

  void startTrackingGapInGap(uint64_t sequence,
                             uint msgInPkt) {
    expectedSeqGapInGap = sequence + msgInPkt;
    EKA_LOG("%s:%u: expectedSeqGapInGap at %s =  %ju",
            EKA_EXCH_DECODE(exch), id, printGrpState(),
            expectedSeqGapInGap);
  }

  inline bool hasGapInGap(uint64_t sequence,
                          uint msgInPkt) {
    if (expectedSeqGapInGap == 0) {
      expectedSeqGapInGap = sequence + msgInPkt;
      EKA_LOG(
          "%s:%u: first expectedSeqGapInGap at %s =  %ju",
          EKA_EXCH_DECODE(exch), id, printGrpState(),
          expectedSeqGapInGap);
      return false;
    }
    if (sequence < expectedSeqGapInGap) {
      on_error("%s:%u BACK-IN-TIME at %s: sequence %ju < "
               "expectedSeqGapInGap %ju",
               EKA_EXCH_DECODE(exch), id, printGrpState(),
               sequence, expectedSeqGapInGap);
    }
    if (sequence > expectedSeqGapInGap) {
      EKA_LOG("%s:%u: Gap-In-Gap at %s: "
              "expectedSeqGapInGap %ju != sequence %ju, "
              "lost %ju",
              EKA_EXCH_DECODE(exch), id, printGrpState(),
              expectedSeqGapInGap, sequence,
              sequence - expectedSeqGapInGap);
      expectedSeqGapInGap = 0;
      return true;
    }
    expectedSeqGapInGap = sequence + msgInPkt;
    return false;
  }

  static constexpr int
  getDefinitionProductMask(EfhSecurityType defType) {
    switch (defType) {
    case EfhSecurityType::kOption:
      return PM_VanillaALL;
    case EfhSecurityType::kComplex:
      return PM_ComplexALL;
    case EfhSecurityType::kFuture:
      return PM_FutureALL;
    default:
      return PM_NoInfo;
    }
  }

  constexpr bool
  isDefinitionPassive(EfhSecurityType defType) {
    int definitionProductMask =
        getDefinitionProductMask(defType);
    return definitionProductMask && productMask &&
           !(productMask & definitionProductMask);
  }

  std::string getDefinitionsFileName() const {
    const int MAXLEN = 10;
    char s[MAXLEN] = {};
    time_t t = time(0);
    strftime(s, MAXLEN, "%Y%m%d", localtime(&t));
    return std::string("definitionsFile") + '.' +
           std::string(EKA_EXCH_DECODE(exch)) +
           std::to_string(id) + '.' + std::string(s);
  }

  //----------------------------------------------------------
  enum class GrpState {
    UNINIT = 0,
    INIT,
    GAP,
    SNAPSHOT_GAP,
    RETRANSMIT_GAP,
    NORMAL,
    PHLX_SNAPSHOT_GAP,
    GAP_IN_GAP
  };
  enum class DIAGNOSTICS : uint64_t {
    SAMPLE_PERIOD = 10000,
    PUSH2POP_SLACK = 10 * 1000 * 1000 /* 10 ms */
  };

  GrpState state = GrpState::INIT;
  bool dropMe = false;

  int connectRetryDelayTime = 15;

  EkaLSI id = -1;  // MC group ID: 1-4 for ITTO
  uint gapNum = 0; // num of Gaps
#ifdef _EKA_SKIP_RECOVERY
  uint gapsLimit = 1; // num of Gaps to switch to
  // "Progressing Recovery"
  // 0 - means never skip
#else
  uint gapsLimit = 0;
#endif
  EkaFh *fh = NULL; // parent FH

  volatile bool thread_active = false; // FH thread
  EfhFeedVer feed_ver = EfhFeedVer::kInvalid;
  EkaSource exch = EkaSource::kInvalid;

  volatile uint64_t seq_after_snapshot =
      0; // set from end of snapshot

  uint64_t expected_sequence = 1;
  volatile bool heartbeat_active =
      false; // Glimpse Heartbeat
  volatile bool snapshot_active =
      false; // Glimpse/Spin recovery thread
  volatile bool recovery_active =
      false; // Mold/Grp/Sesm/... thread

  volatile bool recoveryThreadDone =
      false; // thread join() replacement
  volatile bool heartbeatThreadDone =
      false; // thread join() replacement

  volatile bool gapClosed = false;

  uint32_t mcast_ip = -1;
  uint16_t mcast_port = -1;
  bool mcast_set = false;

  uint32_t snapshot_ip = -1;
  uint16_t snapshot_port = -1;
  bool snapshot_set = false;

  uint32_t recovery_ip = -1;
  uint16_t recovery_port = -1;
  bool recovery_set = false;

  bool auth_set = false;

  int snapshot_sock = -1;
  int recovery_sock = -1;
  volatile uint64_t recovery_sequence = -1;

  uint hearbeat_ctr = 0; // for Glimse stop condition at
                         // PHLX Definition phase

  char auth_user[10] = {};
  char auth_passwd[12] = {};

  bool market_open = false;

  /* std::thread           snapshot_thread; */
  /* std::thread           retransmit_thread; */
  pthread_t snapshot_thread;
  pthread_t retransmit_thread;

  uint numSecurities = 0;

  std::chrono::high_resolution_clock::time_point
      lastMdReceived;
  bool lastMdReceivedValid = false;
  bool gotPkt = false;

  fh_q *q = NULL;

  uint64_t gr_ts = 0;   // time stamp in nano
  uint64_t seconds = 0; // time stamp in seconds
  EfhTradeStatus trade_status = EfhTradeStatus::kUninit;

  uint64_t upd_ctr = 0; // used for test periodic printouts
  uint64_t pktCnt = 0;  // for monitoring by efh_state
  EfhCtx *pEfhCtx = NULL;
  uint8_t core = -1;

  EkaDev *dev = NULL;
  EkaFhRunGroup *runGr = NULL;

  // 1s = 1'000'000'000;
  uint64_t staleDataNsThreshold = 0;
  uint64_t tobUpdatesCnt = 0;

  FILE *parser_log =
      NULL; // used with PRINT_PARSED_MESSAGES define

  uint64_t parserSeq = 0; // used for the sanity check
  int productMask = PM_NoInfo;

  uint64_t firstMcSeq = 0; //

  uint64_t expectedSeqGapInGap = 1;

  bool credentialsAcquired = false;

  bool useDefinitionsFile = false;

  bool initialGapClosed = false;

  const int StaleDataSampleRate = 16 * 1024;

  int mdCheckStartHour = 9;
  int mdCheckStartMinute = 0;

  int mdCheckEndHour = 16;
  int mdCheckEndMinute = 30;

  uint32_t recoverySrcIp =
      0; // used to specify Solarflare NIC I/F for CME
         // recovery by UDP MC

  EfhExchangeErrorCode lastExchErr =
      EfhExchangeErrorCode::kNoError;

#ifdef EFH_INTERNAL_LATENCY_CHECK
  std::vector<EfhLatencyStat> latencies = {};
#endif

private:
protected:
};
#endif

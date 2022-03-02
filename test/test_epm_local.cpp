#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <optional>
#include <pthread.h>
#include <stdio.h>
#include <sstream>

#include <compat/Eka.h>
#include <compat/Epm.h>
#include <compat/Exc.h>

using s8  = int8_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

namespace {

#define ERROR(fmt, ...) do {} while (0)

static bool isResultOk(EkaOpResult result) {
  return result == EKA_OPRESULT__OK || result == EKA_OPRESULT__ALREADY_INITIALIZED;
}

template <typename T>
T alignUpTo(T value, T alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

template <typename T>
std::string toHexString(T t) {
  std::stringstream stream{"0x"};
  stream << std::hex << t;
  return stream.str();
}

class Trigger {
 public:
  Trigger(const EpmTriggerParams &epmTrigger)
      : Trigger(epmTrigger.coreId, epmTrigger.mcIp, epmTrigger.mcUdpPort) {}

  Trigger(int portIdx, const char *inaddr, u16 udpPort)
      : portIdx_{(s8)portIdx}, multicastIP_{inaddr}, multicastPort_{udpPort} {}

  bool isValid() const {
    struct in_addr inaddr;
    return portIdx_ >= 0 && portIdx_ < 4 &&
           multicastPort_ >= 1024 &&
           inet_aton(multicastIP_.c_str(), &inaddr) != 0;
  }

  EpmTriggerParams build() const {
    return makeCopy(peek());
  }

  static EpmTriggerParams makeCopy(const EpmTriggerParams &epmParam) {
    EpmTriggerParams copy{epmParam};
    copy.mcIp = strdup(copy.mcIp);
    return copy;
  }

 private:
  EpmTriggerParams peek() const {
    assert(isValid());
    return EpmTriggerParams{(s8)portIdx_, multicastIP_.data(), multicastPort_};
  }

  s8 portIdx_;
  std::string multicastIP_;
  u16 multicastPort_;
};

class HeapManager {
 public:
  bool initialize(EkaDev *device) {
    TotalHeapSize = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_PayloadMemorySize);
    MessageAlignment = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_PayloadAlignment);
    if (MessageAlignment == 0) MessageAlignment = 1;
    MessageHeaderSize = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_DatagramOffset);
    MessageTrailerSize = epmGetDeviceCapability(device, EpmDeviceCapability::EHC_RequiredTailPadding);
    current_ = 0;
    return TotalHeapSize > 0 &&
           MessageAlignment > 0 &&
           MessageHeaderSize >= 0 &&
           MessageTrailerSize >= 0;
  }
  u32 offset() const { return current_; }
  u32 insert(u32 size) {
    assert(current_ % MessageAlignment == 0);
    u32 totalSize = MessageHeaderSize + size + MessageTrailerSize;
    u32 alignedSize = alignUpTo(totalSize, MessageAlignment);
    current_ += alignedSize;
    return current_;
  }

 private:
  u32 current_ = 0;

  u32 TotalHeapSize = 0;
  u32 MessageAlignment = -1;
  u32 MessageHeaderSize = -1;
  u32 MessageTrailerSize = -1;
};

class Message {
 public:
  Message(const std::string payload, int heapOffset = -1)
      : payload_(payload), heapOffset_(heapOffset) {}

  u32 heapOffset() const { return heapOffset_; }
  void setHeapOffset(int offset) { heapOffset_ = offset; }
  u32 size() const { return payload_.size(); }
  void const *data() const { return payload_.data(); }
  const std::string &payload() const { return payload_; }

 private:
  std::string payload_;
  u32 heapOffset_;
};
std::ostream &operator<<(std::ostream &out, const Message &msg) {
  out << "Message{" << msg.size() << "@" << msg.heapOffset() << " : '"
      <<  msg.payload() << "'}";
  return out;
}

class Action {
 public:
  static constexpr epm_token_t ACTION_TOKEN = 0xf001cafedeadbabe;
  using bitsT = epm_enablebits_t;
  Action(EpmActionType type, ExcConnHandle connection, Message message,
         u32 flags, bitsT enableBits, bitsT actionMask, bitsT strategyMask, uintptr_t cookie)
      : type_(type), connection_(connection), message_(message), flags_(flags),
        enableBits_(enableBits), actionMask_(actionMask), strategyMask_(strategyMask) {}

  Message &message() { return message_; }
  const Message &message() const { return message_; }
  EpmAction build() const {
    EpmAction action{
      .type = type_,
      .token = ACTION_TOKEN,
      .hConn = connection_,
      .offset = message_.heapOffset(),
      .length = message_.size(),
      .actionFlags = flags_,
      .nextAction = -1,
      .enable = enableBits_,
      .postLocalMask = actionMask_,
      .postStratMask = strategyMask_,
      .user = cookie_
    };
    return action;
  }

 private:
  EpmActionType type_;
  ExcConnHandle connection_;
  Message message_;
  u32 flags_;
  bitsT enableBits_;
  bitsT actionMask_;
  bitsT strategyMask_;
  uintptr_t cookie_;
};

class Strategy {
 public:
  using ReportCbFn = EpmFireReportCb;
  using ActionChain = std::vector<Action>;
  using Scenario = std::pair<Trigger, ActionChain>;
  using bitsT = epm_enablebits_t;

  Strategy(ReportCbFn reportCallback, void *cookie, bitsT enableBits)
      : callback_(reportCallback), cookie_(cookie), enableBits_(enableBits) {}

  int createScenario(Trigger trigger, std::vector<Action> &&actions = {}) {
    scenarios_.emplace_back(trigger, std::move(actions));
    return scenarios_.size() - 1;
  }
  bool addAction(u32 scenarioIdx, Action action) {
    assert(scenarioIdx < scenarios_.size());
    if (scenarioIdx >= scenarios_.size())
      return false;
    scenarios_[scenarioIdx].second.push_back(action);
    return true;
  }

  const Scenario &scenario(int idx) const { return scenarios_.at(idx); }
  const Trigger &trigger(int scenarioIdx) const {
    return scenarios_.at(scenarioIdx).first;
  }
  Action &action(int scenarioIdx, int actionIdx) {
    assert(scenarioIdx < scenarios_.size());
    Scenario &scenario{scenarios_[scenarioIdx]};
    assert(actionIdx < scenario.second.size());
    return scenario.second[actionIdx];
  }
  const Scenario *getScenarioForAction(int &combinedActionNo) {
    assert(combinedActionNo >= 0);
    int scenarioIdx = 0;
    int combinedActionCount = 0;
    while (scenarioIdx < scenarios_.size() &&
           combinedActionNo >= combinedActionCount + scenarios_[scenarioIdx].second.size()) {
      combinedActionCount += scenarios_[scenarioIdx].second.size();
      ++scenarioIdx;
    }
    if (scenarioIdx < scenarios_.size()) {
      combinedActionNo -= combinedActionCount;
      return &scenarios_.at(scenarioIdx);
    }
    return nullptr;
  }
  bool validate() {
    // TODO
    return false;
  }

  bool placeMessages(HeapManager &heap) {
    for (auto &scenario : scenarios_) {
      ActionChain &actions{scenario.second};
      for (auto &action : actions) {
        action.message().setHeapOffset(heap.offset());
        if (!heap.insert(action.message().size()))
          return false;
      }
    }
    return true;
  }
  std::tuple<EpmStrategyParams, std::vector<EpmTriggerParams>, std::vector<EpmAction>, bitsT>
      build() const {
    std::vector<EpmTriggerParams> epmTriggers;
    std::vector<EpmAction> epmActions;
    // Copy all triggers and the actions they directly invoke
    for (auto &scenario : scenarios_) {
      epmTriggers.push_back(scenario.first.build());
      assert(scenario.second.size() > 0);
      epmActions.push_back(scenario.second[0].build());
      epmActions.back().nextAction = EPM_LAST_ACTION;
    }
    // Copy the rest of the action chains (the first action from each chain has been handled above)
    for (int idx = 0; idx < scenarios_.size(); ++idx) {
      int lastActionInChainIdx = idx;
      auto &actionChain = scenarios_[idx].second;
      for (int inChainIdx = 1; inChainIdx < actionChain.size(); ++inChainIdx) {
        int thisActionIdx = epmActions.size();
        epmActions[lastActionInChainIdx].nextAction = thisActionIdx;
        lastActionInChainIdx = thisActionIdx;
        epmActions.push_back(actionChain[inChainIdx].build());
      }
      epmActions.back().nextAction = EPM_LAST_ACTION;
    }

    EpmStrategyParams epmStrategy{
        .numActions = (int)epmActions.size(),
        .triggerParams = epmTriggers.data(), //malloc(scenarios_.size() * sizeof(EpmTriggerParams)),
        .numTriggers = epmTriggers.size(),
        .reportCb = callback_,
        .cbCtx = cookie_
    };
    return std::tuple<EpmStrategyParams, std::vector<EpmTriggerParams>, std::vector<EpmAction>, bitsT>{
      epmStrategy, std::move(epmTriggers), std::move(epmActions), enableBits_
    };
  }

 private:
  ReportCbFn  callback_;
  void       *cookie_;
  bitsT       enableBits_;
  std::vector<Scenario> scenarios_;
};

inline std::ostream &operator<<(std::ostream &o, const EpmTriggerParams &trg) {
  return o << "EpmTrigger{ phyPort:" << trg.coreId << ", UDP: " << trg.mcIp << ":" << trg.mcUdpPort << " }";
}

bool operator==(const EpmTriggerParams &a, const EpmTriggerParams &b) {
  return a.coreId == b.coreId &&
         a.mcUdpPort == b.mcUdpPort &&
         std::string(a.mcIp).compare(b.mcIp) == 0;
}
bool operator!=(const EpmTriggerParams &a, const EpmTriggerParams &b) {
  return !(a == b);
}

struct TriggerFireEvent {
  using bitsT = epm_enablebits_t;

  TriggerFireEvent(const EpmTriggerParams &triggerParams,
                   ExcConnHandle hConnection, const Message &message,
                   u32 flags, bitsT enableBits, bitsT actionMask, bitsT strategyMask,
                   uintptr_t reportCookie, bool localFire)
      : triggerParams(triggerParams), connection(hConnection), message(&message), flags(flags), enableBits(enableBits),
        actionMask(actionMask), strategyMask(strategyMask), reportCookie(reportCookie),
        localTrigger(localFire), reportIndex(-1) {}

  EpmTriggerParams triggerParams;
  ExcConnHandle connection;
  const Message *message;
  u32 flags;
  bitsT enableBits;
  bitsT actionMask;
  bitsT strategyMask;
  uintptr_t reportCookie;
  bool localTrigger;
  int reportIndex;
};
std::ostream &operator<<(std::ostream &out, const TriggerFireEvent &event) {
  out << "{ idx: " << event.reportIndex << ", trigger: " << event.triggerParams << ", hConn " << event.connection << ", msg " << *event.message
      << ", flags " << toHexString(event.flags) << ", enable " << toHexString(event.enableBits)
      << ", action mask: " << toHexString(event.actionMask) << ", strategy mask: " << toHexString(event.strategyMask)
      << ", action cookie: " << toHexString(event.reportCookie) << "}";
  return out;
}
std::ostream &operator<<(std::ostream &out, const EpmTriggerAction &action) {
  std::string_view value;
  switch (action) {
  case Unknown:
    value = "Unknown"; break;
  case Sent:
    value = "Sent"; break;
  case InvalidToken:
    value = "InvalidToken"; break;
  case InvalidStrategy:
    value = "InvalidStrategy"; break;
  case InvalidAction:
    value = "InvalidAction"; break;
  case DisabledAction:
    value = "DisabledAction"; break;
  case SendError:
    value = "SendError"; break;
  }
  return out << value;
}
std::ostream &operator<<(std::ostream &out, const EpmFireReport &r) {
  out << "EpmFire{trigger: " << (r.local ? "local" : "HW") << ", action: " << r.action
      << ", strategy_id: " << r.strategyId << ", action_id: " << r.actionId
      << "\n\tactionTrigger: " << "_" << ", result: " << r.error
      <<"\n\taction masks {pre " << toHexString(r.preLocalEnable) << ", post " << toHexString(r.postLocalEnable)
      << " }, startegy masks {pre" << r.preStratEnable << ", post " << r.postStratEnable << " }"
      << "\n\tuser cookie: " << r.user << " }";
  return out;
}

class EkalinePMFixture : public ::testing::Test {
 public:
  using bitsT = epm_enablebits_t;
  using Scenario = Strategy::Scenario;

  static constexpr int MAX_ACTIONS_PER_TRIGGER = 16;

  EkalinePMFixture() : ekaDevice_{nullptr, releaseDevice} {}

  EkaDev *device() { assert(ekaDevice_ != nullptr); return ekaDevice_.get(); }
  bool initDevice() {
    EkaDev *device = nullptr;
    EkaDevInitCtx initCtx {
      .logCallback = nullptr, // TODO(twozniak): supply this, maybe
      .logContext = nullptr,  // TODO(twozniak): supply this, maybe
      .credAcquire = nullptr,
      .credRelease = nullptr,
      .credContext = nullptr,
      .createThread = &EkalinePMFixture::createThread,
      .createThreadContext = nullptr,
    };
    EkaOpResult result = ekaDevInit(&device, &initCtx);
    return isResultOk(result);
  }
  bool initPort() {
    EkaCoreInitCtx portInitCtx{
      .coreId = phyPort,
      .attrs = { .host_ip = inet_addr(ekalineIPAddr_.c_str()),
                 .netmask = inet_addr(ekalineIPMask_.c_str()),
                 .gateway = inet_addr(ekalineIPGateway_.c_str()) }
    };
    EkaOpResult result = ekaDevConfigurePort(device(), &portInitCtx);
    return isResultOk(result);
  }

  Strategy &addStrategy(bitsT enableBits) {
    strategies_.emplace_back(fireReportCallbackShim, this, enableBits);
    return strategies_.back();
  }

  struct EpmStrategyBuilder {
    using bitsT = epm_enablebits_t;

    EpmStrategyBuilder(EkalinePMFixture *fixture) : fixture{fixture} {}

    bool build(std::vector<Strategy> &strategies_) {
      HeapManager heap;
      heap.initialize(fixture->device());
//    auto appendMovedVector = template<typename T> [](std::vector<T> &dst, std::vector<T> &&src) {
//      auto oldDstSize = dst.size();
//      dst.resize(oldDstSize + src.size());
//      for (size_t idx = 0; idx < src.size(); ++idx)
//      std::swap(dst[oldDstSize + idx], src[idx]);
//    };
      for (auto &strategy : strategies_) {
        bool result = strategy.placeMessages(heap);
        if (!result)
          return false;
          // Bad
      }

      for (auto &strategy : strategies_) {
        auto tuple{strategy.build()};
        epmStrategies.push_back(std::get<0>(tuple));
        epmTriggers.push_back(std::move(std::get<1>(tuple)));
        epmActions.push_back(std::move(std::get<2>(tuple)));
//      appendMovedVector(epmTriggers, std::get<1>(tuple));
//      appendMovedVector(epmActions, std::get<2>(tuple));

        enableBits.push_back(std::get<3>(tuple));
      }
      return true;
    }

    EkalinePMFixture *fixture;
    std::vector<EpmStrategyParams> epmStrategies;
    std::vector<std::vector<EpmTriggerParams>> epmTriggers;
    std::vector<std::vector<EpmAction>> epmActions;
    std::vector<bitsT> enableBits;
  };
  bool deployStrategies() {
    assert(bound_);
    assert(connected_);

    EpmStrategyBuilder builder(this);
    if (!builder.build(strategies_)) {
      ERROR("Failed to serialize EPM strategies for ekaline API");
      return false;
    }

    if (!isResultOk(epmEnableController(device(), phyPort, false))) {
      // Warn
    }

    auto result = epmInitStrategies(device(), builder.epmStrategies.data(), builder.epmStrategies.size());
    if (!isResultOk(result)) {
      // Bad
      return false;
    }

    bool failed = false;
    for (int strategyIdx = 0; !failed && (strategyIdx < builder.epmStrategies.size()); ++strategyIdx) {
      Strategy &strategy = strategies_[strategyIdx];
      auto &strategyActions{builder.epmActions[strategyIdx]};
      for (int actionIdx = 0; actionIdx < strategyActions.size(); ++actionIdx) {
        EpmAction &epmAction{strategyActions[actionIdx]};
        int actionIndexInScenario = actionIdx;
        const Scenario *scenario = strategy.getScenarioForAction(actionIndexInScenario);
        if (!scenario) {
          // Bad
        }
        const Message &message{scenario->second.at(actionIndexInScenario).message()};
        if (!isResultOk(epmPayloadHeapCopy(device(), strategyIdx, message.heapOffset(), message.size(), message.data()))) {
          // Bad
          failed = true;
          break;
        }
        if (!isResultOk(epmSetAction(device(), strategyIdx, actionIdx, &epmAction))) {
          // Bad
          failed = true;
          break;
        }
      }
      if (!isResultOk(epmSetStrategyEnableBits(device(), strategyIdx, builder.enableBits[strategyIdx]))) {
        // Bad
        failed = true;
        break;
      }
    }

    if (!failed) {
      deployed_ = isResultOk(epmEnableController(device(), phyPort, true));
      if (!deployed_)
        // Bad
        ;
    }

    return deployed_;
  }

  void fireReportCallback(const EpmFireReport *reports, int nReports) {
    for (int idx = 0; idx < nReports; ++idx) {
      EpmFireReport const &report = reports[idx];

      assert(report.trigger != nullptr);
      int reportStrategyId = report.trigger->strategy;
      assert(reportStrategyId == report.strategyId);
      int reportActionId = report.trigger->action;
      assert(reportActionId == report.actionId);

      fireReports_.push_back(report);
    }
  }
  static void fireReportCallbackShim(const EpmFireReport *reports, int nReports, void *ctx) {
    EkalinePMFixture *thiz = reinterpret_cast<EkalinePMFixture *>(ctx);
    thiz->fireReportCallback(reports, nReports);
  }

  typedef void *(*threadFn)(void*);
  static int createThread(const char* name, EkaServiceType, threadFn task, void *arg, void *, uintptr_t *handle) {
    pthread_attr_t attr{};
    pthread_t tid;
    if (pthread_attr_init(&attr))
      return -1;

    int result = pthread_create(&tid, &attr, task, arg);
    pthread_attr_destroy(&attr);
    pthread_setname_np(tid, name);
    *handle = (uintptr_t)tid;
    return result;
  }

  static void releaseDevice(EkaDev *device) { ekaDevClose(device); }

  // Channel ID 310, Label "CME Globex Equity Futures"
  // Note: ES and 0ES only
  static const char *ipMcastString() {
    return "224.0.31.1";
  }
  static u16 udpPort() {
    return 14310;
  }

  static bool bindSocket(ExcSocketHandle hSocket, u32 ipv4, u16 port) {
    struct sockaddr_in myAddr{ .sin_family = AF_INET, .sin_port = port, .sin_addr = { .s_addr = ipv4 } };
    if (bind(hSocket, (const struct sockaddr *)&myAddr, sizeof(myAddr)) < 0) {
      int binErrno = errno;
      //in_addr
      fprintf(stderr, "error on bind(%s:%d) : %s\n", inet_ntoa(*(in_addr *)&myAddr), port, strerror(binErrno));
      return false;
    }
    return true;
  }
  static ExcConnHandle connectTo(EkaDev *device, ExcSocketHandle hSocket, u32 ipv4, u16 port) {
    struct sockaddr_in peer{ .sin_family = AF_INET, .sin_port = port, .sin_addr = { .s_addr = ipv4 } };
    ExcConnHandle hConnection = excConnect(device, hSocket, (const struct sockaddr *)&peer, sizeof(peer));
    if (hConnection < 0)
      fprintf(stderr, "connect(%s:%d) failed\n", inet_ntoa(*(in_addr *)&peer), port);
    return hConnection;
  }
  static std::pair<std::string_view, u16> bindAddress() {
    return {"192.168.0.1", 4712};
  }
  static std::pair<std::string_view, u16> connectAddress() {
    return {"127.0.0.1", 0};
  }
  bool createTCPSocket(std::string_view peerIp, u16 peerPort, std::string_view bindIp, u16 bindPort) {
    bound_ = false;
    connected_ = false;

    ExcSocketHandle hSocket = excSocket(device(), phyPort, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!bindIp.empty()) {
      struct in_addr bindIpAddr;
      if (inet_aton(std::string(bindIp).c_str(), &bindIpAddr)) {
        bound_ = bindSocket(hSocket, bindIpAddr.s_addr, bindPort);
      }
    }
    if (!peerIp.empty() && peerPort != 0) {
      struct in_addr peerIpAddr;
      if (inet_aton(std::string(peerIp).c_str(), &peerIpAddr)) {
        hConnection_ = connectTo(device(), hSocket, peerIpAddr.s_addr, peerPort);
        connected_ = hConnection_ >= 0;
      }
    }

    hSocket_ = hSocket;
    return hSocket_ > 0;
  }
  void CreateDefaultSocket() {
    auto[localIp, localPort] = bindAddress();
    auto[peerIp, peerPort] = connectAddress();
    ASSERT_TRUE(createTCPSocket(std::string(localIp), localPort, std::string(peerIp), peerPort));
  }

  struct ActionDefaults {
    ActionDefaults() : flags{0}, enableBits{0}, actionMask{0}, strategyMask{0} {}
    ActionDefaults(u32 flags, bitsT enableBits, bitsT actionMask, bitsT strategyMask)
        : flags{flags}, enableBits{enableBits}, actionMask{actionMask}, strategyMask{strategyMask} {}

    u32 flags;
    bitsT enableBits;
    bitsT actionMask;
    bitsT strategyMask;
  };
//  void setFlagsBitmasks(u32 flags, bitsT enableBits, bitsT actionMask, bitsT strategyMask) {
//    defaultBits_ = ActionDefaults(flags, enableBits, actionMask, strategyMask);
//  }
  void setReportCookie(uintptr_t cookie) { reportCookie_ = cookie; }
  void attachMessageVerbatim(Strategy &strategy, int scenarioIdx, Message &message, uintptr_t reportCookie) {
    auto options{defaultActionOptions()};
    Action action{EpmActionType::UserAction, connection(), message,
                  options.flags, options.enableBits, options.actionMask, options.strategyMask, reportCookie};
    ASSERT_TRUE(strategy.addAction(scenarioIdx, action));
  }
  bool triggerScenario(int strategyIdx, int scenarioIdx) {
    if (strategyIdx >= strategies_.size())
      return false;

    Strategy &strategy{strategies_[strategyIdx]};
    int strategyPrevActionCount = 0;
    for (int prevScenarioIdx = 0; prevScenarioIdx < scenarioIdx; ++prevScenarioIdx) {
      auto &[ trigger, actionChain ] = strategy.scenario(scenarioIdx);
      strategyPrevActionCount += actionChain.size();
    }
//  const Trigger &trigger{strategy.trigger(scenarioIdx)};
//  EpmTriggerParams epmTrigger{trigger.build()};
//  epmTriggerUsed_ = epmTrigger;
    bool failed = false;
    auto &[ trigger, actionChain ] = strategy.scenario(scenarioIdx);
    for (int actionIdx = 0; actionIdx < actionChain.size() ; ++actionIdx) {
      EpmTrigger actionToTrigger{
        .token = Action::ACTION_TOKEN,
        .strategy = strategyIdx,
        .action = strategyPrevActionCount + actionIdx
      };
      if (isResultOk(epmRaiseTriggers(device(), &actionToTrigger))) {
        epmTriggerInvoked_.push_back(actionToTrigger);
      } else {
        failed = true;
      }
    }
    return !failed;
  }

  bool expectTriggerFire(const EpmTriggerParams &triggerParams, ExcConnHandle hConnection, const Message &message,
                         u32 flags, bitsT enableBits, bitsT actionMask, bitsT strategyMask,
                         uintptr_t reportCookie, bool localTrigger) {
    expectations_.emplace_back(triggerParams, hConnection, message,
                               flags, enableBits, actionMask, strategyMask,
                               reportCookie, localTrigger);
    return true;
  }

  bool matches(EpmFireReport const &report, TriggerFireEvent const &expectation) {
    Strategy &strategy{strategies_.at(report.strategyId)};
    int actionIndex = report.actionId;
    const Scenario *reportedScenario = strategy.getScenarioForAction(actionIndex);
    if (reportedScenario == nullptr) {
      ERROR("strategy ", report.strategyId, ", action ", report.actionId, " out of bounds");
      return false;
    }

    bool failed = false;
    const Trigger &scenarioTrigger{reportedScenario->first};
    EpmTriggerParams triggerParams{scenarioTrigger.build()};
    if (*epmTriggerUsed_ != triggerParams) {
      ERROR("Reported trigger ", *triggerParams, " differs from used to trigger ", *epmTriggerUsed_);
      failed = true;
    }

    const Action &triggeredAction = reportedScenario->second[actionIndex];
    EpmAction epmAction{triggeredAction.build()};
    if (epmAction.token != report.trigger->token) {
      ERROR("token mismatch; expected: ", epmAction.token, ", have ", report.token);
      failed = true;
    }
    if (epmAction.user != report.user) {
      ERROR("'user' cookie does not match; expected: ", epmAction.user, ", have: ", eport.user);
      failed = true;
    }
//    if (epmAction.hConn != report.hConnection) {
//      ERROR("connection mismatch");
//      failed = true;
//    }
//    if (expectation.enableBits != report.enable) {
//      ERROR("action enable bit mismatch; expected: ", expectation.enableBits, ", have: ", report.enable);
//      failed = true;
//    }
    if (expectation.actionMask != report.preLocalEnable) {
      ERROR("pre local mask mismatch; expected: ", expectation.actionMask, ", have: ", report.preLocalEnable);
      failed = true;
    }
    bitsT postActionEnable = expectation.actionMask & expectation.enableBits;
    if (postActionEnable != report.postLocalEnable) {
      ERROR("post local mask mismatch; expected: ", postActionEnable, ", have: ", report.postLocalEnable);
      failed = true;
    }
    if (expectation.strategyMask != report.preStratEnable) {
      ERROR("pre strategy mask mismatch; expected: ", expectation.strategyMask, ", have: ", report.preStratEnable);
      failed = true;
    }
    return !failed;
  }
  void verifyOutcome() {
    ASSERT_TRUE(epmTriggerUsed_.has_value());
    EXPECT_EQ(fireReports_.size(), expectations_.size());
    int lastFoundIdx = -1;
    for (auto &expectation : expectations_) {
      auto foundIt = std::find_if(fireReports_.begin() + lastFoundIdx + 1, fireReports_.end(),
                                  [that=this, &expectation](auto &report){
                                    return that->matches(report, expectation);
                                  });
      if (foundIt != fireReports_.end()) {
        lastFoundIdx = foundIt - fireReports_.begin();
        expectation.reportIndex = lastFoundIdx;
      } else {
        std::stringstream ss{"Failed to match a report for expectation "};
        ss << expectation;
        FAIL() << ss.str();
      }
    }
    for (int reportIdx = 0; reportIdx < fireReports_.size(); ++reportIdx) {
      // TODO(twozniak): verify against epmTriggerInvoked_
      // TODO(twozniak): perhaps 'Sent' is not the only valid outcome
      EXPECT_EQ(fireReports_[reportIdx].action, EpmTriggerAction::Sent);
      auto foundIt = std::find_if(expectations_.begin(),
                                  expectations_.end(),
                                  [reportIdx](auto &expectation){
                                    return expectation.reportIndex == reportIdx;
                                  });
      if (foundIt == expectations_.end()) {
        std::stringstream ss{"unexpected report "};
        ss << fireReports_[reportIdx];
        FAIL() << ss.str();
      }
    }
    fireReports_.clear();
    expectations_.clear();
    epmTriggerInvoked_.clear();
    epmTriggerUsed_.reset();
  }
  int portIdx() const { return phyPort; }
  ExcConnHandle connection() { return hConnection_; }

  struct ActionOptions {
    u32   flags;
    bitsT enableBits;
    bitsT actionMask;
    bitsT strategyMask;
  };
  ActionOptions defaultActionOptions() {
    return ActionOptions{
      .flags = 0xfee1,
      .enableBits = 0xfedcba98'01234567,
      .actionMask = 0x0000'0000'0000'0001,
      .strategyMask = 0x0000'0000'0000'0001
    };
  }

 private:
  EkaCoreId phyPort{0}; // Physical port index
  std::string ekalineIPAddr_;
  std::string ekalineIPMask_;
  std::string ekalineIPGateway_;

  u32 localTCP_   = 0;
  u16 localPort_  = 0;
  u32 remoteTCP_  = 0;
  u16 remotePort_ = 0;

  bool bound_     = false;
  bool connected_ = false;

  ExcSocketHandle hSocket_  = -1;
  ExcConnHandle hConnection_ = -1;

  Strategy::ReportCbFn callback_;
  std::vector<Strategy> strategies_;
  ActionDefaults defaultBits_;
  uintptr_t reportCookie_;
  bool deployed_  = false;

  std::optional<EpmTriggerParams> epmTriggerUsed_;
  std::vector<EpmTrigger>         epmTriggerInvoked_;
  std::vector<TriggerFireEvent> expectations_;
  std::vector<EpmFireReport> fireReports_;
  std::unique_ptr<EkaDev, decltype(&releaseDevice)> ekaDevice_;
};

TEST_F(EkalinePMFixture, SocketCreateBindConnect) {
  auto [ localIp, localPort ] = bindAddress();
  auto [ peerIp, peerPort ] = connectAddress();
  EXPECT_TRUE(createTCPSocket(localIp, localPort, peerIp, peerPort));
}
// defaultActionOptions() options.flags, options.enableBits, options.actionMask, options.strategyMask
TEST_F(EkalinePMFixture, EPMCreate) {
  CreateDefaultSocket();
  Strategy &strategy{addStrategy(0x1234'5678'9abc'def1ULL)};
  Message message1{"Some verbatim message #1"};
  auto options{defaultActionOptions()};
  uintptr_t reportCookie{0xfee10bad'00000001};
  Action action{EpmActionType::UserAction, connection(), message1,
                options.flags, options.enableBits, options.actionMask, options.strategyMask, reportCookie};
  int scenarioIdx = strategy.createScenario(Trigger(portIdx(), ipMcastString(), udpPort()), {action});
  EXPECT_TRUE(deployStrategies());
}

TEST_F(EkalinePMFixture, SmokeTest) {
  CreateDefaultSocket();
  Strategy &strategy{addStrategy(0x1234'5678'9abc'def1ULL)};
  Message message{"Some verbatim message #1"};
  auto options{defaultActionOptions()};
  uintptr_t reportCookie{0xfee10bad'00000002};
  Action action{EpmActionType::UserAction, connection(), message,
                options.flags, options.enableBits, options.actionMask, options.strategyMask, reportCookie};
  int scenarioIdx = strategy.createScenario(Trigger(portIdx(), ipMcastString(), udpPort()), {action});
  ASSERT_TRUE(deployStrategies());

  auto &trigger{strategy.trigger(0)};
  EpmTriggerParams epmTriggerParams{trigger.build()};
  expectTriggerFire(epmTriggerParams, connection(), message,
                    options.flags, options.enableBits,
                    options.actionMask, options.strategyMask,
                    reportCookie, true);
  ASSERT_TRUE(triggerScenario(0, scenarioIdx));
  verifyOutcome();
}

TEST_F(EkalinePMFixture, MultipleMessages) {
  CreateDefaultSocket();
  Strategy &strategy{addStrategy(0x1234'5678'9abc'def1ULL)};
  auto options{defaultActionOptions()};
  int scenarioIdx = strategy.createScenario(Trigger(portIdx(), ipMcastString(), udpPort()));
  for (int idx = 0; idx < MAX_ACTIONS_PER_TRIGGER; ++idx) {
    Message message{"Some verbatim message #" + std::to_string(idx + 1)};
    uintptr_t reportCookie{0xfee1cafe'ca430000 + idx};
    attachMessageVerbatim(strategy, scenarioIdx, message, reportCookie);
    Action action{EpmActionType::UserAction, connection(), message,
                  options.flags, options.enableBits, options.actionMask, options.strategyMask, reportCookie};
    ASSERT_TRUE(strategy.addAction(scenarioIdx, action));
  }
  ASSERT_TRUE(deployStrategies());
  ASSERT_TRUE(triggerScenario(0, scenarioIdx));
  verifyOutcome();
}

}  // anonymous namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


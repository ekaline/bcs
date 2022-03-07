#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <optional>
#include <pthread.h>
#include <tuple>
#include <vector>
#include <stdio.h>
#include <sstream>

#include <compat/Eka.h>
#include <compat/Epm.h>
#include <compat/Exc.h>

#include "Action.h"
#include "common.h"
#include "Strategy.h"
#include "StrategyManager.h"
#include "Trigger.h"

namespace {

using namespace gts;
using namespace gts::ekaline;

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

class EkalinePMFixture : public ::testing::Test {
 public:
  using bitsT = epm_enablebits_t;
  using Scenario = Strategy::Scenario;

  static constexpr int MAX_ACTIONS_PER_TRIGGER = 16;

  EkalinePMFixture() : ekaDevice_{nullptr, releaseDevice} {}

  void SetUp() override {
    INFO("SetUp called");
    if (!initDevice()) {
      std::cerr << "Failed to initialize ekaline device\n";
      std::abort();
    }
    auto [ ipAddr, ipPort ] = bindAddress();
    ekalineIPAddr_ = ipAddr;
    ekalineIPMask_ = "255.255.255.0";
    ekalineIPGateway_ = ekalineIPAddr_;

    if (!initPort()) {
      std::cerr << "Failed to initialize port " << phyPort << "on ekaline device\n";
      std::abort();
    }
  }
  EkaDev *device() { assert(ekaDevice_ != nullptr); return ekaDevice_.get(); }
  bool initDevice() {
    if (ekaDevice_ != nullptr) {
      WARN("Attempted to init already init'ed device");
      return true;
    }
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
    if (!isResultOk(result)) {
      ERROR("ekaDevInit() failed with ", result);
    } else {
      ekaDevice_.reset(device);
    }
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
  bool deployStrategies() {
//    assert(bound_);
    assert(connected_);

    StrategyManager man;
    return man.deployStrategies(device(), phyPort, strategies_);
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
    struct sockaddr_in myAddr{ .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = { .s_addr = ipv4 } };
    if (bind(hSocket, (const struct sockaddr *)&myAddr, sizeof(myAddr)) < 0) {
      int binErrno = errno;
      //in_addr
      fprintf(stderr, "error on bind(%s:%d) : %s\n", inet_ntoa(*(in_addr *)&myAddr), port, strerror(binErrno));
      return false;
    }
    return true;
  }
  static ExcConnHandle connectTo(EkaDev *device, ExcSocketHandle hSocket, u32 ipv4, u16 port) {
    struct sockaddr_in peer{ .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = { .s_addr = ipv4 } };
    ExcConnHandle hConnection = excConnect(device, hSocket, (const struct sockaddr *)&peer, sizeof(peer));
    if (hConnection < 0)
      fprintf(stderr, "connect(%s:%d) failed\n", inet_ntoa(*(in_addr *)&peer), port);
    return hConnection;
  }
  static std::pair<std::string_view, u16> bindAddress() {
    return {"10.120.10.51", 49420};
  }
  static std::pair<std::string_view, u16> connectAddress() {
    return {"10.120.15.83", 7060};
  }
//  bool configurePort(ExcSocketHandle hSocket, ) {
//    EkaCoreInitCtx ekaCoreInitCtx = {
//        .coreId = phyPort,
//        .attrs = {
//            .host_ip      = inet_addr(clientIp.c_str()),
//            .netmask      = inet_addr("255.255.255.0"),
//            .gateway      = inet_addr(clientIp.c_str()),
//            .nexthop_mac  = {}, // resolved by our internal ARP
//            .src_mac_addr = {}, // taken from system config
//            .dont_garp    = 0
//        }
//    };
//    ekaDevConfigurePort (dev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);
//  }
  bool createTCPSocket(std::string_view bindIp, u16 bindPort, std::string_view peerIp, u16 peerPort) {
    bool failed = false;
    bound_ = false;
    connected_ = false;

    ExcSocketHandle hSocket = excSocket(device(), phyPort, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!bindIp.empty()) {
      WARN("Local socket binding not supported, skipping");
//      struct in_addr bindIpAddr;
//      if (inet_aton(std::string(bindIp).c_str(), &bindIpAddr)) {
//        bound_ = bindSocket(hSocket, bindIpAddr.s_addr, bindPort);
//      }
//      failed |= !bound;
    }
    if (!peerIp.empty() && peerPort != 0) {
      struct in_addr peerIpAddr;
      if (inet_aton(std::string(peerIp).c_str(), &peerIpAddr)) {
        hConnection_ = connectTo(device(), hSocket, peerIpAddr.s_addr, peerPort);
        connected_ = hConnection_ >= 0;
      }
      failed |= !connected_;
    }

    hSocket_ = hSocket;
    return !failed && (hSocket_ > 0);
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
    if (strategyIdx >= (int)strategies_.size())
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
    for (int actionIdx = 0; actionIdx < (int)actionChain.size() ; ++actionIdx) {
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
      ERROR("Reported trigger ", triggerParams, " differs from used to trigger ", *epmTriggerUsed_);
      failed = true;
    }

    const Action &triggeredAction = reportedScenario->second[actionIndex];
    EpmAction epmAction{triggeredAction.build()};
    if (epmAction.token != report.trigger->token) {
      ERROR("token mismatch; expected: ", epmAction.token, ", have ", report.trigger->token);
      failed = true;
    }
    if (epmAction.user != report.user) {
      ERROR("'user' cookie does not match; expected: ", epmAction.user, ", have: ", report.user);
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
    for (int reportIdx = 0; reportIdx < (int)fireReports_.size(); ++reportIdx) {
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
  ASSERT_GE(scenarioIdx, 0);
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
  std::cout << "Build date " << BUILD_DATE << ", git revision " << GIT_REVISION << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


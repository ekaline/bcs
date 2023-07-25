#ifndef __EFC_MASTER_TEST_CONFIG_H__
#define __EFC_MASTER_TEST_CONFIG_H__

/* --------------------------------------------- */
static const int MaxTcpTestSessions = 16;
static const int MaxUdpTestSessions = 64;
static uint16_t numTcpSess = 1;

enum class TestStrategy : int {
  Invalid = 0,
  P4,
  CmeFC,
  Qed
};

static const char *printStrat(TestStrategy s) {
  switch (s) {
  case TestStrategy::Invalid:
    return "Invalid";
  case TestStrategy::P4:
    return "P4";
  case TestStrategy::CmeFC:
    return "CmeFC";
  case TestStrategy::Qed:
    return "Qed";
  default:
    on_error("Unexpected testStrategy %d", (int)s);
  }
}

static TestStrategy string2strat(const char *s) {
  if (!strcmp(s, "P4"))
    return TestStrategy::P4;

  if (!strcmp(s, "CmeFC"))
    return TestStrategy::CmeFC;

  if (!strcmp(s, "Qed"))
    return TestStrategy::Qed;

  return TestStrategy::Invalid;
}

struct TestTcpSess {
  std::string srcIp;
  std::string dstIp;
  uint16_t dstPort;
};

struct TestUdpMc {
  std::string mcIp;
  uint16_t mcPort;
};

static const TestTcpSess testDefaultTcpSess[] = {
    {"100.0.0.110", "10.0.0.10", 22222},
    {"200.0.0.110", "10.0.0.11", 33333}};

static const TestUdpMc testDefaultUdpMc[] = {
    {"224.0.74.0", 30300}, {"224.0.74.1", 30301}};

/* --------------------------------------------- */

struct TestCaseConfig {
  TestStrategy strat;
  EkaCoreId mdCoreId;
  uint8_t fireTcpCoreBitmap;
};

struct TestScenarioConfig {
  const char *name;
  TestCaseConfig testConf[2];
};

const TestScenarioConfig scenarios[] = {
    {"QedOnly",
     {{TestStrategy::Qed, 0, 0x1},
      {TestStrategy::Invalid, 0, 0x0}}},

    {"P4Only",
     {{TestStrategy::P4, 0, 0x1},
      {TestStrategy::Invalid, 0, 0x0}}},

    {"Qed_0__P4_1",
     {{TestStrategy::Qed, 0, 0x1},
      {TestStrategy::P4, 1, 0x2}}},

    {"P4_0__Qed_1",
     {{TestStrategy::Qed, 1, 0x2},
      {TestStrategy::P4, 0, 0x1}}},

    {"P4_0__Qed_3",
     {{TestStrategy::P4, 0, 0x1},
      {TestStrategy::Qed, 3, 0x8}}},

};

/* -------------------------------------------- */

inline void printSingleScenario(int i) {
  if (i < 0 || i > std::size(scenarios))
    on_error("invalid scenario index %d", i);
  auto sc = scenarios[i];
  printf("%d: %s: \n", i++, sc.name);
  for (const auto &tc : sc.testConf) {
    if (tc.strat != TestStrategy::Invalid) {
      printf("\t%6s: ", printStrat(tc.strat));
      printf("Market Data lane: %d, ", tc.mdCoreId);
      printf("Fire Lanes bitmap: 0x%x ",
             tc.fireTcpCoreBitmap);
      printf("\n");
    }
  }
}

inline void printAllScenarios() {
  for (auto i = 0; i < std::size(scenarios); i++)
    printSingleScenario(i);
}

#endif

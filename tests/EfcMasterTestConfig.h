#ifndef __EFC_MASTER_TEST_CONFIG_H__
#define __EFC_MASTER_TEST_CONFIG_H__

#include "Efc.h"

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
  int coreId;
  const char *srcIp;
  const char *dstIp;
  uint16_t dstPort;
};

struct TestTcpParams {
  TestTcpSess *tcpSess;
  size_t nTcpSess;
};

static const char *udpSrcIp[] = {
    "10.0.0.10",
    "10.0.0.11",
    "10.0.0.12",
    "10.0.0.13",
};

static const TestTcpSess testDefaultTcpSess[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111},
    {2, "120.0.0.0", "10.0.0.12", 22222},
    {3, "130.0.0.0", "10.0.0.13", 22333}};

TestTcpSess tcp0_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000}};
TestTcpSess tcp1_s[] = {
    {1, "110.0.0.0", "10.0.0.11", 22111}};
TestTcpSess tcp01_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111}};

static const TestTcpParams tcp0 = {tcp0_s,
                                   std::size(tcp0_s)};

static const TestTcpParams tcp1 = {tcp1_s,
                                   std::size(tcp1_s)};

static const TestTcpParams tcp01 = {tcp01_s,
                                    std::size(tcp01_s)};

/* --------------------------------------------- */
static const EfcUdpMcGroupParams mc0[] = {
    {0, "224.0.0.0", 30300}};
static const EfcUdpMcGroupParams mc1[] = {
    {1, "224.0.0.1", 30301}};
static const EfcUdpMcGroupParams mc01[] = {
    {0, "224.0.0.0", 30300}, {1, "224.0.0.1", 30301}};

static const EfcUdpMcParams core0_1mc = {mc0,
                                         std::size(mc0)};
static const EfcUdpMcParams core1_1mc = {mc1,
                                         std::size(mc1)};
static const EfcUdpMcParams two_cores_1mc = {
    mc01, std::size(mc01)};

/* --------------------------------------------- */

struct TestCaseConfig {
  TestStrategy strat;
  EfcUdpMcParams mcParams;
  TestTcpParams tcpParams;
};

struct TestScenarioConfig {
  const char *name;
  TestCaseConfig testConf[2];
};

const TestScenarioConfig scenarios[] = {
    {"Qed_0",
     {{TestStrategy::Qed, core0_1mc, tcp0},
      {TestStrategy::Invalid, {}, {}}}},

    {"P4_0",
     {{TestStrategy::P4, core0_1mc, tcp0},
      {TestStrategy::Invalid, {}, {}}}},

    {"P4_0_1",
     {{TestStrategy::P4, two_cores_1mc, tcp0},
      {TestStrategy::Invalid, {}, {}}}},

    {"Qed_1",
     {{TestStrategy::Qed, core1_1mc, tcp1},
      {TestStrategy::Invalid, {}, {}}}},

    {"P4_1",
     {{TestStrategy::P4, core1_1mc, tcp1},
      {TestStrategy::Invalid, {}, {}}}},

    {"Qed_0__P4_1",
     {{TestStrategy::Qed, core0_1mc, tcp0},
      {TestStrategy::P4, core1_1mc, tcp1}}},

    {"P4_0__Qed_1",
     {{TestStrategy::Qed, core1_1mc, tcp1},
      {TestStrategy::P4, core0_1mc, tcp0}}},

};

/* -------------------------------------------- */

void printMcConf(const EfcUdpMcParams *conf) {
  printf("\t%ju MC groups:\n", conf->nMcGroups);
  for (auto i = 0; i < conf->nMcGroups; i++) {
    auto gr = &conf->groups[i];
    printf("\t\t%d, %s:%u, ", gr->coreId, gr->mcIp,
           gr->mcUdpPort);
  }
  printf("\n");
}

void printTcpConf(const TestTcpParams *conf) {
  printf("\t%ju TCP sessions:\n", conf->nTcpSess);
  for (auto i = 0; i < conf->nTcpSess; i++) {
    auto gr = &conf->tcpSess[i];
    printf("\t\t%d, %s --> %s:%u, ", gr->coreId, gr->srcIp,
           gr->dstIp, gr->dstPort);
  }
  printf("\n");
}

inline void printTestCaseConf(const TestCaseConfig *tc) {
  if (tc->strat != TestStrategy::Invalid) {
    printf("\tStrategy: %6s\n", printStrat(tc->strat));
    printMcConf(&tc->mcParams);
    printTcpConf(&tc->tcpParams);
  }
}

inline void printSingleScenario(int i) {
  if (i < 0 || i > std::size(scenarios))
    on_error("invalid scenario index %d", i);
  auto sc = scenarios[i];
  printf("-----------------------\n"
         "%d: %s: \n",
         i++, sc.name);
  for (const auto &tc : sc.testConf) {
    printTestCaseConf(&tc);
  }
}

inline void printAllScenarios() {
  for (auto i = 0; i < std::size(scenarios); i++)
    printSingleScenario(i);
}

#endif

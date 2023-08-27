#include <gtest/gtest.h>
#include <iostream>

#if 1

#include "TestCmeFc.h"
#include "TestP4.h"

#endif

/* --------------------------------------------- */
#if 0
static const TestTcpSess testDefaultTcpSess[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111},
    {2, "120.0.0.0", "10.0.0.12", 22222},
    {3, "130.0.0.0", "10.0.0.13", 22333}};

static const TestTcpSess tcp1_s[] = {
    {1, "110.0.0.0", "10.0.0.11", 22111}};
static const TestTcpSess tcp01_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111}};


static const TestTcpParams tcp1 = {tcp1_s,
                                   std::size(tcp1_s)};

static const TestTcpParams tcp01 = {tcp01_s,
                                    std::size(tcp01_s)};
static const EfcUdpMcGroupParams mc1[] = {
    {1, "224.0.0.1", 30301}};
static const EfcUdpMcGroupParams mc01[] = {
    {0, "224.0.0.0", 30300}, {1, "224.0.0.1", 30301}};

static const EfcUdpMcParams core1_1mc = {mc1,
                                         std::size(mc1)};
static const EfcUdpMcParams two_cores_1mc = {
    mc01, std::size(mc01)};
#endif
/* --------------------------------------------- */

/* --------------------------------------------- */
#if 0

static const EfcUdpMcGroupParams mc0[] = {};
static const EfcUdpMcParams core0_1mc = {};

static const TestTcpSess tcp0_s[] = {};
static const TestTcpParams tcp0 = {};
#endif

#if 1
static const TestTcpSess tcp0_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000}};
static const TestTcpParams tcp0 = {tcp0_s,
                                   std::size(tcp0_s)};
static const EfcUdpMcGroupParams mc0[] = {0, "224.0.0.0",
                                          30300};
static const EfcUdpMcParams core0_1mc = {mc0,
                                         std::size(mc0)};
#endif
/* --------------------------------------------- */
#if 0
TEST_F(TestCmeFc, CmeFC_0) {
  runTest({TestStrategy::CmeFc, core0_1mc, tcp0, nullptr,
           nullptr});
}
#endif
/* --------------------------------------------- */
TEST(Dummy, Zero) { EXPECT_EQ(0, 0); }
#if 1

TEST_F(TestP4, FireOnAsk) {
  TEST_LOG("Im here");

  TestP4CboeSec secs[] = {{.id = "01gtst",
                           .bidMinPrice = 1000,
                           .askMaxPrice = 2000,
                           .size = 1}};
  TestP4SecConf secConf = {.sec = secs,
                           .nSec = std::size(secs)};

  TestP4Md mdConf = {
      .secId = secs[0].id,
      .side = SideT::ASK,
      .price = (FixedPrice)(secs[0].askMaxPrice - 1),
      .size = 1};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf,
                             .mdInjectParams = &mdConf};

  runTest(&tc);
}
#endif

/* --------------------------------------------- */
#if 1

TEST_F(TestP4, FireOnBid) {
  TestP4CboeSec secs[] = {{.id = "02gtst",
                           .bidMinPrice = 1000,
                           .askMaxPrice = 2000,
                           .size = 1}};
  TestP4SecConf secConf = {.sec = secs,
                           .nSec = std::size(secs)};

  TestP4Md mdConf = {
      .secId = secs[0].id,
      .side = SideT::BID,
      .price = (FixedPrice)(secs[0].bidMinPrice + 1),
      .size = 1};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf,
                             .mdInjectParams = &mdConf};

  runTest(&tc);
}
#endif
/* --------------------------------------------- */

int main(int argc, char **argv) {
  std::cout << "Main of " << argv[0] << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

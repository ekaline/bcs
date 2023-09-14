#include <gtest/gtest.h>
#include <iostream>

#if 1

#include "TestCmeFc.h"
#include "TestP4.h"
#include "TestP4Rand.h"

#endif

/* --------------------------------------------- */
#if 1
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

static const TestTcpSess tcp3_s[] = {
    {3, "130.0.0.0", "10.0.0.13", 22333}};
static const TestTcpParams tcp3 = {tcp3_s,
                                   std::size(tcp3_s)};

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
#if 1
TEST_F(TestCmeFc, CmeFC_core3) {

  EfcCmeFcParams cmeParams = {.maxMsgSize = 97,
                              .minNoMDEntries = 0};

  const uint8_t mdPkt[] = {
      0x22, 0xa5, 0x0d, 0x02, 0xa5, 0x6f, 0x01, 0x38, 0xca,
      0x42, 0xdc, 0x16, 0x60, 0x00, 0x0b, 0x00, 0x30, 0x00,
      0x01, 0x00, 0x09, 0x00, 0x41, 0x23, 0xff, 0x37, 0xca,
      0x42, 0xdc, 0x16, 0x01, 0x00, 0x00, 0x20, 0x00, 0x01,
      0x00, 0xfc, 0x2f, 0x9c, 0x9d, 0xb2, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x5b, 0x33, 0x00, 0x00, 0x83, 0x88,
      0x26, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0xd9,
      0x7a, 0x6d, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x02, 0x0e, 0x19, 0x84, 0x8e, 0x36,
      0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0xb0, 0x7f, 0x8e, 0x36, 0x06, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  TestCmeFcMd mdConf = {.preloadedPkt = mdPkt,
                        .pktLen = sizeof(mdPkt),
                        .appSeq = 0xa1b2c3d4,
                        .expectedFire = true};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp3,
                             .algoConfigParams = &cmeParams,
                             .mdInjectParams = &mdConf};
  nExpectedFires = 1;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4, FireOnAsk) {
  TestP4CboeSec secs[] = {{.strId = "01gtst",
                           .bidMinPrice = 1000,
                           .askMaxPrice = 2000,
                           .size = 1}};
  TestP4SecConf secConf = {
      .type = TestP4SecConfType::Predefined,
      .sec = secs,
      .nSec = std::size(secs)};

  TestP4Md mdConf = {
      .secId = secs[0].strId,
      .side = SideT::ASK,
      .price = (FixedPrice)(secs[0].askMaxPrice - 1),
      .size = 1,
      .expectedFire = true};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf,
                             .mdInjectParams = &mdConf};

  nExpectedFires = 1;

  runTest(&tc);
}
#endif

/* --------------------------------------------- */
#if 1
TEST_F(TestP4, FireOnBid) {
  TestP4CboeSec secs[] = {{.strId = "02gtst",
                           .bidMinPrice = 1000,
                           .askMaxPrice = 2000,
                           .size = 1}};
  TestP4SecConf secConf = {
      .type = TestP4SecConfType::Predefined,
      .sec = secs,
      .nSec = std::size(secs)};

  TestP4Md mdConf = {
      .secId = secs[0].strId,
      .side = SideT::BID,
      .price = (FixedPrice)(secs[0].bidMinPrice + 1),
      .size = 1,
      .expectedFire = true};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf,
                             .mdInjectParams = &mdConf};

  nExpectedFires = 1;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4Rand, RandFires) {
  TestP4CboeSec secs[] = {};
  TestP4SecConf secConf = {.type =
                               TestP4SecConfType::Random,
                           .percentageValidSecs = 0.5,
                           .nSec = 8};

  TestP4Md mdConf = {};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf};

  nExpectedFires = 1;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */

int main(int argc, char **argv) {
  std::cout << "Main of " << argv[0] << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

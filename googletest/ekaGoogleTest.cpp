#include <gtest/gtest.h>
#include <iostream>

#include "TestCmeFc.h"
#include "TestIgmpFixture.h"
#include "TestP4.h"
#include "TestP4FixedSecs.h"
#include "TestP4PredefinedCtx.h"
#include "TestP4Rand.h"
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
  nExpectedFires_ = 1;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4, FireOnAsk) {
  TestP4CboeSec secs[] = {{.strId = "01gtst",
                           .bidMinPrice = 10,
                           .askMaxPrice = 20,
                           .size = 1}};
  TestP4SecConf secConf = {
      .type = TestP4SecConfType::Predefined,
      .sec = secs,
      .nSec = std::size(secs)};

  TestP4Md mdConf = {.secId = secs[0].strId,
                     .side = SideT::ASK,
                     .price = static_cast<TestP4MdPrice>(
                         secs[0].askMaxPrice * 100 - 1),
                     .size = 1,
                     .expectedFire = true};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf,
                             .mdInjectParams = &mdConf};

  nExpectedFires_ = 1;

  runTest(&tc);
}
#endif

/* --------------------------------------------- */
#if 1
TEST_F(TestP4, FireOnBid) {
  TestP4CboeSec secs[] = {{.strId = "02gtst",
                           .bidMinPrice = 10,
                           .askMaxPrice = 20,
                           .size = 1}};
  TestP4SecConf secConf = {
      .type = TestP4SecConfType::Predefined,
      .sec = secs,
      .nSec = std::size(secs)};

  TestP4Md mdConf = {.secId = secs[0].strId,
                     .side = SideT::BID,
                     .price = static_cast<TestP4MdPrice>(
                         secs[0].bidMinPrice * 100 + 1),
                     .size = 1,
                     .expectedFire = true};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf,
                             .mdInjectParams = &mdConf};

  nExpectedFires_ = 1;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4FixedSecs, Hash1Line16secs) {
  // subscribing on 16 securities fitting same Hash line

  std::vector<std::string> testSecIds = {
      "01bfkq", "00avkq", "02ovkq", "02yfkq",
      "01bvkq", "00zvkq", "01kfkq", "00nvkq",
      "02wvkq", "00jfkq", "02kvkq", "03jfkq",
      "02cfkq", "02jvkq", "00qfkq", "03evkq"};

  fixedSecs_.insert(fixedSecs_.begin(), testSecIds.begin(),
                    testSecIds.begin() + testSecIds.size());

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0};

  nExpectedFires_ = 16;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4FixedSecs, Hash1Line25secs) {
  // subscribing on 25 securities at same Hash line

  std::vector<std::string> testSecIds = {
      "000aaa", "001aaa", "002aaa", "003aaa", "010aaa",
      "011aaa", "012aaa", "013aaa", "020aaa", "021aaa",
      "022aaa", "023aaa", "030aaa", "031aaa", "032aaa",
      "033aaa", "03aaaa", "03baaa", "03caaa", "03daaa",
      "03eaaa", "03faaa", "03gaaa", "03haaa", "03Xaaa"};

  fixedSecs_.insert(fixedSecs_.begin(), testSecIds.begin(),
                    testSecIds.begin() + testSecIds.size());

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0};

  nExpectedFires_ = 24;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4Rand, RandFires) {
  TestP4CboeSec secs[] = {};
  TestP4SecConf secConf = {.type =
                               TestP4SecConfType::Random,
                           .percentageValidSecs = 0.9,
                           .nSec = 100000};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf};

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4PredefinedCtx, RandFires) {
  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0};

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4PredefinedCtx, Hash1Line25secs) {
  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0};

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestP4, FailureDebug) {
  TestP4CboeSec secs[] = {{.strId = "03DB6K",
                           .bidMinPrice = 19203,
                           .askMaxPrice = 11520,
                           .size = 1}};
  TestP4SecConf secConf = {
      .type = TestP4SecConfType::Predefined,
      .sec = secs,
      .nSec = std::size(secs)};

  TestP4Md mdConf = {
      .secId = secs[0].strId,
      .side = SideT::BID,
      .price = static_cast<TestP4MdPrice>(1920301),
      .size = 1,
      .expectedFire = true};

  const TestCaseConfig tc = {.mcParams = &core0_1mc,
                             .tcpParams = &tcp0,
                             .algoConfigParams = &secConf,
                             .mdInjectParams = &mdConf};

  nExpectedFires_ = 1;

  runTest(&tc);
}
#endif
/* --------------------------------------------- */
TEST_F(TestIgmpFixture, Box12x1_Emld3x4) {

  // BOX: coreId = 0, 12 groups 1 per group
  for (auto i = 0; i < 12; i++) {
    TestEfhRunGrConfig rg = {.exch = EkaSource::kBOX_HSVF,
                             .coreId = 0,
                             .nMcgroups = 1,
                             .firstGrId = 0};
    runGroups_.push_back(rg);
  }
#if 1
  // EMLD: coreId = 2, 3 groups 4 per group
  for (auto i = 0; i < 3; i++) {
    TestEfhRunGrConfig rg = {.exch = EkaSource::kEMLD_TOM,
                             .coreId = 2,
                             .nMcgroups = 4,
                             .firstGrId = 0};
    runGroups_.push_back(rg);
  }
#endif
  runIgmpTest();
}
/* --------------------------------------------- */
TEST_F(TestIgmpFixture, Emld3x4_Box12x1) {

  // EMLD: coreId = 2, 3 groups 4 per group
  for (auto i = 0; i < 3; i++) {
    TestEfhRunGrConfig rg = {.exch = EkaSource::kEMLD_TOM,
                             .coreId = 2,
                             .nMcgroups = 4,
                             .firstGrId = 0};
    runGroups_.push_back(rg);
  }

  // BOX: coreId = 0, 12 groups 1 per group
  for (auto i = 0; i < 12; i++) {
    TestEfhRunGrConfig rg = {.exch = EkaSource::kBOX_HSVF,
                             .coreId = 0,
                             .nMcgroups = 1,
                             .firstGrId = 0};
    runGroups_.push_back(rg);
  }

  runIgmpTest();
}
/* --------------------------------------------- */
int main(int argc, char **argv) {
  std::cout << "Main of " << argv[0] << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
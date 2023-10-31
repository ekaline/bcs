#include <gtest/gtest.h>
#include <iostream>

#include "TestEfcFixture.h"
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

static const EkaBcMcGroupParams mc1[] = {
    {1, "224.0.0.1", 30301}};
static const EkaBcMcGroupParams mc01[] = {
    {0, "224.0.0.0", 30300}, {1, "224.0.0.1", 30301}};

static const EkaBcUdpMcParams core1_1mc = {mc1,
                                           std::size(mc1)};
static const EkaBcUdpMcParams two_cores_1mc = {
    mc01, std::size(mc01)};
#endif
/* --------------------------------------------- */

/* --------------------------------------------- */
#if 1
static const TestTcpSess tcp0_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000}};
static const TestTcpParams tcp0 = {tcp0_s,
                                   std::size(tcp0_s)};
static const EkaBcMcGroupParams mc0[] = {0, "224.0.0.0",
                                         30300};
static const EkaBcUdpMcParams core0_1mc = {mc0,
                                           std::size(mc0)};
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestEfcFixture, Eur_basic) {

  mcParams_ = &core0_1mc;
  tcpParams_ = &tcp3;

  prodList_[0] = static_cast<EkaBcEurSecId>(123);
  prodList_[1] = static_cast<EkaBcEurSecId>(456);
  prodList_[2] = static_cast<EkaBcEurSecId>(789);

  nProds_ = 3;

  runTest();
}
#endif
/* --------------------------------------------- */

/* --------------------------------------------- */
int main(int argc, char **argv) {
  std::cout << "Main of " << argv[0] << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

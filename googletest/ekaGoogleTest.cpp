#include <arpa/inet.h>
#include <chrono>
#include <gtest/gtest.h>
#include <netinet/ether.h>
#include <optional>
#include <pthread.h>
#include <span>
#include <sstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <compat/Efc.h>
#include <compat/Eka.h>
#include <compat/Epm.h>
#include <compat/Exc.h>

#include "TestEfcFixture.h"

TEST_F(TestEfcFixture, Dummiest) { EXPECT_EQ(0, 0); }

TEST_F(TestCmeFc, CmeFC_0) {
  auto t =
      new TestCase({TestStrategy::CmeFc, core0_1mc, tcp0});
  ASSERT_NE(t, nullptr);
}

int main(int argc, char **argv) {
  std::cout << "Main of " << argv[0] << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

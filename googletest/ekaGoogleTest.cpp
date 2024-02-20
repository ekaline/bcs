#include <gtest/gtest.h>
#include <iostream>

#include "EkaBcs.h"
#include "TestEfcFixture.h"
#include "TestMoex.h"

using namespace EkaBcs;

/* --------------------------------------------- */

/* --------------------------------------------- */
int main(int argc, char **argv) {
  std::cout << "Main of " << argv[0] << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

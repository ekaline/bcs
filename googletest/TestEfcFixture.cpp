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

#include "eka_macros.h"

#include "TestEfcFixture.h"

void TestEfcFixture::SetUp() {
  TEST_LOG("SetUp()");

  const EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&dev_, &ekaDevInitCtx);

  return;
}

void TestEfcFixture::TearDown() {
  TEST_LOG("TearDown()");
  return;
}

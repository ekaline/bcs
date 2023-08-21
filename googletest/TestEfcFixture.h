#ifndef __TEST_EFC_FIXTURE_H__
#define __TEST_EFC_FIXTURE_H__

#include "eka_macros.h"

class TestEfcFixture : public ::testing::Test {
public:
  void SetUp() override;
  void TearDown() override;

protected:
  EkaDev *dev_;
};

#endif

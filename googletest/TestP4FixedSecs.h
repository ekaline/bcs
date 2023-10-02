#ifndef __TEST_P4_FIXED_SECS_H__
#define __TEST_P4_FIXED_SECS_H__

#include "TestP4Rand.h"

class TestP4FixedSecs : public TestP4Rand {

protected:
  void createSecList(const void *secConf) override;

protected:
  std::vector<std::string> fixedSecs_ = {};
};

#endif

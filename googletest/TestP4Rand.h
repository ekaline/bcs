#ifndef __TEST_P4_RAND_H__
#define __TEST_P4_RAND_H__

#include "TestP4.h"

class TestP4Rand : public TestP4 {

protected:
  void createSecList(const void *secConf) override;
  virtual void generateMdDataPkts(const void *t) override;

protected:
};

#endif

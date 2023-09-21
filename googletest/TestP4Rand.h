#ifndef __TEST_P4_RAND_H__
#define __TEST_P4_RAND_H__

#include "TestP4.h"

class TestP4Rand : public TestP4 {

protected:
  TestP4Rand();
  virtual ~TestP4Rand();

  void createSecList(const void *secConf) override;
  virtual void generateMdDataPkts(const void *t) override;

  void archiveSecCtxsMd() override;

protected:
#if 0
  std::ofstream outputFile_;
  cereal::JSONOutputArchive archive_;
#endif
};

#endif

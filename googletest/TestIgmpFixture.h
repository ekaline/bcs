#ifndef __TEST_IGMP_FIXTURE_H__
#define __TEST_IGMP_FIXTURE_H__

#include "TestEfcFixture.h"

struct TestEfhRunGrConfig {
  EkaSource exch;
  EkaCoreId coreId;
  size_t nMcgroups;
  int firstGrId;
};

struct TestProp {
  char key[512];
  char val[512];
};

class TestIgmpFixture : public TestEfcFixture {
protected:
  virtual size_t prepareProps(int runGrId, TestProp *props,
                              EkaProp *prop,
                              const TestEfhRunGrConfig *rg);
  virtual void prepareGroups(EkaGroup *pGroups,
                             const TestEfhRunGrConfig *rg);
  virtual void runIgmpTest();

protected:
  std::vector<TestEfhRunGrConfig> runGroups_;
  std::thread efhRunThread_[64];
  EfhRunCtx efhRunCtx_[64] = {};
  EkaGroup *pGroups_[64] = {};
};
#endif

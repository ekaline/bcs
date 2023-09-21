#ifndef __TEST_P4_PREDEFINED_CTX_H__
#define __TEST_P4_PREDEFINED_CTX_H__

#include "TestP4Rand.h"

class TestP4PredefinedCtx : public TestP4Rand {

protected:
  TestP4PredefinedCtx();
  virtual ~TestP4PredefinedCtx();

  virtual void generateMdDataPkts(const void *t) override{};

  void archiveSecCtxsMd() override{};
  void loadSecCtxsMd() override;
  void createSecList(const void *unused) override;

protected:
};

#endif

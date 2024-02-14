#ifndef __TEST_MOEX_H__
#define __TEST_MOEX_H__

#include <vector>
#include "TestEfcFixture.h"

class TestMoex : public TestEfcFixture {
protected:
  void generateMdDataPkts(const void *t) override;
  void sendData() override;

protected:
  std::vector<int> insertedMd_ = {};
};

/* --------------------------------------------- */

#endif

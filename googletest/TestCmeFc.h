#ifndef __TEST_CME_FC_H__
#define __TEST_CME_FC_H__

#include "TestEfcFixture.h"

class TestCmeFc : public TestEfcFixture {
protected:
  void configure(const TestCaseConfig *t) override;
  void sendData(const void *mdInjectParams) override;
};

/* --------------------------------------------- */

#endif

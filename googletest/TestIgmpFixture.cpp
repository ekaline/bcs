#include "TestIgmpFixture.h"

static void *
onEfhGroupStateChange(const EfhGroupStateChangedMsg *msg,
                      EfhSecUserData secData,
                      EfhRunUserData userData) {
  return NULL;
}

size_t TestIgmpFixture::prepareProps(
    int runGrId, TestProp *testProp, EkaProp *prop,
    const TestEfhRunGrConfig *rg) {
  size_t propId = 0;
  for (EkaLSI grId = 0; grId < rg->nMcgroups; grId++) {
    sprintf(testProp[propId].key,
            "efh.%s.group.%d.mcast.addr",
            EKA_EXCH_DECODE(rg->exch),
            grId);
    sprintf(testProp[propId].val,
            "224.%d.%d.%d:%d",
            rg->coreId,
            runGrId,
            rg->firstGrId + grId,
            18000 + rg->firstGrId + grId);
    prop[propId].szKey = testProp[propId].key;
    prop[propId].szVal = testProp[propId].val;
    propId++;
  }
  return propId;
};

void TestIgmpFixture::prepareGroups(
    EkaGroup *pGroups, const TestEfhRunGrConfig *rg) {
  size_t propId = 0;
  for (EkaLSI grId = 0; grId < rg->nMcgroups; grId++) {
    pGroups[grId].source = rg->exch;
    pGroups[grId].localId = rg->firstGrId + grId;
  }
};

void TestIgmpFixture::runIgmpTest() {
  int runGrId = 0;
  const size_t MaxProps = 64;
  for (const auto &rg : runGroups_) {
    /* ------------------------------------------- */
    auto testProp = new TestProp[MaxProps];
    auto prop = new EkaProp[MaxProps];
    EkaProps props = {.numProps = prepareProps(
                          runGrId, testProp, prop, &rg),
                      .props = prop};
    EfhInitCtx initCtx = {.ekaProps = &props,
                          .numOfGroups = rg.nMcgroups,
                          .coreId = rg.coreId,
                          .recvSoftwareMd = true};
    EfhCtx *pEfhCtx = NULL;
    efhInit(&pEfhCtx, g_ekaDev, &initCtx);

    delete[] testProp;
    delete[] prop;

    EXPECT_EQ(pEfhCtx->dev, g_ekaDev);

    /* ------------------------------------------- */
    pGroups_[runGrId] = new EkaGroup[rg.nMcgroups];
    auto pGroups = pGroups_[runGrId];
    prepareGroups(pGroups, &rg);

    efhRunCtx_[runGrId].groups = pGroups;
    efhRunCtx_[runGrId].numGroups = rg.nMcgroups;
    efhRunCtx_[runGrId].onEfhGroupStateChangedMsgCb =
        onEfhGroupStateChange;

    efhRunThread_[runGrId] =
        std::thread(efhRunGroups,
                    pEfhCtx,
                    &efhRunCtx_[runGrId],
                    (void **)NULL);
    runGrId++;
  }
  fflush(g_ekaLogFile);
  sleep(2);
  efhDestroy(NULL);

  runGrId = 0;
  for (const auto &rg : runGroups_) {
    efhRunThread_[runGrId].join();

    delete[] pGroups_[runGrId];
    runGrId++;
  }
}

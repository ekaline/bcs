#include <random>

#include "TestP4PredefinedCtx.h"

/* ############################################# */

TestP4PredefinedCtx::TestP4PredefinedCtx() {}
/* ############################################# */

TestP4PredefinedCtx::~TestP4PredefinedCtx() {
#if 0
outputFile_.close();
#endif
}
/* ############################################# */
void TestP4PredefinedCtx::loadSecCtxsMd() {
  std::ifstream inputFile(ctxFileName_);
  cereal::JSONInputArchive archive(inputFile);
  EKA_LOG("Loading predefined allSecs_");
  archive(CEREAL_NVP(allSecs_));
  EKA_LOG("Loaded %ju predefined allSecs_ elems",
          allSecs_.size());

  EKA_LOG("Loading predefined insertedMd_");
  archive(CEREAL_NVP(insertedMd_));
  EKA_LOG("Loaded %ju predefined insertedMd_ elems",
          insertedMd_.size());
}
/* ############################################# */
void TestP4PredefinedCtx::createSecList(
    const void *unused) {
  EKA_LOG("Using %ju predefined allSecs_ elems",
          allSecs_.size());

  nSec_ = 0;
  for (const auto &sec : allSecs_) {
    secList_[nSec_++] = sec.binId;
  }
}
/* ############################################# */

/* ############################################# */

/* ############################################# */

/* ############################################# */
/* --------------------------------------------- */

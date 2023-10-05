#include <random>

#include "TestP4FixedSecs.h"

/* ############################################# */

void TestP4FixedSecs::createSecList(const void *unused) {
  nSec_ = 0;
  for (const auto &s : fixedSecs_) {
    TestP4CboeSec sec = {
        .strId = s,
        .binId = getBinSecId(s),
        .bidMinPrice = static_cast<TestP4SecCtxPrice>(
            1 + rand() % 0x7FFF),
        .askMaxPrice = static_cast<TestP4SecCtxPrice>(
            1 + rand() % 0x7FFF),
        .size = 1,
        .valid = true,
        .handle = -1};
    allSecs_.push_back(sec);

    secList_[nSec_] = sec.binId;
    nSec_++;
  }

  EKA_LOG("Created List of Total %ju (== %ju) P4 "
          "Securities:",
          allSecs_.size(), nSec_);

  for (const auto &sec : allSecs_)
    EKA_LOG("\t\'%.8s\', 0x%jx, "
            "bidMinPrice=%u, "
            "askMaxPrice=%u, ",
            sec.strId.c_str(), sec.binId, sec.bidMinPrice,
            sec.askMaxPrice);
}

/* ############################################# */

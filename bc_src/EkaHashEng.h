#ifndef __EKA_HASH_ENG_H__
#define __EKA_HASH_ENG_H__

#include "eka_macros.h"

#include "EkaHashEngLine.h"

class EkaHashEngBase {
public:
  virtual ~EkaHashEngBase() {}
};

/* =================================================== */
template <class SecIdT, const EfhFeedVer FeedVer,
          const size_t Rows, const size_t Cols>
class EkaHashEng : public EkaHashEngBase {
public:
  EkaHashEng() {
    for (auto i = 0; i < Rows; i++) {
      hashLine_[i] = new HashLineT(i);
      if (!hashLine_[i])
        on_error("!hashLine_[%d]", i);
    }

    secIdList_ = new SecIdT[Rows * Cols];
    if (!secIdList_)
      on_error("!secIdList_");
    memset(secIdList_, 0, Rows * Cols);
  }
  /* --------------------------------------------------- */
  virtual ~EkaHashEng() {}
  /* --------------------------------------------------- */
  int getLineIdx(SecIdT normSecId) {
    return (int)normSecId & (Rows - 1);
  }

  /* --------------------------------------------------- */

  bool subscribeSec(SecIdT secId) {
    if (numSecurities_ == Rows * Cols) {
      EKA_WARN("numSecurities_ %d  == Rows * Cols %ju : "
               "secId %ju (0x%jx) is ignored",
               numSecurities_, Rows * Cols, (uint64_t)secId,
               (uint64_t)secId);
      return false;
    }

    int lineIdx = getLineIdx(secId);

    //  EKA_DEBUG("Subscribing on 0x%jx, lineIdx = 0x%x
    //  (%d)",secId,lineIdx,lineIdx);
    if (hashLine_[lineIdx]->addSecurity(secId))
      numSecurities_++;
    return true;
  }
  /* --------------------------------------------------- */

  void packDB() {
    int sum = 0;
    for (auto i = 0; i < Rows; i++) {
      sum += hashLine_[i]->pack(sum);
    }
  }
  /* --------------------------------------------------- */

  std::pair<int, size_t> getPackedLine(int id, void *dst) {
    return hashLine_[id]->getPacked(dst);
  }

private:
  /* --------------------------------------------------- */

private:
  using HashLineT =
      EkaHashEngLine<SecIdT, FeedVer, Rows, Cols>;
  HashLineT *hashLine_[Rows] = {};

  int numSecurities_ = 0;

  SecIdT *secIdList_ = nullptr;
};

#endif

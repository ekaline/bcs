#ifndef __EKA_HASH_ENG_LINE_H__
#define __EKA_HASH_ENG_LINE_H__

#include "eka_macros.h"

class EkaHashEngLineBase {
public:
  virtual ~EkaHashEngLineBase() {}
};

template <class SecIdT, const EfhFeedVer FeedVer,
          const size_t Rows, const size_t Cols>
class EkaHashEngLine : public EkaHashEngLineBase {
public:
  EkaHashEngLine(int id) { id_ = id; }
  /* --------------------------------------------------- */

  bool addSecurity(SecIdT secId) {
    if (validCnt_ == Cols) {
      EKA_WARN("No room for %ju in Hash Row %d",
               (uint64_t)secId, id_);
      return false;
    }
    auto hash = getHash(secId);

    for (auto i = 0; i < validCnt_; i++) {
      if (col_[i].hash == hash) {
        if (col_[i].secId != secId)
          on_error(
              "Hash error! Hash line %d: Securities %jx "
              "(%ju) and %jx (%ju) have same hash %04x",
              id_, (uint64_t)secId, (uint64_t)secId,
              (uint64_t)col_[i].secId,
              (uint64_t)col_[i].secId, hash);
        EKA_WARN("secId %jx (%ju) is already subscribed at "
                 "Hash line %d with hash %04x",
                 (uint64_t)secId, (uint64_t)secId, id_,
                 hash);
        return false;
      }
    }

    col_[validCnt_].hash = hash;
    col_[validCnt_].secId = secId;
    col_[validCnt_].valid = true;
    validCnt_++;
    return true;
  }
  /* --------------------------------------------------- */

  int getSubscriptionId(SecIdT secId) {
    for (auto i = 0; i < validCnt_; i++) {
      if (col_[i].secId == secId)
        return sum_ + i;
    }
    return -1;
  }
  /* --------------------------------------------------- */

  int pack(int sum) {
    sum_ = sum;
    packed_.attr = sum_;
    packed_.attr |= (validCnt_ << 24);
    for (auto i = 0; i < Cols; i++) {
      packed_.col[i] = col_[i].hash;
    }
    return validCnt_;
  }
  /* --------------------------------------------------- */

  int downloadPacked() {
    int packedBytes =
        roundUp<int>(Cols * HashSize, 8) / 8 + 4;
    int packedWords = roundUp<int>(packedBytes, 8) / 8;

    uint64_t *pWord = (uint64_t *)&packed_;
    for (auto i = 0; i < packedWords; i++) {
      //    EKA_LOG("%s %d: pWord = %016jx",msg,i,*pWord);
      pWord++;
    }
    return 0;
  }
  /* --------------------------------------------------- */

private:
  /* --------------------------------------------------- */

  uint16_t getHash(uint64_t normSecId) {
    switch (FeedVer) {
    case EfhFeedVer::kMIAX:
      return (normSecId >> 24) & 0xFF;
    default:
      return normSecId / Rows;
    }
  }
  /* --------------------------------------------------- */

  int printLine(const char *msg) {
    char str[16000] = {};
    char *s = str;
    s += sprintf(s, "%s: line %d: validCnt = %u, sum=%u, ",
                 msg, id_, validCnt_, sum_);
    for (auto i = 0; i < validCnt_; i++) {
      s += sprintf(s, "%jx:%x, ", col_[i].secId,
                   col_[i].hash);
    }
    EKA_LOG("%s", str);
#if 0
  EKA_LOG("%s: line %d: validCnt = %u, sum=%u, %jx:%x, "
          "%jx:%x, %jx:%x, %jx:%x",
          msg, id_, validCnt_, sum_, col_[0].secId, col_[0].hash,
          col_[1].secId, col_[1].hash, col_[2].secId,
          col_[2].hash, col_[3].secId, col_[3].hash);
#endif
    return 0;
  }
  /* --------------------------------------------------- */

  int printPacked(const char *msg) {
    int packedBytes =
        roundUp<int>(Cols * HashSize, 8) / 8 + 4;
    int packedWords = roundUp<int>(packedBytes, 8) / 8;

    uint64_t *pWord = (uint64_t *)&packed_;
    for (auto i = 0; i < packedWords; i++) {
      //    EKA_LOG("%s %d: pWord = %016jx",msg,i,*pWord);
      pWord++;
    }
    return 0;
  }
  /* --------------------------------------------------- */
public:
  std::pair<int, size_t> getPacked(void *dst) {
    memcpy(dst, &packed_, sizeof(packed_));
    return std::make_pair(validCnt_, sizeof(packed_));
  }

private:
  static const int HashSize = 8;

  struct PackedHashLine {
    uint32_t attr;
    uint16_t col[Cols];
  } __attribute__((packed));
  PackedHashLine packed_ = {};

  struct HashCol {
    bool valid = false;
    SecIdT secId = 0;
    uint16_t hash = 0;
  };

  int validCnt_ = 0;
  uint32_t sum_ = 0;

  HashCol col_[Cols] = {};
  EkaDev *dev = NULL;
  int id_ = -1;
};

#endif

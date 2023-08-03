#ifndef __EKA_FH_BATS_TRANSACTION_H__
#define __EKA_FH_BATS_TRANSACTION_H__
#include <chrono>

#include "EkaFhFullBook.h"
#include "eka_macros.h"

template <class SecurityT> class EkaFhBatsTransaction {
public:
  EkaFhBatsTransaction() {
    reset();
    active_ = false;
  }
  virtual ~EkaFhBatsTransaction() = default;

  /* -------------------------------------------- */
  inline void reset() {
    nPendingSecurities_ = 0;
    //  m_[MAX_PENDING_SECURITIES] = {};
  }
  /* -------------------------------------------- */

  inline void open() {
    reset();
    active_ = true;
  }
  /* -------------------------------------------- */

  inline void close() { active_ = false; }
  /* -------------------------------------------- */

  inline void invalidate() { s_.valid_ = false; }
  /* -------------------------------------------- */
  inline bool isValid() { return s_.valid_; }
  /* -------------------------------------------- */
  inline bool isActive() { return active_; }
  /* -------------------------------------------- */

  inline void pushTob(SecurityT *s, uint64_t seq,
                      uint64_t ts) {
    if (s_.valid_ && !s_.hasSameSec(s))
      on_error("pushing to valid TOB buf");

#if 0
    TEST_LOG("%s, %ju, TOB buf hit", ts_ns2str(ts).c_str(),
             seq);
#endif
    s_.set(s, seq, ts);
    return;
  }
  /* -------------------------------------------- */
  inline SecurityT *getSecPtr() { return s_.secPtr_; }
  /* -------------------------------------------- */
  inline uint64_t getSecTs() { return s_.ts_; }
  /* -------------------------------------------- */
  inline uint64_t getSecSeq() { return s_.seq_; }
  /* -------------------------------------------- */

  inline void pushTob__old(SecurityT *s, uint64_t seq,
                           uint64_t ts) {
    for (size_t i = 0; i < nPendingSecurities_; i++) {
      if (m_[i].hasSameSec(s)) {
        // exclude from the middle and append
        for (size_t j = i; j < nPendingSecurities_ - 1;
             j++) {
          m_[j].copyContent(&m_[j + 1]);
        }
        m_[nPendingSecurities_ - 1].set(s, seq, ts);
        goto EXIT;
      }
    }
    // s not found in m_[]
    if (nPendingSecurities_ == MAX_PENDING_SECURITIES)
      on_error("nPendingSecurities_ %ju == "
               "MAX_PENDING_SECURITIES %ju",
               nPendingSecurities_, MAX_PENDING_SECURITIES);
    m_[nPendingSecurities_].set(s, seq, ts);
    nPendingSecurities_++;
  EXIT:
    return;
  }
  /* -------------------------------------------- */

private:
  /* -------------------------------------------- */
  class TobCtx {
  public:
    inline void set(SecurityT *s, uint64_t seq,
                    uint64_t ts) {
      secPtr_ = s;
      seq_ = seq;
      ts_ = ts;
      valid_ = true;
    }

    inline void copyContent(const TobCtx *d) {
      secPtr_ = d->secPtr_;
      seq_ = d->seq_;
      ts_ = d->ts_;
      valid_ = true;
    }

    inline bool hasSameSec(const SecurityT *secPtr) {
      return secPtr_ == secPtr;
    }

    SecurityT *secPtr_;
    uint64_t seq_;
    uint64_t ts_;
    bool valid_ = false;
  };
  /* -------------------------------------------- */

  static const size_t MAX_PENDING_SECURITIES = 64;
  bool active_ = false;

public:
  TobCtx m_[MAX_PENDING_SECURITIES] = {};
  size_t nPendingSecurities_ = 0;

  TobCtx s_ = {};
};

#endif

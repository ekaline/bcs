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

  inline bool isActive() { return active_; }
  /* -------------------------------------------- */

  inline void pushSecurityCtx(SecurityT *s, uint64_t seq,
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
  class SecurityCtx {
  public:
    inline void set(SecurityT *s, uint64_t seq,
                    uint64_t ts) {
      secPtr_ = s;
      seq_ = seq;
      ts_ = ts;
    }

    inline void copyContent(const SecurityCtx *d) {
      secPtr_ = d->secPtr_;
      seq_ = d->seq_;
      ts_ = d->ts_;
    }

    inline bool hasSameSec(const SecurityT *secPtr) {
      return secPtr_ == secPtr;
    }

    SecurityT *secPtr_;
    uint64_t seq_;
    uint64_t ts_;
  };
  /* -------------------------------------------- */

  static const size_t MAX_PENDING_SECURITIES = 512;
  bool active_ = false;

public:
  SecurityCtx m_[MAX_PENDING_SECURITIES] = {};
  size_t nPendingSecurities_ = 0;
};

#endif

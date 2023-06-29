#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <linux/sockios.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "Efc.h"
#include "Efh.h"
#include "EkaCore.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"

#include "EkaEfcDataStructs.h"

#include "EkaP4Strategy.h"

/* --------------------------------------------------- */
EkaP4Strategy::EkaP4Strategy(
    EfhFeedVer feedVer, const EpmStrategyParams *params)
    : EkaStrategy(feedVer, params) {

  feedVer_ = feedVer;
  name_ = "P4_" + EKA_FEED_VER_DECODE(feedVer_);
  EKA_LOG("Creating %s with %d MC groups", name_.c_str(),
          numUdpSess_);

  disableRxFire();
  eka_write(dev, P4_STRAT_CONF, (uint64_t)0);

  preallocateFireActions();
  configureTemplates();
  configureEhp();
  initHwRoundTable();
  createSecHash();

#ifndef _VERILOG_SIM
  cleanSubscrHwTable();
  cleanSecHwCtx();
  eka_write(dev, SCRPAD_EFC_SUBSCR_CNT, 0);
#endif

  auto swStatistics = eka_read(dev, SW_STATISTICS);
  eka_write(dev, SW_STATISTICS,
            swStatistics | (1ULL << 63));
}
/* --------------------------------------------------- */

void EkaP4Strategy::preallocateFireActions() {
  EpmActionType actionType = EpmActionType::INVALID;
  switch (feedVer_) {
  case EfhFeedVer::kCBOE:
    actionType = EpmActionType::BoeFire;
    break;
  case EfhFeedVer::kNASDAQ:
    actionType = EpmActionType::SqfFire;
    break;
  default:
    on_error("Unexpected feedVer %d", (int)feedVer);
  }

  auto regionId = EkaEpmRegion::Regions::Efc;
  epm_->createActionMtx.lock();

  for (auto i = 0; i < numUdpSess_; i++) {
    auto localIdx = i;
    auto globalIdx =
        EkaEpmRegion::getBaseActionIdx(regionId) + localIdx;

    uint64_t actionAddr = EkaEpm::EpmActionBase +
                          globalIdx * EkaEpm::ActionBudget;

    uint heapOffs =
        EkaEpmRegion::getActionHeapOffs(regionId, localIdx);

    epm_->occupyAction(globalIdx);

    a_[i] = new EkaEpmAction(
        dev_, actionType, globalIdx, localIdx, regionId,
        -1 /* coreId */, -1 /* sessId */, -1 /* auxIdx */);

    if (!a_[i])
      on_error("Failed on addAction()");
  }
  epm_->createActionMtx.unlock();
}
/* --------------------------------------------------- */
void EkaP4Strategy::configureTemplates() {
  switch (feedVer_) {
  case EfhFeedVer::kBATS:
    epm_->template
        [EkaEpm::TemplateId::BoeQuoteUpdateShort] =
        new EpmBoeQuoteUpdateShortTemplate(
            EkaEpm::TemplateId::BoeQuoteUpdateShort);
    break;

  default:
    on_error("Unexpected feedVer_ %s (%d)",
             EKA_FEED_VER_DECODE(feedVer_), feedVer_);
  }
}
/* --------------------------------------------------- */
void EkaP4Strategy::configureEhp() {
  switch (feedVer_) {
  case EfhFeedVer::kBATS: {
    auto ehp = new EhpPitch(dev);
    if (!ehp)
      on_error("!ehp");
    ehp->init();
    ehp->download2Hw();
  } break;

  default:
    on_error("Unexpected feedVer_ %s (%d)",
             EKA_FEED_VER_DECODE(feedVer_), feedVer_);
  }
}

/* --------------------------------------------------- */

static bool isAscii(char letter) {
  //  EKA_LOG("testing %d", letter);
  if ((letter >= '0' && letter <= '9') ||
      (letter >= 'a' && letter <= 'z') ||
      (letter >= 'A' && letter <= 'Z'))
    return true;
  return false;
}

/* --------------------------------------------------- */
int EkaP4Strategy::cleanSubscrHwTable() {
  EKA_LOG("Cleaning HW Subscription Table: %d rows, %d "
          "words per row",
          EFC_SUBSCR_TABLE_ROWS, EFC_SUBSCR_TABLE_COLUMNS);

  uint64_t val = eka_read(dev, SW_STATISTICS);
  val &= 0xFFFFFFFF00000000;
  eka_write(dev, SW_STATISTICS, val);
  return 0;
}
/* --------------------------------------------------- */
int EkaP4Strategy::cleanSecHwCtx() {
  EKA_LOG("Cleaning HW Contexts of %d securities",
          MAX_SEC_CTX);

  for (EfcSecCtxHandle handle = 0;
       handle < (EfcSecCtxHandle)MAX_SEC_CTX; handle++) {
    const EkaHwSecCtx hwSecCtx = {};
    writeSecHwCtx(handle, &hwSecCtx, 0 /* writeChan */);
  }

  return 0;
}
/* --------------------------------------------------- */

inline void
EkaP4Strategy::writeSecHwCtx(const EfcSecCtxHandle handle,
                             const EkaHwSecCtx *pHwSecCtx,
                             uint16_t writeChan) {
  uint64_t ctxWrAddr =
      P4_CTX_CHANNEL_BASE +
      writeChan * EKA_BANKS_PER_CTX_THREAD *
          EKA_WORDS_PER_CTX_BANK * 8 +
      ctxWriteBank[writeChan] * EKA_WORDS_PER_CTX_BANK * 8;

  // EkaHwSecCtx is 8 Bytes ==> single write
  eka_write(dev, ctxWrAddr, *(uint64_t *)pHwSecCtx);

  union large_table_desc done_val = {};
  done_val.ltd.src_bank = ctxWriteBank[writeChan];
  done_val.ltd.src_thread = writeChan;
  done_val.ltd.target_idx = handle;
  eka_write(dev, P4_CONFIRM_REG, done_val.lt_desc);

  ctxWriteBank[writeChan] = (ctxWriteBank[writeChan] + 1) %
                            EKA_BANKS_PER_CTX_THREAD;
}
/* --------------------------------------------------- */

int EkaP4Strategy::initHwRoundTable() {
#ifdef _VERILOG_SIM
  return 0;
#else

  for (uint64_t addr = 0; addr < ROUND_2B_TABLE_DEPTH;
       addr++) {
    uint64_t data = 0;
    switch (hwFeedVer) {
    case EfhFeedVer::kPHLX:
    case EfhFeedVer::kGEMX:
      data = (addr / 10) * 10;
      break;
    case EfhFeedVer::kNASDAQ:
    case EfhFeedVer::kMIAX:
    case EfhFeedVer::kCBOE:
    case EfhFeedVer::kCME:
    case EfhFeedVer::kQED:
    case EfhFeedVer::kNEWS:
    case EfhFeedVer::kITCHFS:
      data = addr;
      break;
    default:
      on_error("Unexpected hwFeedVer = 0x%x",
               (int)hwFeedVer);
    }

    uint64_t indAddr = 0x0100000000000000 + addr;
    indirectWrite(dev, indAddr, data);

    /* eka_write (dev,ROUND_2B_ADDR,addr); */
    /* eka_write (dev,ROUND_2B_DATA,data); */
    //    EKA_LOG("%016x (%ju) @ %016x
    //    (%ju)",data,data,addr,addr);
  }
#endif
  return 0;
}
/* --------------------------------------------------- */
static bool isValidCboeSecondByte(char c) {
  switch (c) {
  case '0':
  case '1':
  case '2':
  case '3':
    return true;
  default:
    return false;
  }
}
/* --------------------------------------------------- */

bool EkaP4Strategy::isValidSecId(uint64_t secId) {
  switch (hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
    if ((secId & ~0x1FFFFFULL) != 0)
      return false;
    return true;

  case EfhFeedVer::kMIAX:
    if ((secId & ~0x3E00FFFFULL) != 0)
      return false;
    return true;

  case EfhFeedVer::kCBOE:
    if (((char)((secId >> (8 * 5)) & 0xFF) != '0') ||
        !isValidCboeSecondByte(
            (char)((secId >> (8 * 4)) & 0xFF)) ||
        !isAscii((char)((secId >> (8 * 3)) & 0xFF)) ||
        !isAscii((char)((secId >> (8 * 2)) & 0xFF)) ||
        !isAscii((char)((secId >> (8 * 1)) & 0xFF)) ||
        !isAscii((char)((secId >> (8 * 0)) & 0xFF)))
      return false;
    return true;

  default:
    on_error("Unexpected hwFeedVer: %d", (int)hwFeedVer);
  }
}

/* --------------------------------------------------- */
static uint64_t char2num(char c) {
  if (c >= '0' && c <= '9')
    return c - '0'; // 10
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 10; // 36
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 10 + 26; // 62
  on_error("Unexpected char \'%c\' (0x%x)", c, c);
}
/* --------------------------------------------------- */
int EkaP4Strategy::normalizeId(uint64_t secId) {
  switch (hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
  case EfhFeedVer::kMIAX:
    return secId;
  case EfhFeedVer::kCBOE: {
    uint64_t res = 0;
    char c = '\0';
    uint64_t n = 0;

    c = (char)((secId >> (8 * 0)) & 0xFF);
    n = char2num(c) << (6 * 0);
    res |= n;

    c = (char)((secId >> (8 * 1)) & 0xFF);
    n = char2num(c) << (6 * 1);
    res |= n;

    c = (char)((secId >> (8 * 2)) & 0xFF);
    n = char2num(c) << (6 * 2);
    res |= n;

    c = (char)((secId >> (8 * 3)) & 0xFF);
    n = char2num(c) << (6 * 3);
    res |= n;

    c = (char)((secId >> (8 * 4)) & 0xFF);
    n = char2num(c) << (6 * 4);
    res |= n;

    return res;
  }
  default:
    on_error("Unexpected hwFeedVer: %d", (int)hwFeedVer);
  }
}
/* --------------------------------------------------- */
int EkaP4Strategy::getLineIdx(uint64_t normSecId) {
  return (int)normSecId & (EFC_SUBSCR_TABLE_ROWS - 1);
}
/* --------------------------------------------------- */
int EkaP4Strategy::subscribeSec(uint64_t secId) {
  if (!isValidSecId(secId))
    return -1;
  //    on_error("Security %ju (0x%jx) violates Hash
  //    function assumption",secId,secId);

  if (numSecurities_ == EKA_MAX_P4_SUBSCR) {
    EKA_WARN("numSecurities_ %d  == EKA_MAX_P4_SUBSCR: "
             "secId %ju (0x%jx) is ignored",
             numSecurities_, secId, secId);
    return -1;
  }

  uint64_t normSecId = normalizeId(secId);
  int lineIdx = getLineIdx(normSecId);

  //  EKA_DEBUG("Subscribing on 0x%jx, lineIdx = 0x%x
  //  (%d)",secId,lineIdx,lineIdx);
  if (hashLine[lineIdx]->addSecurity(normSecId)) {
    numSecurities_++;
    uint64_t val = eka_read(dev, SW_STATISTICS);
    val &= 0xFFFFFFFF00000000;
    val |= (uint64_t)(numSecurities_);

#ifndef _VERILOG_SIM
    eka_write(dev, SW_STATISTICS, val);
#endif
  }
  return 0;
}
/* --------------------------------------------------- */
EfcSecCtxHandle
EkaP4Strategy::getSubscriptionId(uint64_t secId) {
  if (!isValidSecId(secId))
    return -1;
  //    on_error("Security %ju (0x%jx) violates Hash
  //    function assumption",secId,secId);
  uint64_t normSecId = normalizeId(secId);
  int lineIdx = getLineIdx(normSecId);

  auto handle =
      hashLine[lineIdx]->getSubscriptionId(normSecId);

  secIdList_[handle] = secId;

  return handle;
}

/* --------------------------------------------------- */
int EkaP4Strategy::downloadTable() {
  int sum = 0;
  for (auto i = 0; i < EFC_SUBSCR_TABLE_ROWS; i++) {
    struct timespec req = {0, 1000};
    struct timespec rem = {};

    sum += hashLine[i]->pack(sum);
    hashLine[i]->downloadPacked();

    nanosleep(
        &req,
        &rem); // added due to "too fast" write to card
  }

  if (sum != numSecurities_)
    on_error("sum %d != numSecurities_ %d", sum,
             numSecurities_);

  return 0;
}
/* --------------------------------------------------- */

void EkaP4Strategy::createSecHash() {
  for (auto i = 0; i < EFC_SUBSCR_TABLE_ROWS; i++) {
    hashLine[i] = new EkaHwHashTableLine(dev, hwFeedVer, i);
    if (!hashLine[i])
      on_error("!hashLine[%d]", i);
  }

  secIdList_ = new uint64_t[EFC_SUBSCR_TABLE_ROWS *
                            EFC_SUBSCR_TABLE_COLUMNS];
  if (!secIdList_)
    on_error("!secIdList_");
  memset(secIdList_, 0,
         EFC_SUBSCR_TABLE_ROWS * EFC_SUBSCR_TABLE_COLUMNS);
}
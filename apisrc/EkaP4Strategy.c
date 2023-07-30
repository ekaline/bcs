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
#include "EkaEpm.h"

#include "EkaCore.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"

#include "EkaEfcDataStructs.h"

#include "EkaP4Strategy.h"

#include "EhpCmeFC.h"
#include "EhpItchFS.h"
#include "EhpNews.h"
#include "EhpNom.h"
#include "EhpPitch.h"
#include "EhpQED.h"

#include "EpmBoeQuoteUpdateShortTemplate.h"
#include "EpmCancelBoeTemplate.h"
#include "EpmCmeILinkHbTemplate.h"
#include "EpmCmeILinkSwTemplate.h"
#include "EpmCmeILinkTemplate.h"
#include "EpmFastSweepUDPReactTemplate.h"
#include "EpmFireBoeTemplate.h"
#include "EpmFireSqfTemplate.h"

#include "EkaHwHashTableLine.h"

/* --------------------------------------------------- */
EkaP4Strategy::EkaP4Strategy(const EfcUdpMcParams *mcParams,
                             const EfcP4Params *p4Params)
    : EkaStrategyEhp(mcParams) {

  feedVer_ = p4Params->feedVer;
  name_ =
      "P4_" + std::string(EKA_FEED_VER_DECODE(feedVer_));

  maxSize_ = p4Params->max_size;
  eka_write(dev_, P4_GLOBAL_MAX_SIZE, maxSize_);

  fireOnAllAddOrders_ = p4Params->fireOnAllAddOrders;

  EKA_LOG("Creating %s with %d MC groups on lane #0 "
          "and %d MC groups on lane #1, "
          "max_size = %u",
          name_.c_str(), mcCoreSess_[0].numUdpSess,
          mcCoreSess_[1].numUdpSess, maxSize_);

  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);

  configureTemplates();
  preallocateFireActions();
  configureEhp();
  // downloadEhp2Hw();

  initHwRoundTable();
  createSecHash();

#ifndef _VERILOG_SIM
  cleanSubscrHwTable();
  cleanSecHwCtx();
  eka_write(dev_, SCRPAD_EFC_SUBSCR_CNT, 0);
#endif

  auto swStatistics = eka_read(dev_, SW_STATISTICS);
  eka_write(dev_, SW_STATISTICS,
            swStatistics | (1ULL << 63));
}

/* --------------------------------------------------- */
void EkaP4Strategy::arm(EfcArmVer ver) {
  EKA_LOG("Arming %s", name_.c_str());
  uint64_t armData = ((uint64_t)ver << 32) | 1;
  eka_write(dev_, ArmDisarmP4Addr, armData);
}
/* --------------------------------------------------- */

void EkaP4Strategy::disarm() {
  EKA_LOG("Disarming %s", name_.c_str());
  eka_write(dev_, ArmDisarmP4Addr, 0);
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
    on_error("Unexpected feedVer %d", (int)feedVer_);
  }

  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++)
    for (auto i = 0; i < mcCoreSess_[coreId].numUdpSess;
         i++) {
      auto actionIdx =
          coreId * EFC_PREALLOCATED_P4_ACTIONS_PER_LANE + i;

      auto udpSess = mcCoreSess_[coreId].udpSess[i];
      if (udpSess->coreId != coreId)
        on_error("udpSess->coreId %d != coreId %d",
                 udpSess->coreId, coreId);

      EKA_LOG("Preallocating P4 1st Fire Action %d for "
              "MC group: %s:%u",
              actionIdx, EKA_IP2STR(udpSess->ip),
              udpSess->port);
      epm_->addAction(actionType, actionIdx, regionId_);
    }
}
/* --------------------------------------------------- */
void EkaP4Strategy::configureTemplates() {
  int templateIdx = -1;
  switch (feedVer_) {
  case EfhFeedVer::kCBOE:
    templateIdx =
        (int)EkaEpm::TemplateId::BoeQuoteUpdateShort;
    epm_->epmTemplate[templateIdx] =
        new EpmBoeQuoteUpdateShortTemplate(templateIdx);

    epm_->DownloadSingleTemplate2HW(
        epm_->epmTemplate[templateIdx]);
    break;

  default:
    on_error("Unexpected feedVer_ %s (%d)",
             EKA_FEED_VER_DECODE(feedVer_), (int)feedVer_);
  }
}
/* --------------------------------------------------- */
/* void EkaP4Strategy::configureEhp() {
  switch (feedVer_) {
  case EfhFeedVer::kCBOE:
    ehp_ = new EhpPitch(dev_, fireOnAllAddOrders_);
    break;

  default:
    on_error("Unexpected feedVer_ %s (%d)",
             EKA_FEED_VER_DECODE(feedVer_), (int)feedVer_);
  }

  if (!ehp_)
    on_error("!ehp_");
} */

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
void EkaP4Strategy::cleanSubscrHwTable() {
  EKA_LOG("Cleaning HW Subscription Table: %d rows, %d "
          "words per row",
          EFC_SUBSCR_TABLE_ROWS, EFC_SUBSCR_TABLE_COLUMNS);

  uint64_t val = eka_read(dev_, SW_STATISTICS);
  val &= 0xFFFFFFFF00000000;
  eka_write(dev_, SW_STATISTICS, val);
}
/* --------------------------------------------------- */
void EkaP4Strategy::cleanSecHwCtx() {
  EKA_LOG("Cleaning HW Contexts of %d securities",
          MAX_SEC_CTX);

  for (EfcSecCtxHandle handle = 0;
       handle < (EfcSecCtxHandle)MAX_SEC_CTX; handle++) {
    const EkaHwSecCtx hwSecCtx = {};
    writeSecHwCtx(handle, &hwSecCtx, 0 /* writeChan */);
  }
}
/* --------------------------------------------------- */

void EkaP4Strategy::writeSecHwCtx(
    const EfcSecCtxHandle handle,
    const EkaHwSecCtx *pHwSecCtx, uint16_t writeChan) {
  uint64_t ctxWrAddr =
      P4_CTX_CHANNEL_BASE +
      writeChan * EKA_BANKS_PER_CTX_THREAD *
          EKA_WORDS_PER_CTX_BANK * 8 +
      ctxWriteBank[writeChan] * EKA_WORDS_PER_CTX_BANK * 8;

  // EkaHwSecCtx is 8 Bytes ==> single write
  eka_write(dev_, ctxWrAddr, *(uint64_t *)pHwSecCtx);

  union large_table_desc done_val = {};
  done_val.ltd.src_bank = ctxWriteBank[writeChan];
  done_val.ltd.src_thread = writeChan;
  done_val.ltd.target_idx = handle;
  eka_write(dev_, P4_CONFIRM_REG, done_val.lt_desc);

  ctxWriteBank[writeChan] = (ctxWriteBank[writeChan] + 1) %
                            EKA_BANKS_PER_CTX_THREAD;
}
/* --------------------------------------------------- */

void EkaP4Strategy::initHwRoundTable() {
#ifdef _VERILOG_SIM
  return 0;
#else

  for (uint64_t addr = 0; addr < ROUND_2B_TABLE_DEPTH;
       addr++) {
    uint64_t data = 0;
    switch (feedVer_) {
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
      on_error("Unexpected feedVer_ = 0x%x", (int)feedVer_);
    }

    uint64_t indAddr = 0x0100000000000000 + addr;
    indirectWrite(dev_, indAddr, data);

    /* eka_write (dev_,ROUND_2B_ADDR,addr); */
    /* eka_write (dev_,ROUND_2B_DATA,data); */
    //    EKA_LOG("%016x (%ju) @ %016x
    //    (%ju)",data,data,addr,addr);
  }
#endif
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
  switch (feedVer_) {
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
    on_error("Unexpected feedVer_: %d", (int)feedVer_);
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
  switch (feedVer_) {
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
    on_error("Unexpected feedVer_: %d", (int)feedVer_);
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
    uint64_t val = eka_read(dev_, SW_STATISTICS);
    val &= 0xFFFFFFFF00000000;
    val |= (uint64_t)(numSecurities_);

#ifndef _VERILOG_SIM
    eka_write(dev_, SW_STATISTICS, val);
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
    hashLine[i] = new EkaHwHashTableLine(dev_, feedVer_, i);
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

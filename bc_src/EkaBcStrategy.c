#include <arpa/inet.h>
#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>

#include "EkaBcStrategy.h"
#include "EkaBook.h"
#include "EkaProd.h"
#include "eka_bc.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "EkaHwInternalStructs.h"
#include "EkaUdpSess.h"

#include "EkaDev.h"

#define EKA_NORMALIZE_FIELD(type, name)                    \
  _dst->name = normalizeField<decltype(_dst->name),        \
                              decltype(_src->name)>(       \
      hwFeedVer, #name, #type, _src->name);

#define EKA_PRINT_STRATEGY_PARAMS_FIELD(type, name)        \
  EKA_LOG("EKA_PRINT_STRATEGY_PARAMS_FIELD: "              \
          "fieldName=%s, typeName=%s, fieldValue=%ju",     \
          #name, #type, _src->name);

EkaBcStrategy::EkaBcStrategy(EkaExch *_exch,
                             EkaExch::StrategyType _type) {
  exch = _exch;
  dev = exch->dev;
  type = _type;

  prodNum = 0;
  numBooks = 0;

  hwFeedVer = exch->hwFeedVer;
  auto_rearm = false;
  memset(&globalParams, 0, sizeof(globalParams));

  for (uint i = 0; i < MAX_BOOKS; i++)
    book[i] = NULL;
  for (uint i = 0; i < EKA_MAX_UDP_SESSIONS_PER_CORE; i++)
    udpSess[i] = NULL;
  udpSessions = 0;

  for (int c = 0; c < EkaDev::CONF::MAX_CORES; c++)
    udpChannel[c] = NULL;

  threadActive = false;
}

/* -------------------------------------------------------------
 */
int EkaBcStrategy::createBook(uint8_t coreId,
                              EkaProd *_prod) {
  if (numBooks >= MAX_BOOKS)
    on_error("numBooks = %u", numBooks);
  EKA_LOG("Creating book[%u]", numBooks);
  fflush(stderr);
  book[numBooks] = exch->newBook(coreId, this, _prod);
  EKA_LOG("book[%u] = %p", numBooks, book[numBooks]);
  fflush(stderr);
  if (book[numBooks] == NULL)
    on_error("book[%u] == NULL", numBooks);
  numBooks++;
  EKA_LOG("numBooks = %u", numBooks);
  fflush(stderr);

  return 0;
}

/* ----------------------------------------------------- */

EkaBook *
EkaBcStrategy::findBook(GlobalExchSecurityId securityId) {
  for (uint i = 0; i < numBooks; i++) {
    if (unlikely(book[i] == NULL))
      on_error("book[%u] = NULL", i);
    if (book[i]->getSecurityId() == securityId)
      return book[i];
  }
  return NULL;
}
/* -------------------------------------------------------------
 */
int EkaBcStrategy::initGlobalParams(
    const struct global_params *params) {
  globalParams.debug_no_tcpsocket =
      params->debug_no_tcpsocket;
  globalParams.report_only = params->report_only;
  globalParams.no_report_on_exception =
      params->no_report_on_exception;
  globalParams.debug_always_fire =
      params->debug_always_fire;
  globalParams.watchdog_timeout_sec =
      params->watchdog_timeout_sec;

  //    dev->debug_no_tcpsocket             =
  //    params->debug_no_tcpsocket;

  /* if (params->book_thread_affinity_core > 36)  */
  /*   on_error("Unsupported
   * params->book_thread_affinity_core =
   * %u",params->book_thread_affinity_core); */
  /* dev->book_thread_affinity_core =
   * params->book_thread_affinity_core; */
  return 0;
}
/* -------------------------------------------------------------
 */
void EkaBcStrategy::addProd(
    const init_product_t *init_product_conf) {
  if (prodNum == CONF::MAX_PROD)
    on_error("prodNum == CONF::MAX_PROD %u", prodNum);
  prod[prodNum] = new EkaProd(dev, hwFeedVer, prodNum,
                              init_product_conf);
  prodNum++;
}
/* -------------------------------------------------------------
 */
uint EkaBcStrategy::findProd(eka_product_t product_id,
                             char const *callingf) {
  for (uint i = 0; i < prodNum; i++) {
    if (prod[i] == NULL)
      on_error("prod[%u] == NULL", i);
    if (prod[i]->product_id == product_id)
      return i;
  }
  on_error("product_id %s (=%u) not found, called from %s",
           EKA_RESOLVE_PROD(hwFeedVer, product_id),
           product_id, callingf);
}
/* -------------------------------------------------------------
 */
EkaProd *EkaBcStrategy::findProdSecId(uint32_t secId) {
  for (uint i = 0; i < prodNum; i++) {
    if (prod[i]->exchange_security_id == secId)
      return prod[i];
  }
  return NULL;
}
/* -------------------------------------------------------------
 */
uint EkaBcStrategy::findProdHw(eka_product_t product_id,
                               char const *callingf) {
  uint64_t book_cnt = 0;
  uint64_t light_cnt = 0;
  uint64_t ret = 0xFFFFFFFFFFFF;
  for (uint i = 0; i < prodNum; i++) {
    if (prod[i]->is_book) {
      if (prod[i]->product_id == product_id) {
        ret = book_cnt;
        break;
      } else {
        book_cnt++;
      }
    } else { // Light
      if (prod[i]->product_id == product_id) {
        ret = light_cnt + EKA_MAX_BOOK_PRODUCTS;
        break;
      } else {
        light_cnt++;
      }
    }
  }
  if (ret == 0xFFFFFFFFFFFF)
    on_error(
        "product_id %s (=%u) not found, called from %s",
        EKA_RESOLVE_PROD(hwFeedVer, product_id), product_id,
        callingf);
  //    EKA_LOG("product_id = %s (%u), ret =
  //    %ju",EKA_RESOLVE_PROD(hwFeedVer,product_id),product_id,ret);
  return ret;
}
/* -------------------------------------------------------------
 */
/* -------------------------------------------------------------
 */
EkaUdpSess *EkaBcStrategy::findUdpSess(uint32_t ip,
                                       uint16_t port) {
  for (uint i = 0; i < udpSessions; i++) {
    if (udpSess[i] == NULL)
      on_error("udpSess[%u] == NULL", i);
    if (udpSess[i]->myParams(ip, port))
      return udpSess[i];
  }
  return NULL;
}
/* -------------------------------------------------------------
 */

uint EkaBcStrategy::addUdpSess(uint8_t coreId, uint32_t ip,
                               uint16_t port) {
  for (uint i = 0; i < udpSessions; i++) {
    if (udpSess[i] == NULL)
      on_error("udpSess[%u] = NULL", i);
    if (udpSess[i]->myParams(ip, port))
      return i;
  }
  if (udpSessions == EKA_MAX_UDP_SESSIONS_PER_CORE)
    on_error("udpSessions = %u", udpSessions);
  uint8_t sessId = udpSessions;
  udpSess[sessId] =
      new EkaUdpSess(dev, coreId, sessId, ip, port);
  udpSessions++;
  return sessId;
}
/* -------------------------------------------------------------
 */

template <class DST_F, class SRC_F>
DST_F normalizeField(uint8_t hwFeedVer,
                     const char *fieldName,
                     const char *typeName, SRC_F src);

template <class DST_F, class SRC_F>
DST_F normalizeField(uint8_t hwFeedVer,
                     const char *fieldName,
                     const char *typeName, SRC_F src) {
  //    EKA_LOG("fieldName=%s, typeName=%s, scrValue = %ju
  //    ",fieldName,typeName,(uint64_t)src);

  if (strcmp(fieldName, "time_delta_ns") == 0) {
    if (src == static_cast<SRC_F>(-1))
      return static_cast<DST_F>(-1);

    if (src % 1000)
      on_error("%s: time delta must have whole number of "
               "microseconds, (not %s nanoseconds) -> ",
               __func__, std::to_string(src).c_str());
    if (src > 64000000)
      on_error("%s: time delta is limited to 64ms (not %s "
               "ns) -> ",
               __func__, std::to_string(src).c_str());

    return static_cast<DST_F>(src / 1000);
  }

  return static_cast<DST_F>(src);
}

template <class DST, class SRC>
void normalizeJumpParams(DST *_dst, SRC *_src,
                         uint8_t hwFeedVer) {
  jump_params_t__ITER(EKA_NORMALIZE_FIELD)
}

void initJumpParams(volatile hw_jump_params_t *dst,
                    const jump_params_t *src) {
  dst->bit_params = ((uint8_t)src->strat_en) << BP_ENABLE;
  if (!src->strat_en)
    return;
  if (src->buy_size > 15 || src->sell_size > 15)
    on_error("too high buy_size %u or sell_size %u",
             src->buy_size, src->sell_size);

  normalizeJumpParams<volatile hw_jump_params_t,
                      const jump_params_t>(dst, src,
                                           SN_CME);
}

template <class DST, class SRC>
void normalizeRJumpParams(DST *_dst, SRC *_src,
                          uint8_t hwFeedVer) {
  //  reference_jump_params_t__ITER (
  //  EKA_PRINT_STRATEGY_PARAMS_FIELD )

  reference_jump_params_t__ITER(EKA_NORMALIZE_FIELD)
}

void initRJumpParams(volatile hw_rjump_params_t *dst,
                     const reference_jump_params_t *src) {
  dst->bit_params = ((uint8_t)src->strat_en) << BP_ENABLE;
  if (!src->strat_en)
    return;
  if (src->buy_size > 15 || src->sell_size > 15)
    on_error("too high buy_size %u or sell_size %u",
             src->buy_size, src->sell_size);

  normalizeRJumpParams<volatile hw_rjump_params_t,
                       const reference_jump_params_t>(
      dst, src, SN_CME);
}

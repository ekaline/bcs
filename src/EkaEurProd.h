#ifndef _EKA_EUR_PROD_H_
#define _EKA_EUR_PROD_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "EkaBc.h"
#include "EkaBookTypes.h"
#include "EkaExch.h"
#include "eka_hw_conf.h"

class EkaDev;
class EkaEpmAction;

#define EKA_RESOLVE_EOBI_PROD(prod)                        \
  prod == EKA_DAX    ? "DAX"                               \
  : prod == EKA_ESX  ? "ESX"                               \
  : prod == EKA_GBL  ? "GBL"                               \
  : prod == EKA_GBM  ? "GBM"                               \
  : prod == EKA_GBS  ? "GBS"                               \
  : prod == EKA_BTP  ? "BTP"                               \
  : prod == EKA_GBX  ? "GBX"                               \
  : prod == EKA_OAT  ? "OAT"                               \
  : prod == EKA_SMI  ? "SMI"                               \
  : prod == EKA_BONO ? "BON"                               \
                     : "---"

#define EKA_RESOLVE_CME_PROD(prod)                         \
  prod == EKA_NQ    ? "NQ"                                 \
  : prod == EKA_ES  ? "ES"                                 \
  : prod == EKA_YM  ? "YM"                                 \
  : prod == EKA_EMD ? "EMD"                                \
  : prod == EKA_RTY ? "RTY"                                \
  : prod == EKA_NIY ? "NIY"                                \
  : prod == EKA_6J  ? "6J"                                 \
  : prod == EKA_6E  ? "6E"                                 \
  : prod == EKA_6B  ? "6B"                                 \
  : prod == EKA_ZN  ? "ZN"                                 \
  : prod == EKA_TN  ? "TN"                                 \
  : prod == EKA_ZF  ? "ZF"                                 \
  : prod == EKA_ZT  ? "ZT"                                 \
  : prod == EKA_ZB  ? "ZB"                                 \
  : prod == EKA_UB  ? "UB"                                 \
  : prod == EKA_CL  ? "CL"                                 \
  : prod == EKA_NG  ? "NG"                                 \
  : prod == EKA_ZC  ? "ZC"                                 \
  : prod == EKA_ZW  ? "ZW"                                 \
  : prod == EKA_ZS  ? "ZS"                                 \
  : prod == EKA_ZM  ? "ZM"                                 \
  : prod == EKA_ZL  ? "ZL"                                 \
  : prod == EKA_GC  ? "GC"                                 \
  : prod == EKA_HG  ? "HG"                                 \
  : prod == EKA_SI  ? "SI"                                 \
                    : "---"

#define EKA_RESOLVE_ICE_PROD(prod)                         \
  prod == EKA_FTSE   ? "FTSE"                              \
  : prod == EKA_GILT ? "GILT"                              \
                     : "---"

#define EKA_RESOLVE_NYSE_PROD(prod)                        \
  prod == EKA_NYSE1   ? "NYSE1"                            \
  : prod == EKA_NYSE2 ? "NYSE2"                            \
                      : "---"

#define EKA_RESOLVE_PROD(feed, prod)                       \
  feed == SN_EOBI   ? EKA_RESOLVE_EOBI_PROD(prod)          \
  : feed == SN_CME  ? EKA_RESOLVE_CME_PROD(prod)           \
  : feed == SN_ICE  ? EKA_RESOLVE_ICE_PROD(prod)           \
  : feed == SN_NYSE ? EKA_RESOLVE_NYSE_PROD(prod)          \
                    : "---"

class EkaEurProd {
public:
  using GlobalExchSecurityId =
      EkaExch::GlobalExchSecurityId;
  using ProdId = EkaExch::ProdId;

  EkaProd(EkaDev *pEkaDev, uint8_t hwFeedVer, uint idx,
          const init_product_t *init_product_conf) {
    dev = pEkaDev;

    prodIdx = idx;
    // SUBSCRIBE START
    exchange_security_id =
        init_product_conf->exchange_security_id;

    // PRODUCT PARAMS START (static fields, dynamic (sizes)
    // are updated from ARM
    max_book_spread = init_product_conf->max_book_spread;
    sock_fd = init_product_conf->sock_fd;
    product_id = init_product_conf->product_id;
    is_book = init_product_conf->is_book;
    midpoint = init_product_conf->midpoint;
    price_div = init_product_conf->price_div;

    ei_price_flavor = init_product_conf->ei_price_flavor;
    step = init_product_conf->step;

    ip = inet_addr(init_product_conf->ip_port.ip);
    port = init_product_conf->ip_port.port;

    mdCoreId = init_product_conf->eka_md_if;
    hwmidpoint = midpoint / step;

    hwstep = /*is_book ? step2hwstep(hwFeedVer,step) : */ 0;

    strategy_bitmap =
        0; // uknown during init, will be updated later
    ei_session = 0;
    ei_if = 0;
    buy_size = 0;
    sell_size = 0;
    ei_seq_num = 0;
    parent_product_id = eka_product_t::INVALID;

    bottom = midpoint * 0.8;
    top = midpoint * 1.2;

    if ((hwmidpoint * step != midpoint) && (is_book))
      on_error("Misconfiguration: step=%ju, midpoint=%ju, "
               "hwmidpoint=%u",
               step, midpoint, hwmidpoint);

    EKA_LOG(
        "%s (%d, mdCoreId=%u, isbook %d):  prodIndex=%u, "
        "%s:%d for exchange_secid %d step %ju price_flavor "
        "%d conf_midpoint %ju calc_midpoint %u",
        EKA_RESOLVE_PROD(hwFeedVer, product_id),
        init_product_conf->product_id, mdCoreId,
        init_product_conf->is_book, prodIdx,
        init_product_conf->ip_port.ip,
        init_product_conf->ip_port.port,
        init_product_conf->exchange_security_id,
        init_product_conf->step,
        init_product_conf->ei_price_flavor,
        init_product_conf->midpoint, hwmidpoint);
  }

  inline GlobalExchSecurityId getSecurityId() {
    return static_cast<GlobalExchSecurityId>(
        exchange_security_id);
  }

  inline ProdId getProdId() {
    return static_cast<ProdId>(product_id);
  }

  uint64_t getStep() { return step; }

  uint64_t getBottom() { return bottom; }

  uint prodIdx;
  uint32_t exchange_security_id;
  uint64_t max_book_spread;
  int sock_fd;

  uint64_t midpoint;
  uint64_t price_div;
  uint8_t ei_price_flavor;
  uint32_t hwmidpoint;
  eka_product_t product_id;
  eka_product_t parent_product_id;

  uint64_t step;
  uint64_t bottom;
  uint64_t top;
  uint64_t ei_seq_num;
  uint32_t ip;
  uint16_t port;

  uint64_t hwstep;
  uint8_t ei_if;
  uint8_t strategy_bitmap;
  uint8_t ei_session;
  uint64_t buy_size;
  uint64_t sell_size;
  uint8_t mdCoreId;
  bool is_book;

  /* --------------------------------- */
  uint32_t Csum4trigger = 0;

  uint8_t __attribute__((aligned(0x100)))
  cancelMsgBuf[2000] = {};
  uint8_t __attribute__((aligned(0x100)))
  newOrderBuf[2000] = {};

  uint32_t cancelMsgSize = 0;
  uint32_t cancelMsgChckSum = 0;
  EkaEpmAction *cancelAction = NULL;

  uint32_t newOrderSize = 0;
  uint32_t newOrderChckSum = 0;
  EkaEpmAction *newOrderAction = NULL;

private:
  EkaDev *dev;
};

#endif

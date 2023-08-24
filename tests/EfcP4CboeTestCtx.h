#ifndef __EFC_P4_CBOE_TEST_CTX_H__
#define __EFC_P4_CBOE_TEST_CTX_H__

#include "Efc.h"
#include "EkaFhBatsParser.h"
#include "EkaFhTypes.h"

#include "EfcStratTestCtx.h"
using namespace Bats;

struct P4SecurityCtx {
  char id[8];
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t size;
};

struct CboePitchAddOrderShort {
  sequenced_unit_header hdr;
  add_order_short msg;
} __attribute__((packed));

struct CboePitchAddOrderLong {
  sequenced_unit_header hdr;
  add_order_long msg;
} __attribute__((packed));

struct CboePitchAddOrderExpanded {
  sequenced_unit_header hdr;
  add_order_expanded msg;
} __attribute__((packed));

static inline char cboeSide(SideT side) {
  return side == SideT::BID ? 'B' : 'S';
}

class EfcP4CboeTestCtx : public EfcStratTestCtx {
public:
  static const size_t MaxTestSecurities = 16;
  /* --------------------------------------------- */

  EfcP4CboeTestCtx() {
    for (auto i = 0; i < std::size(security); i++) {
      secList[i] = getBinSecId(&security[i]);
    }
    nSec = std::size(security);
    EKA_LOG("Created List of %ju P4 Securities:", nSec);
    for (auto i = 0; i < nSec; i++)
      EKA_LOG("\t%s, %ju %c",
              cboeSecIdString(security[i].id, 8).c_str(),
              secList[i], i == nSec - 1 ? '\n' : ',');
  }
  /* --------------------------------------------- */

  virtual ~EfcP4CboeTestCtx() = default;
  /* --------------------------------------------- */

  /* --------------------------------------------- */

  uint64_t getBinSecId(const P4SecurityCtx *sec) {
    return be64toh(*(uint64_t *)sec->id);
  }
  /* --------------------------------------------- */

  void getSecCtx(size_t secIdx, SecCtx *dst) {
    if (secIdx >= nSec)
      on_error("secIdx %ju >= nSec %ju", secIdx, nSec);

    auto sec = &security[secIdx];
    dst->bidMinPrice = security[secIdx].bidMinPrice / 100;
    dst->askMaxPrice = security[secIdx].askMaxPrice / 100;
    dst->bidSize = security[secIdx].size;
    dst->askSize = security[secIdx].size;
    dst->versionKey = secIdx; // TBD

    dst->lowerBytesOfSecId =
        (uint8_t)(getBinSecId(sec) & 0xFF);
  }
  /* --------------------------------------------- */
  size_t createOrderExpanded(char *dst, const char *id,
                             SideT side, uint64_t price,
                             uint32_t size,
                             bool expectedFire) {
    auto p =
        reinterpret_cast<CboePitchAddOrderExpanded *>(dst);

    p->hdr.length = sizeof(CboePitchAddOrderExpanded);
    p->hdr.count = 1;
    p->hdr.sequence = sequence_++;

    p->msg.header.length = sizeof(p->msg);
    p->msg.header.type = (uint8_t)MsgId::ADD_ORDER_EXPANDED;
    p->msg.header.time = 0x11223344; // just a number

    p->msg.order_id = 0xaabbccddeeff5566;
    p->msg.side = cboeSide(side);
    p->msg.size = size;

    char dstSymb[8] = {id[2], id[3], id[4], id[5],
                       id[6], id[7], ' ',   ' '};
    memcpy(p->msg.exp_symbol, dstSymb, 8);

    p->msg.price = price;

    p->msg.customer_indicator = 'C';

    EKA_LOG("%s %s s=%c, P=%ju, S=%u, c=%c",
            EKA_BATS_PITCH_MSG_DECODE(p->msg.header.type),
            std::string(p->msg.exp_symbol,
                        sizeof(p->msg.exp_symbol))
                .c_str(),
            p->msg.side, p->msg.price, p->msg.size,
            p->msg.customer_indicator);
    return sizeof(CboePitchAddOrderExpanded);
  }
  /* --------------------------------------------- */

  size_t createOrderShort(char *dst, size_t secIdx,
                          SideT side, bool expectedFire) {
    auto sec = &security[secIdx];

    auto p =
        reinterpret_cast<CboePitchAddOrderShort *>(dst);

    p->hdr.length = sizeof(CboePitchAddOrderShort);
    p->hdr.count = 1;
    p->hdr.sequence = sequence_++;

    p->msg.header.length = sizeof(p->msg);
    p->msg.header.type = (uint8_t)MsgId::ADD_ORDER_SHORT;
    p->msg.header.time = 0x11223344; // just a number

    p->msg.order_id = 0xaabbccddeeff5566;
    p->msg.side = cboeSide(side);
    p->msg.size = sec->size;

    char dstSymb[6] = {sec->id[2], sec->id[3], sec->id[4],
                       sec->id[5], sec->id[6], sec->id[7]};
    memcpy(p->msg.symbol, dstSymb, 8);

    p->msg.price = sec->askMaxPrice / 100 - 1;

    return sizeof(CboePitchAddOrderShort);
  }
  /* --------------------------------------------- */
  size_t createOrderLong(char *dst, size_t secIdx,
                         SideT side, bool expectedFire) {
    auto sec = &security[secIdx];

    auto p = reinterpret_cast<CboePitchAddOrderLong *>(dst);

    p->hdr.length = sizeof(CboePitchAddOrderLong);
    p->hdr.count = 1;
    p->hdr.sequence = sequence_++;

    p->msg.header.length = sizeof(p->msg);
    p->msg.header.type = (uint8_t)MsgId::ADD_ORDER_LONG;
    p->msg.header.time = 0x11223344; // just a number

    p->msg.order_id = 0xaabbccddeeff5566;
    p->msg.side = cboeSide(side);
    p->msg.size = sec->size;

    char dstSymb[6] = {sec->id[2], sec->id[3], sec->id[4],
                       sec->id[5], sec->id[6], sec->id[7]};
    memcpy(p->msg.symbol, dstSymb, 8);

    p->msg.price = sec->askMaxPrice / 100 - 1;

    return sizeof(CboePitchAddOrderLong);
  }
  /* --------------------------------------------- */

public:
  size_t nSec = 0;
  uint64_t secList[MaxTestSecurities] = {};

  uint32_t sequence_ = 100; // just a number

  const P4SecurityCtx security[5] = {
      {{'\0', '\0', '0', '0', 'T', 'E', 'S', 'T'},
       1000,
       2000,
       1}, // correct SecID
      {{'\0', '\0', '0', '1', 'T', 'E', 'S', 'T'},
       3000,
       4000,
       1}, // correct SecID
      {{'\0', '\0', '0', '2', 'T', 'E', 'S', 'T'},
       5000,
       6000,
       1}, // correct SecID
      {{'\0', '\0', '0', '3', 'T', 'E', 'S', 'T'},
       7000,
       8000,
       1}, // correct SecID
      {{'\0', '\0', '0', '2', 'A', 'B', 'C', 'D'},
       7000,
       8000,
       1}, // correct SecID
  };
};

#if 0
sendAddOrder(AddOrder::Short, t->udpParams_[0]->udpSock,
             &t->udpParams_[0]->mcDst, p4Ctx.at(2).id,
             sequence++, 'S',
             p4Ctx.at(2).askMaxPrice / 100 - 1,
             p4Ctx.at(2).size);

sleep(1);
efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 0
sendAddOrder(AddOrder::Short, t->udpParams_[0]->udpSock,
             &t->udpParams_[0]->mcDst, p4Ctx.at(2).id,
             sequence++, 'B',
             p4Ctx.at(2).bidMinPrice / 100 + 1,
             p4Ctx.at(2).size);

sleep(1);
efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 0
sendAddOrder(AddOrder::Long, t->udpParams_[0]->udpSock,
             &t->udpParams_[0]->mcDst, p4Ctx.at(2).id,
             sequence++, 'S', p4Ctx.at(2).askMaxPrice - 1,
             p4Ctx.at(2).size);

sleep(1);
efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 0
sendAddOrder(AddOrder::Long, t->udpParams_[0]->udpSock,
             &t->udpParams_[0]->mcDst, p4Ctx.at(2).id,
             sequence++, 'B', p4Ctx.at(2).bidMinPrice + 1,
             p4Ctx.at(2).size);

sleep(1);
efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 0
sendAddOrder(AddOrder::Expanded, t->udpParams_[0]->udpSock,
             &t->udpParams_[0]->mcDst, p4Ctx.at(2).id,
             sequence++, 'S', p4Ctx.at(2).askMaxPrice - 1,
             p4Ctx.at(2).size);

sleep(1);
efcArmP4(pEfcCtx, p4ArmVer++);
#endif

#endif

#include "EkaMoexProdPair.h"
#include "eka_hw_conf.h"

#include "EkaDev.h"

extern EkaDev *g_ekaDev;

using namespace EkaBcs;

EkaMoexProdPair::EkaMoexProdPair(
    PairIdx idx, const ProdPairInitParams *params) {
  idx_ = idx;

  secA_ = params->secA;
  secB_ = params->secB;

  fireActionIdx_ = params->fireActionIdx;
}
/* --------------------------------------------------- */

OpResult EkaMoexProdPair::downloadStaticParams() {
  struct HwStruct {
    uint32_t reservedA;
    char nameA[12];
    uint32_t reservedB;
    char nameB[12];
  } __attribute__((packed));

  HwStruct __attribute__((aligned(32))) hw = {};

  secA_.getName(hw.nameA);
  secB_.getName(hw.nameB);

  const uint32_t BaseDstAddr = 0x86000;

  copyBuf2Hw(g_ekaDev, BaseDstAddr,
             reinterpret_cast<uint64_t *>(&hw), sizeof(hw));

  return OPRESULT__OK;
}
/* ---------------------------------------------------
 */

OpResult EkaMoexProdPair::setDynamicParams(
    const ProdPairDynamicParams *params) {
  return OPRESULT__OK;
}
/* --------------------------------------------------- */

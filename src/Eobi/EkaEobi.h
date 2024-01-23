#ifndef _EKA_EOBI_H_
#define _EKA_EOBI_H_

#include "EkaBcs.h"
#include "EpmIlink3NewOrderTemplate.h" //PATCH
#include "eka_macros.h"

class EkaEobi : public EkaExch {
public:
  typedef int32_t ProductId;
  using ExchSecurityId = EkaBcSecId;
  typedef int64_t Price;
  typedef int32_t Size;
  typedef uint16_t NormPrice;

  struct EobiTobParams {
    uint64_t last_transacttime;
    uint64_t fireBetterPrice;
    uint64_t firePrice;
    uint64_t price;
    uint16_t normprice;
    uint32_t size;
    uint32_t msg_seq_num;
  } __attribute__((packed));

  struct EobiDepthEntryParams {
    uint64_t price;
    uint32_t size;
  } __attribute__((packed));

  typedef EobiDepthEntryParams
      depth_entry_params_array[HW_DEPTH_PRICES];

  struct EobiDepthParams {
    depth_entry_params_array entry;
  } __attribute__((packed));

  struct EobiHwBookParams {
    EobiTobParams tob;
    EobiDepthParams depth;
  } __attribute__((packed));

  /* -----------------------------------------------------
   */
  EkaEobi(EkaDev *_dev /* , uint8_t _coreId */);
  /* -----------------------------------------------------
   */
  virtual ~EkaEobi(){};
  /* -----------------------------------------------------
   */
  int createParser(EkaEobi *exch);
  /* -----------------------------------------------------
   */
  //  int initBook(EkaDev* dev,uint8_t  coreId, EkaProd*
  //  prod);
  /* -----------------------------------------------------
   */
  EkaParser *newParser(EkaStrat *strat);
  /* -----------------------------------------------------
   */
  EkaBook *newBook(uint8_t coreId, EkaStrat *strat,
                   EkaProd *prod);
  /* -----------------------------------------------------
   */
  int ekaDownloadProductParams(eka_product_t product_id);
  /* -----------------------------------------------------
   */
  int jumpParamsApi2Internal(const jump_params_t *apiParams,
                             volatile void *dst);
  /* -----------------------------------------------------
   */
  int rjumpParamsApi2Internal(
      const reference_jump_params_t *apiParams,
      volatile void *dst);
  /* -----------------------------------------------------
   */
  int sendDate2Hw(EkaDev *dev);
  /* -----------------------------------------------------
   */
  uint32_t getFireReportSeq(uint32_t seq) { return seq; }

  /* -----------------------------------------------------
   */
  uint16_t getHwProductParamEntry() {
    return sizeof(hw_product_param_entry_t);
  }
  /* -----------------------------------------------------
   */
  uint16_t getHwRawFireSize() { return 120; }
  /* -----------------------------------------------------
   */
  uint16_t getFireParamsSize() {
    return sizeof(ilink_fire_params_t);
  }

  /* -----------------------------------------------------
   */
  //  virtual uint16_t getHwRawFireSize();
  /* -----------------------------------------------------
   */
  //  virtual  uint16_t getFireParamsSize();

  /* -----------------------------------------------------
   */
  int onTobChange(MdOut *mdOut, EkaBook *b, SIDE side);
  /* -----------------------------------------------------
   */
  int createCancelMsg(uint8_t *dst, void *params);
  /* -----------------------------------------------------
   */
  int updateCancelMsg(uint prodIdx, uint8_t *dst,
                      void *params);
  /* -----------------------------------------------------
   */
  EpmTemplate *addCancelMsgTemplate(uint templateId);
  /* -----------------------------------------------------
   */
  EpmTemplate *
  addNewOrderMsgTemplate(uint templateId); // PATCH
  /* -----------------------------------------------------
   */

  uint16_t getHwBookParamsSize() {
    return sizeof(EobiHwBookParams);
  }
};

#endif

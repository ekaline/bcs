#ifndef _EKA_EOBI_H_
#define _EKA_EOBI_H_

#include "eka_macros.h"
#include "EkaExch.h"
#include "eka_bc.h"
#include "EpmIlink3NewOrderTemplate.h" //PATCH

class EkaEobi : public EkaExch {
 public:
  typedef int32_t ProductId;
  typedef uint32_t ExchSecurityId;
  typedef int64_t Price;
  typedef int32_t Size;
  typedef uint16_t NormPrice;


 struct EobiTobParams { // NOT used in eurex, in eurex different struct
#define EobiTobParams__ITER( _x)			\
   _x(uint64_t, last_transacttime)		\
   _x(uint64_t, fireBetterPrice)	\
   _x(uint64_t, firePrice)		\
   _x(uint64_t, price)				\
   _x(uint16_t, normprice)			\
   _x(uint32_t, size)				\
   _x(uint32_t, msg_seq_num)
   EobiTobParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));

 struct EobiDepthEntryParams {
#define EobiDepthEntryParams__ITER( _x)		\
   _x(uint64_t, price)				\
   _x(uint32_t, size)
   EobiDepthEntryParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));

 typedef EobiDepthEntryParams depth_entry_params_array[HW_DEPTH_PRICES];

 struct EobiDepthParams {
#define EobiDepthParams__ITER( _x)		\
   _x(depth_entry_params_array, entry)
   EobiDepthParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));


 struct EobiHwBookParams {
#define EobiHwBookParams__ITER( _x)		\
   _x(EobiTobParams,   tob)			\
   _x(EobiDepthParams, depth)
   EobiHwBookParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));


  /* ----------------------------------------------------- */
  EkaEobi(EkaDev* _dev/* , uint8_t _coreId */);
  /* ----------------------------------------------------- */
  virtual ~EkaEobi() {};
  /* ----------------------------------------------------- */
  int createParser(EkaEobi* exch);
  /* ----------------------------------------------------- */
  //  int initBook(EkaDev* dev,uint8_t  coreId, EkaProd* prod);
  /* ----------------------------------------------------- */
  EkaParser* newParser(EkaStrat* strat);
  /* ----------------------------------------------------- */
  EkaBook* newBook(uint8_t coreId,EkaStrat* strat,EkaProd* prod);
  /* ----------------------------------------------------- */
  int ekaDownloadProductParams(eka_product_t product_id);
  /* ----------------------------------------------------- */
  int jumpParamsApi2Internal(const jump_params_t* apiParams,volatile void* dst);
  /* ----------------------------------------------------- */
  int rjumpParamsApi2Internal(const reference_jump_params_t* apiParams,volatile void* dst);
  /* ----------------------------------------------------- */
  int sendDate2Hw(EkaDev* dev);
  /* ----------------------------------------------------- */
  uint32_t getFireReportSeq(uint32_t seq) {
    return seq;
  }

  /* ----------------------------------------------------- */
  uint16_t  getHwProductParamEntry() {
    return sizeof(hw_product_param_entry_t);
  }
  /* ----------------------------------------------------- */
  uint16_t getHwRawFireSize() {
    return 120;
  }
  /* ----------------------------------------------------- */
  uint16_t getFireParamsSize() {
    return sizeof(ilink_fire_params_t);
  }

  /* ----------------------------------------------------- */
  //  virtual uint16_t getHwRawFireSize();
  /* ----------------------------------------------------- */
  //  virtual  uint16_t getFireParamsSize();

  /* ----------------------------------------------------- */
  int onTobChange(MdOut* mdOut, EkaBook* b, SIDE side);
  /* ----------------------------------------------------- */
  int createCancelMsg(uint8_t* dst, void* params);
  /* ----------------------------------------------------- */
  int updateCancelMsg(uint prodIdx, uint8_t* dst, void* params);
  /* ----------------------------------------------------- */
  EpmTemplate* addCancelMsgTemplate(uint templateId);
  /* ----------------------------------------------------- */
  EpmTemplate* addNewOrderMsgTemplate(uint templateId); //PATCH
  /* ----------------------------------------------------- */

  uint16_t getHwBookParamsSize() {
    return sizeof(EobiHwBookParams);
  }

};


#endif

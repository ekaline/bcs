#ifndef _EKA_NYSE_H_
#define _EKA_NYSE_H_

#include "eka_macros.h"
#include "EkaExch.h"
#include "eka_bc.h"

class EkaNyse : public EkaExch {
 public:
  typedef uint32_t ProductId;
  typedef uint32_t ExchSecurityId;
  typedef uint64_t Price;
  typedef uint32_t Size;
  typedef uint16_t NormPrice;

  typedef uint8_t  PriceLevelIdx;
  typedef uint32_t OrderID;

  typedef uint32_t CntT;


 struct NyseTobParams { // NOT used in eurex, in eurex different struct
#define NyseTobParams__ITER( _x)			\
   _x(uint64_t, last_transacttime)		\
   _x(uint64_t, fireBetterPrice)	\
   _x(uint64_t, firePrice)		\
   _x(uint64_t, price)				\
   _x(uint16_t, normprice)			\
   _x(uint32_t, size)				\
   _x(uint32_t, msg_seq_num)
   NyseTobParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));

 struct NyseDepthEntryParams {
#define NyseDepthEntryParams__ITER( _x)		\
   _x(uint64_t, price)				\
   _x(uint32_t, size)
   NyseDepthEntryParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));

 typedef NyseDepthEntryParams depth_entry_params_array[HW_DEPTH_PRICES];

 struct NyseDepthParams {
#define NyseDepthParams__ITER( _x)		\
   _x(depth_entry_params_array, entry)
   NyseDepthParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));


 struct NyseHwBookParams {
#define NyseHwBookParams__ITER( _x)		\
   _x(NyseTobParams,   tob)			\
   _x(NyseDepthParams, depth)
   NyseHwBookParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));


  /* ----------------------------------------------------- */
  EkaNyse(EkaDev* _dev/* , uint8_t _coreId */);
  /* ----------------------------------------------------- */
  virtual ~EkaNyse() {};
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
  uint16_t  getHwProductParamEntry() {
    return sizeof(hw_product_param_entry_t);
  }
  /* ----------------------------------------------------- */
  uint16_t getHwRawFireSize() { //UPDATEME
    return 160;
  }
  /* ----------------------------------------------------- */
  uint16_t getFireParamsSize() {
    return sizeof(nyse_fire_params_t); //UPDATEME
  }
  /* ----------------------------------------------------- */
  int onTobChange(MdOut* mdOut, EkaBook* b, SIDE side);
  /* ----------------------------------------------------- */

  /* ----------------------------------------------------- */
  int createCancelMsg(uint8_t* dst, void* params);
  /* ----------------------------------------------------- */
  int createNewOrder(uint8_t* dst, void* params);
  /* ----------------------------------------------------- */
  int updateCancelMsg(uint prodIdx, uint8_t* dst, void* params);
  /* ----------------------------------------------------- */
  int updateNewOrder(uint prodIdx, uint8_t* dst, void* params);
  /* ----------------------------------------------------- */
  uint16_t getHwBookParamsSize() {
    return sizeof(NyseHwBookParams);
  }
  /* ----------------------------------------------------- */
  EpmTemplate* addCancelMsgTemplate(uint templateId);
  /* ----------------------------------------------------- */
  EpmTemplate* addNewOrderMsgTemplate(uint templateId);
  /* ----------------------------------------------------- */

};


#endif

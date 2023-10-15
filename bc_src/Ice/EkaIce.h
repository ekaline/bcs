#ifndef _EKA_ICE_H_
#define _EKA_ICE_H_

#include "eka_macros.h"
#include "EkaExch.h"
class EkaProd;

class EkaIce : public EkaExch {
 public:
  typedef uint32_t ProductId;
  typedef uint32_t ExchSecurityId;
  typedef uint64_t Price;
  typedef uint32_t Size;
  typedef uint16_t NormPrice;

  typedef uint64_t OrderId;
  typedef uint8_t  PriceLevelIdx;

  typedef uint32_t CntT;


typedef char EkaTobPriceAscii[8];

 struct IceTobParams { // NOT used in eurex, in eurex different struct
#define IceTobParams__ITER( _x)			\
   _x(uint64_t, last_transacttime)		\
   _x(EkaTobPriceAscii, fireBetterPrice)	\
   _x(EkaTobPriceAscii, firePrice)		\
   _x(uint64_t, price)				\
   _x(uint16_t, normprice)			\
   _x(uint32_t, size)				\
   _x(uint32_t, msg_seq_num)
   IceTobParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));

 struct IceDepthEntryParams {
#define IceDepthEntryParams__ITER( _x)		\
   _x(uint64_t, price)				\
   _x(uint32_t, size)
   IceDepthEntryParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));

 typedef IceDepthEntryParams depth_entry_params_array[HW_DEPTH_PRICES];

 struct IceDepthParams {
#define IceDepthParams__ITER( _x)		\
   _x(depth_entry_params_array, entry)
   IceDepthParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));


 struct IceHwBookParams {
#define IceHwBookParams__ITER( _x)		\
   _x(IceTobParams,   tob)			\
   _x(IceDepthParams, depth)
   IceHwBookParams__ITER( EKA__FIELD_DEF )
 } __attribute__((packed));


  /* ----------------------------------------------------- */
  EkaIce(EkaDev* _dev/* , uint8_t _coreId */);
  /* ----------------------------------------------------- */  
  virtual ~EkaIce(){};
  /* ----------------------------------------------------- */  
  //  int initBook(EkaDev* dev, uint8_t coreId, EkaProd* prod);
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

  uint16_t getHwRawFireSize() {
    return 316;
  }
  /* ----------------------------------------------------- */
  uint16_t getFireParamsSize() {
    return sizeof(ice_fire_params);
  }
  /* ----------------------------------------------------- */
  uint16_t getHwBookParamsSize() {
    return sizeof(IceHwBookParams);
  }
  /* ----------------------------------------------------- */
  int onTobChange(MdOut* mdOut, EkaBook* b, SIDE side);
  /* ----------------------------------------------------- */
  int createCancelMsg(uint8_t* dst, void* params) {
    return 0;
  }
  /* ----------------------------------------------------- */
  int updateCancelMsg(uint prodIdx, uint8_t* dst, void* params) {
    return 0;
  }
  /* ----------------------------------------------------- */

  uint16_t  getHwProductParamEntry() {
    return sizeof(hw_product_param_entry_t);
  }
};

#endif

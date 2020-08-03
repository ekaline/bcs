#ifndef _EKA_FH_BOOK_H
#define _EKA_FH_BOOK_H
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>

#include "eka_sn_addr_space.h"
#include "eka_hw_conf.h"
#include "eka_fh_q.h"
//#include "eka_fh_udp_channel.h"
#include "Efh.h"

//typedef enum {EKA_DEFINITIONS = 0, EKA_GAP_SNAPSHOT = 1, EKA_MCAST = 2} eka_fh_op_t;
enum class EkaFhMode : uint8_t {DEFINITIONS = 0, SNAPSHOT = 1, MCAST = 2, RECOVERY = 3};
enum EKA_FH_ERR_CODE {
  EKA_FH_RESULT__OK = 0,
  EKA_FH_RESULT__NEG_SIZE = -1,
  EKA_FH_RESULT__O_SIZE_GREATER_THAN_TOTAL = -2
};


class fh_b_security;
class fh_book;
class fh_b_plevel;

class fh_b_order {
 public:
  typedef enum {ORDER = 1, QUOTE = 2} type_t;
  fh_b_order();
  EKA_FH_ERR_CODE deduct_size(uint32_t size);

  uint64_t            order_id;
  type_t              type;
  uint32_t	      size;
  fh_b_plevel*        plevel;
  fh_b_order*         next;

 private:
};

class fh_b_plevel {
 public:
  typedef enum {INIT_PRICE = 0, BEST_BUY = 1, BEST_SELL = 2, BUY = 3, SELL = 4} price_level_state_t;
  fh_b_plevel();
  uint32_t add_order(uint32_t size, fh_b_order::type_t type);
  EKA_FH_ERR_CODE delete_order_from_plevel(uint32_t ord_size, fh_b_order::type_t type);
  bool is_empty();
  void reset();

  price_level_state_t state;
  uint32_t	      size;
  uint32_t	      o_size;
  uint32_t            cnt; // amount of orders for this plevel
  uint32_t	      price;
  fh_b_plevel*        prev;
  fh_b_plevel*        next;
  fh_b_security*      s; // pointer to the security
 private:
};


class fh_b_security_state {
 public:
  void          reset();
  void          set(fh_b_security* s);
  bool          is_equal(fh_b_security* s);

  uint64_t      efhUserData;
  EfhTradeStatus trading_action;
  bool          option_open;

  uint64_t      timestamp;

  uint32_t      security_id;
  uint16_t      num_of_buy_plevels;
  uint16_t      num_of_sell_plevels;
  uint32_t	best_buy_price;
  uint32_t	best_sell_price;
  uint32_t	best_buy_size;
  uint32_t	best_sell_size;
  uint32_t	best_buy_o_size;
  uint32_t	best_sell_o_size;
};

class fh_b_security {
 public:
  fh_b_security(uint32_t secid, uint8_t secType, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB);

  fh_b_plevel*   buy;
  fh_b_plevel*   sell;
  uint32_t       security_id;
  fh_b_security* next;
  EfhTradeStatus trading_action;
  bool		 option_open;

  uint8_t       type; // EfhSecTypeIndx, EfhSecTypeMleg, EfhSecTypeOpt, EfhSecTypeFut, EfhSecTypeCs
  uint64_t      efhUserData; // to be returned per TOB update

  uint16_t      num_of_buy_plevels;
  uint16_t      num_of_sell_plevels;

  uint64_t      bid_ts;
  uint64_t      ask_ts;

  /* uint32_t      seconds; */
  /* uint32_t      nanoseconds; */


  // For TOB feeds only
  uint32_t	bid_size;
  uint32_t	bid_o_size;
  uint32_t	bid_cust_size;
  uint32_t	bid_pro_cust_size;
  uint32_t	bid_bd_size;

  uint32_t	bid_price;
  
  uint32_t	ask_size;
  uint32_t	ask_o_size;
  uint32_t	ask_cust_size;
  uint32_t	ask_pro_cust_size;
  uint32_t	ask_bd_size;

  uint32_t	ask_price;

  uint          underlyingIdx = (uint)-1;

 private:
};

class fh_b_security64 {
 public:
  fh_b_security64(uint64_t secid, uint8_t secType, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB);

  fh_b_plevel*   buy;
  fh_b_plevel*   sell;
  uint64_t       security_id;
  fh_b_security64* next;
  EfhTradeStatus trading_action;
  bool		 option_open;

  uint8_t       type; // EfhSecTypeIndx, EfhSecTypeMleg, EfhSecTypeOpt, EfhSecTypeFut, EfhSecTypeCs
  uint64_t      efhUserData; // to be returned per TOB update

  uint16_t      num_of_buy_plevels;
  uint16_t      num_of_sell_plevels;

  uint64_t      bid_ts;
  uint64_t      ask_ts;

  /* uint32_t      seconds; */
  /* uint32_t      nanoseconds; */


  // For TOB feeds only
  uint32_t	bid_size;
  uint32_t	bid_o_size;
  uint32_t	bid_cust_size;
  uint32_t	bid_pro_cust_size;
  uint32_t	bid_bd_size;

  uint32_t	bid_price;
  
  uint32_t	ask_size;
  uint32_t	ask_o_size;
  uint32_t	ask_cust_size;
  uint32_t	ask_pro_cust_size;
  uint32_t	ask_bd_size;

  uint32_t	ask_price;

  uint          underlyingIdx = (uint)-1;

 private:
};

class EkaDev;
 /* ##################################################################### */

class Underlying {
 public:
  Underlying(char* _name,size_t size) {
    memset(name,0,sizeof(name));
    memcpy(name,_name,size);
    tradeStatus = EfhTradeStatus::kNormal;
  }
  
  EfhSymbol name = {};
  EfhTradeStatus tradeStatus = EfhTradeStatus::kNormal;
};

 /* ##################################################################### */

class FhGroup;

class fh_book {
 public:
  static const uint UndelyingsPerGroup = 2048;
  
  
  fh_book (EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx,FhGroup* gr);
  virtual int     init() = 0;

  fh_b_security*  find_security(uint32_t security_id);
  fh_b_security64*  find_security64(uint64_t security_id);
  fh_b_security*  subscribe_security (uint32_t secid, uint8_t type, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB);
  fh_b_security64*  subscribe_security64 (uint64_t secid, uint8_t type, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB);
  virtual int generateOnQuote(const EfhRunCtx* pEfhRunCtx, fh_b_security* s, uint64_t sequence, uint64_t timestamp,uint gapNum) = 0;
  virtual int generateOnQuote64(const EfhRunCtx* pEfhRunCtx, fh_b_security64* s, uint64_t sequence, uint64_t timestamp,uint gapNum) = 0;

  inline int findUnderlying(char* name, size_t size) {
    for (int i = 0; i < (int)underlyingNum; i ++) {
      if (memcmp(underlying[i]->name,name,size) == 0) return i;
    }
    return -1;
  }

  inline uint addUnderlying(char* name, size_t size) {
    int u = findUnderlying(name,size);
    if (u >= 0) return (uint)u;

    if (underlyingNum == UndelyingsPerGroup) 
      on_error("cannot add %s to gr %u: underlyingNum=%u",name,id,underlyingNum);

    uint newUnderlying = underlyingNum++;

    underlying[newUnderlying] = new Underlying(name,size);
    if (underlying[newUnderlying] == NULL) on_error("cannot create new Underlying %s",name);

    return newUnderlying;
  }

  //----------------------------------------------------------

  //----------------------------------------------------------
  uint8_t               id; // MC group ID: 1-4 for ITTO
  bool                  subscribe_all;


  FILE*                 parser_log; // used with PRINT_PARSED_MESSAGES define

  EfhFeedVer            feed_ver;
  EkaSource             exch;
  FhGroup*              gr; 

  uint32_t              total_securities;

  EkaDev*               dev;

  fh_b_security*        sec[EKA_FH_SEC_HASH_LINES]; // array of pointers to the securities
  fh_b_security64*      sec64[EKA_FH_SEC_HASH_LINES]; // array of pointers to the securities

  Underlying*           underlying[UndelyingsPerGroup] = {};
  uint                  underlyingNum = 0;

 private:
  static const uint64_t MAX_ORDERS = 0;

};
 /* ##################################################################### */

class TobBook : public fh_book {
 public:
  static const bool full_book = false;
  
  int                     init();
 TobBook(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx,FhGroup* gr) : fh_book(pEfhCtx,pInitCtx,gr){};
  int  generateOnQuote(const EfhRunCtx* pEfhRunCtx, fh_b_security* s, uint64_t sequence, uint64_t timestamp,uint gapNum);
  void sendTobImage (const EfhRunCtx* pEfhRunCtx);

  int generateOnQuote64(const EfhRunCtx* pEfhRunCtx, fh_b_security64* s, uint64_t sequence, uint64_t timestamp,uint gapNum);

};

class FullBook : public fh_book {
 public:
  static const bool full_book = true;


  FullBook(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx,FhGroup* gr);

  int generateOnQuote(const EfhRunCtx* pEfhRunCtx, fh_b_security* s, uint64_t sequence, uint64_t timestamp,uint gapNum);

  virtual  int            init() = 0;


  void                    init_plevels_and_orders ();
  fh_b_plevel*            add_order2book (fh_b_security* s,uint64_t order_id,fh_b_order::type_t t,uint32_t price,uint32_t size,char side);
  fh_b_order*             find_order(uint64_t order_id);
  EKA_FH_ERR_CODE         replace_order (fh_b_order* o, uint64_t new_order_id,int32_t price,uint32_t size);
  EKA_FH_ERR_CODE         modify_order (fh_b_order* o, uint32_t price,uint32_t size);
  EKA_FH_ERR_CODE         delete_order (fh_b_order* o);
  EKA_FH_ERR_CODE         change_order_size (fh_b_order* o,uint32_t size,int incr);
  int generateOnQuote64(const EfhRunCtx* pEfhRunCtx, fh_b_security64* s, uint64_t sequence, uint64_t timestamp,uint gapNum);

  void                    invalidate();

  //----------------------------------------------------------------------


  fh_b_order*           free_o_head; // head pointer to free list of preallocated fh_b_order elements
  fh_b_plevel*          free_p_head; // head pointer to free list of preallocated fh_b_plevel elements
  uint32_t              orders_cnt;  // counter of currently used orders (for statistics only)
  uint32_t              plevels_cnt; // counter of currently used plevelss (for statistics only)
  uint32_t              max_orders_cnt;  // highest value of orders_cnt ever reached
  uint32_t              max_plevels_cnt; // highest value of plevels_cnt ever reached


 private:

  fh_b_plevel*            get_new_plevel_element();
  void                    release_plevel_element(fh_b_plevel* p);
  fh_b_order*             get_new_order_element();
  void                    release_order_element(fh_b_order* o);
  EKA_FH_ERR_CODE         delete_plevel(fh_b_plevel* p);
  void                    add_order2hash (fh_b_order* o);
  void                    delete_order_from_hash(uint64_t order_id);
  fh_b_plevel* find_and_update_buy_price_level4add  (fh_b_security* s, fh_b_order::type_t o_type, uint32_t price, uint32_t size);
  fh_b_plevel* find_and_update_sell_price_level4add (fh_b_security* s, fh_b_order::type_t o_type, uint32_t price, uint32_t size);

  fh_b_order*           ord[1]; // (overloaded in derived classes)

  virtual const uint64_t getMaxOrders() = 0;
  virtual const uint64_t getMaxPlevels() = 0;
  virtual const uint64_t getOrdersHashMask() = 0;

};

class NomBook : public FullBook {
 public:
  NomBook(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx,FhGroup* gr) : FullBook(pEfhCtx,pInitCtx,gr){};

  int init();

 private:
  static const uint     BOOK_SCALE  = 23; //
  static const uint64_t MAX_ORDERS  = (1 << BOOK_SCALE);       // 8M 
  static const uint64_t MAX_PLEVELS = (1 << (BOOK_SCALE - 1)); // 4M 
  static const uint64_t ORDERS_HASH_MASK = ((1 << (BOOK_SCALE - 3)) - 1); //0xFFFFF

  fh_b_order*           ord[MAX_ORDERS]; // array of pointers to the orders

  const uint64_t getMaxOrders()      { return MAX_ORDERS; }
  const uint64_t getMaxPlevels()     { return MAX_PLEVELS; }
  const uint64_t getOrdersHashMask() { return ORDERS_HASH_MASK; }
};

class BatsBook : public FullBook {
 public:
 BatsBook(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx,FhGroup* gr) : FullBook(pEfhCtx,pInitCtx,gr){};

  int init();
 private:
  static const uint     BOOK_SCALE  = 22; //
  static const uint64_t MAX_ORDERS  = (1 << BOOK_SCALE);       // 4 *256K 
  static const uint64_t MAX_PLEVELS = (1 << (BOOK_SCALE - 1)); // 4 * 128K
  static const uint64_t ORDERS_HASH_MASK = ((1 << (BOOK_SCALE - 3)) - 1); //0x7FFF

  fh_b_order*           ord[MAX_ORDERS]; // array of pointers to the orders

  const uint64_t getMaxOrders()      { return MAX_ORDERS; }
  const uint64_t getMaxPlevels()     { return MAX_PLEVELS; }
  const uint64_t getOrdersHashMask() { return ORDERS_HASH_MASK; }
};





#endif

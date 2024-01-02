#ifndef _EKA_FH_SECURITY_STATE_H_
#define _EKA_FH_SECURITY_STATE_H_

#include "EkaFhSecurity.h"

template <class PriceT, class SizeT>
  class EkaFhSecurityState {
 public:
/* --------------------------------------------------------------- */
  int reset() {
    secId                = -1;

    trading_action       = EfhTradeStatus::kUninit;
    option_open          = false;

    num_of_buy_plevels   = 0;
    num_of_sell_plevels  = 0;
    best_buy_price       = 0;
    best_sell_price      = 0;
    best_buy_size        = 0;
    best_sell_size       = 0;
    best_buy_o_size      = 0;
    best_sell_o_size     = 0;

    return 0;
  }
/* --------------------------------------------------------------- */

  int set (EkaFhSecurity* s) {
    secId               = s->secId;

    /* best_buy_price      = s->bid != NULL ? s->bid->price                     : 0; */
    /* best_sell_price     = s->ask != NULL ? s->ask->price                     : 0; */
    /* best_buy_size       = s->bid != NULL ? s->bid->get_total_size()          : 0; */
    /* best_sell_size      = s->ask != NULL ? s->ask->get_total_size()          : 0; */
    /* best_buy_o_size     = s->bid != NULL ? s->bid->get_total_customer_size() : 0; */
    /* best_sell_o_size    = s->ask != NULL ? s->ask->get_total_customer_size() : 0; */

    best_buy_price      = (PriceT) s->getTopPrice(SideT::BID);
    best_sell_price     = (PriceT) s->getTopPrice(SideT::ASK);

    best_buy_size       = (SizeT) s->getTopTotalSize(SideT::BID);
    best_sell_size      = (SizeT) s->getTopTotalSize(SideT::ASK);
    best_buy_o_size     = (SizeT) s->getTopTotalCustomerSize(SideT::BID);
    best_sell_o_size    = (SizeT) s->getTopTotalCustomerSize(SideT::ASK);

    trading_action      = s->trading_action;
    option_open         = s->option_open;

    return 0;
  }
/* --------------------------------------------------------------- */
  bool isEqual (EkaFhSecurity* s) {
    if (secId != s->secId)  
      on_error("secId(%u) != s->secId(%u)",secId,s->secId);

    if ((trading_action   != s->trading_action)                                        ||
	(option_open      != s->option_open)                                           ||
	/* (best_buy_price   != (s->bid != NULL ? s->bid->price                     : 0)) || */
	/* (best_sell_price  != (s->ask != NULL ? s->ask->price                     : 0)) || */
	/* (best_buy_size    != (s->bid != NULL ? s->bid->get_total_size()          : 0)) || */
	/* (best_sell_size   != (s->ask != NULL ? s->ask->get_total_size()          : 0)) || */
	/* (best_buy_o_size  != (s->bid != NULL ? s->bid->get_total_customer_size() : 0)) || */
	/* (best_sell_o_size != (s->ask != NULL ? s->ask->get_total_customer_size() : 0)) */

	(best_buy_price   != (PriceT) s->getTopPrice(SideT::BID)) ||
	(best_sell_price  != (PriceT) s->getTopPrice(SideT::ASK)) ||
	(best_buy_size    != (SizeT) s->getTopTotalSize(SideT::BID)) ||
	(best_sell_size   != (SizeT) s->getTopTotalSize(SideT::ASK)) ||
	(best_buy_o_size  != (SizeT) s->getTopTotalCustomerSize(SideT::BID)) ||
	(best_sell_o_size != (SizeT) s->getTopTotalCustomerSize(SideT::ASK))
	) return false;
    return true;
  }
/* --------------------------------------------------------------- */

 private:
  EfhTradeStatus trading_action = EfhTradeStatus::kUninit;
  bool           option_open    = false;

  uint32_t      secId               = 0;
  uint16_t      num_of_buy_plevels  = 0;
  uint16_t      num_of_sell_plevels = 0;
  PriceT	best_buy_price      = 0;
  PriceT	best_sell_price     = 0;
  SizeT  	best_buy_size       = 0;
  SizeT	        best_sell_size      = 0;
  SizeT	        best_buy_o_size     = 0;
  SizeT	        best_sell_o_size    = 0;

};

#endif

#include "EkaBc.h"

int main(int argc, char *argv[]) {

  struct EkaBcEurJumpParams eurJumpParams = {};

  /////////////// General
  uint8_t        activeJumpAtBestSet      = 2;
  uint8_t        activeJumpBetterBestSet  = 4;
  EkaBcEurMdSize sizeMultiplier       = 10000;
  /////////////// General

  /////////////// TOB  
  EkaBcEurMdSize rawTobBidSize  = 5; 
  EkaBcEurPrice  tobBidPrice    = 50000;
  
  EkaBcEurMdSize rawTobAskSize  = 8; 
  EkaBcEurPrice  tobBidPrice    = 80000;
  /////////////// TOB

  /////////////// Trade
  EkaBcEurMdSize rawTradeSize   = 2;
  EkaBcEurPrice  tradePrice     = 60000;
  /////////////// Trade
  
  /////////////// PRODUCT
  EkaBcEurProductInitParams productInitParams = {};
  productInitParams.maxBidSize    = sizeMultiplier; //TBD 
  productInitParams.maxAskSize    = sizeMultiplier*2;//TBD 
  productInitParams.maxBookSpread = tobBidPrice-tobBidPrice;
  /////////////// PRODUCT
  
  EkaBcEurMdSize tobSize = rawTobSize * sizeMultiplier;
  EkaBcEurMdSize tradeSize = rawTradeSize * sizeMultiplier;
  
  eurJumpParams.atBest[activeJumpAtBestSet].max_tob_size = tobSize + sizeMultiplier;
  eurJumpParams.atBest[activeJumpAtBestSet].min_tob_size = tobSize - sizeMultiplier;
  eurJumpParams.atBest[activeJumpAtBestSet].max_post_size = tobSize - tradeSize;
  eurJumpParams.atBest[activeJumpAtBestSet].min_ticker_size = tradeSize;
  eurJumpParams.atBest[activeJumpAtBestSet].min_price_delta = tradePrice - tobPrice;
  eurJumpParams.atBest[activeJumpAtBestSet].buy_size = sizeMultiplier;
  eurJumpParams.atBest[activeJumpAtBestSet].sell_size = sizeMultiplier*2;
  eurJumpParams.atBest[activeJumpAtBestSet].strat_en = 0;
  eurJumpParams.atBest[activeJumpAtBestSet].boc = 1;

  eurJumpParams.atBest[activeJumpAtBestSet].max_tob_size = tobSize + sizeMultiplier;
  eurJumpParams.atBest[activeJumpAtBestSet].min_tob_size = tobSize - sizeMultiplier;
  eurJumpParams.atBest[activeJumpAtBestSet].max_post_size = tobSize - tradeSize;
  eurJumpParams.atBest[activeJumpAtBestSet].min_ticker_size = tradeSize;
  eurJumpParams.atBest[activeJumpAtBestSet].min_price_delta = tradePrice - tobPrice;
  eurJumpParams.atBest[activeJumpAtBestSet].buy_size = sizeMultiplier;
  eurJumpParams.atBest[activeJumpAtBestSet].sell_size = sizeMultiplier*2;
  eurJumpParams.atBest[activeJumpAtBestSet].strat_en = 1;
  eurJumpParams.atBest[activeJumpAtBestSet].boc = 1;

  
  return 0;
}

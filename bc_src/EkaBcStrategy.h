#ifndef __EKA_BC_STRATEGY_H__
#define __EKA_BC_STRATEGY_H__

#include "EkaStrategyEhp.h"

// #include "EkaExch.h"

class EkaUdpChannel;
class EkaUdpSess;
class EkaBook;

template <class Ehp>
class EkaBcStrategy : public EkaStrategyEhp<Ehp> {
public:
  /*   using GlobalExchSecurityId =
        EkaExch::GlobalExchSecurityId;
    using ProdId = EkaExch::ProdId;

    enum CONF { MAX_PROD = 8, MAX_BOOKS = 4 }; */
  // static const uint MAX_BOOKS = 32;

  /* protected: */
  /*  EkaStrat(EkaExch* _exch); */
  // EkaStrat(EkaExch *_exch, EkaExch::StrategyType type);

public:
  /* void setParent(EkaExch* _exch) { */
  /*   exch = _exch; */
  /*   dev = exch->dev; */
  /* } */

#if 0
  uint addUdpSess(uint8_t coreId, uint32_t ip,
                  uint16_t port);
  EkaUdpSess *findUdpSess(uint32_t ip, uint16_t port);

  int initGlobalParams(const struct global_params *params);

  void addProd(const init_product_t *init_product_conf);

  uint findProd(eka_product_t product_id,
                char const *callingf);

  EkaProd *findProdSecId(uint32_t secId);

  uint findProdHw(eka_product_t product_id,
                  char const *callingf);

  int createBook(uint8_t coreId, EkaProd *_prod);

  EkaBook *findBook(GlobalExchSecurityId securityId);

  EkaExch *exch = NULL;
  EkaProd *prod[CONF::MAX_PROD] = {};

  uint prodNum = 0;

  struct global_params globalParams; // = fire_params
  bool auto_rearm = false;

  EkaUdpChannel *udpChannel[EkaDev::CONF::MAX_CORES] = {};

  EkaUdpSess *udpSess[EKA_MAX_UDP_SESSIONS_PER_CORE] = {};
  uint8_t udpSessions = 0;

  //  std::thread loopThread[EkaDev::CONF::MAX_CORES];
  std::thread loopThread;
  volatile bool threadActive = false;

  uint8_t hwFeedVer = 0;

  EkaParser *parser = NULL;

  EkaBook *book[MAX_BOOKS] = {};
  uint numBooks = 0;

  EkaExch::StrategyType type;

  hw_jump_strategy_params_t __attribute__((aligned(0x8)))
  hw_jump_params_shadow[MAX_BOOKS] = {};
  hw_rjump_strategy_params_t __attribute__((aligned(0x8)))
  hw_rjump_params_shadow[MAX_PROD] = {};

  hw_product_param_entry_t __attribute__((aligned(0x8)))
  hw_product_param_shadow[MAX_BOOKS] = {};
  hw_session_param_entry_t __attribute__((aligned(0x8)))
  hw_session_param_shadow[MAX_BOOKS * 2] = {}; // per i/f
#endif

  EkaDev *dev = NULL;
};
#endif

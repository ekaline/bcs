#ifndef _EKA_FH_PCAP_NOM_GR_H_
#define _EKA_FH_PCAP_NOM_GR_H_

#include <stdarg.h>

#include "EkaFhFullBook.h"
#include "EkaFhNomParser.h"

enum class EkaFhMode : uint8_t {
  UNINIT = 0,
    DEFINITIONS,
    SNAPSHOT,
    MCAST,
    RECOVERY};

#define EkaFhMode2STR(x) \
  x == EkaFhMode::UNINIT ? "UNINIT" :			\
    x == EkaFhMode::DEFINITIONS ? "DEFINITIONS" :	\
    x == EkaFhMode::SNAPSHOT ? "SNAPSHOT" :		\
    x == EkaFhMode::RECOVERY ? "RECOVERY" :		\
    x == EkaFhMode::MCAST ? "MCAST" :			\
    "UNEXPECTED"



class EkaFhNomGr {
public:
  static const uint   SCALE          = (const uint) 24;
  static const uint   SEC_HASH_SCALE = 19;

  using SecurityIdT = uint32_t;
  using OrderIdT    = uint64_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhPlevel    = EkaFhPlevel<PriceT, SizeT>;
  using FhSecurity  = EkaFhFbSecurity<FhPlevel,SecurityIdT,OrderIdT,PriceT,SizeT>;
  using FhOrder     = EkaFhOrder<FhPlevel,OrderIdT,SizeT>;

  using FhBook      = EkaFhFullBook<
    SCALE,SEC_HASH_SCALE,
    FhSecurity,
    FhPlevel,
    FhOrder,
    EkaFhNoopQuotePostProc,
    SecurityIdT, OrderIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

  EkaSource exch = EkaSource::kNOM_ITTO;
  EkaLSI id = 0;
  uint gapNum = 0;
  uint64_t gr_ts = 0;  // time stamp in nano
  uint64_t seq_after_snapshot  = 0;
public:
  EkaFhNomGr() {
    id = 0;
    book = new FhBook(NULL,id,exch);
    book->init();
  };

  bool parseMsg(const EfhRunCtx* pEfhRunCtx,
		const unsigned char* m,
		uint64_t sequence,
		EkaFhMode op,
		std::chrono::high_resolution_clock::time_point
		startTime={});


  int invalidateBook();


private:
  template <class SecurityT, class Msg>
  SecurityT* processTradingAction(const unsigned char* m);
  
  template <class SecurityT, class Msg>
  SecurityT* processOptionOpen(const unsigned char* m);
 
  template <class SecurityT, class Msg>
  SecurityT* processAddOrder(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processAddQuote(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processOrderExecuted(const unsigned char* m,
                                  uint64_t sequence, uint64_t msgTs,
                                  const EfhRunCtx* pEfhRunCtx);

  template <class SecurityT, class Msg>
  SecurityT* processReplaceOrder(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processDeleteOrder(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processSingleSideUpdate(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processReplaceQuote(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processDeleteQuote(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processTrade(const unsigned char* m,
			  uint64_t sequence,uint64_t msgTs,
			  const EfhRunCtx* pEfhRunCtx);

  template <class SecurityT, class Msg>
  SecurityT* processAuctionUpdate(const unsigned char* m,
				  uint64_t sequence,uint64_t msgTs,
				  const EfhRunCtx* pEfhRunCtx);  
  template <class Msg>
  void processDefinition(const unsigned char* m,
			 const EfhRunCtx* pEfhRunCtx);
  
  template <class Msg>
  uint64_t processEndOfSnapshot(const unsigned char* m,
				EkaFhMode op);


};

#endif

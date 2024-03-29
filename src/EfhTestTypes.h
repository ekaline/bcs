#ifndef _EFH_TEST_TYPES_H_
#define _EFH_TEST_TYPES_H_

#include "Efc.h"
#include "Efh.h"
#include "Eka.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "Exc.h"
#include "eka_macros.h"

#include "EkaFhGroup.h"

#include "EkaFhBatsParser.h"

#define MAX_SECURITIES 600000
#define MAX_UNDERLYINGS 4000
#define MAX_EXCH 32
#define MAX_GROUPS 36
#define MAX_TEST_THREADS 16
#define SYMBOL_SIZE 32

#define DEFAULT_DISPLAY_PRICE_SCALE 10000
#define CME_DEFAULT_DISPLAY_PRICE_SCALE 100
// #define CME_DEFAULT_DISPLAY_PRICE_SCALE 10000

struct TestSecurityCtx {
  uint64_t securityId;
  std::string avtSecName;
  std::string underlying;
  std::string classSymbol;
  EfhExchange exch;
  int64_t displayPriceScale = DEFAULT_DISPLAY_PRICE_SCALE;
  bool isCompllex = false;
};

class McGrpCtx {
public:
  McGrpCtx(EkaSource _exch, EkaLSI _grId) {
    exch = _exch;
    grId = _grId;

    std::string groupName =
        std::string(EKA_EXCH_DECODE(exch)) +
        std::to_string(grId);

    std::string fullDictName =
        groupName + std::string("_FULL_DICT.txt");
    std::string subscrDictName =
        groupName + std::string("_SUBSCR_DICT.txt");
    std::string mdName = groupName + std::string("_MD.txt");

    fullDict = fopen(fullDictName.c_str(), "w");
    if (fullDict == NULL)
      on_error("Failed to open %s", fullDictName.c_str());

    subscrDict = fopen(subscrDictName.c_str(), "w");
    if (subscrDict == NULL)
      on_error("Failed to open %s", subscrDictName.c_str());

    MD = fopen(mdName.c_str(), "w");
    if (MD == NULL)
      on_error("Failed to open %s", mdName.c_str());

    TEST_LOG("%s:%d TestGrCtx created",
             EKA_EXCH_DECODE(exch), grId);
  }

  ~McGrpCtx() {
    fclose(fullDict);
    fclose(subscrDict);
    fclose(MD);
  }

  inline const TestSecurityCtx *
  getSecurityCtx(uint64_t securityId) const {
    for (const auto &sec : security) {
      if (sec.securityId == securityId)
        return &sec;
    }
    return static_cast<const TestSecurityCtx *>(nullptr);
  }

  EkaSource exch = static_cast<EkaSource>(-1);
  EkaLSI grId = static_cast<EkaLSI>(-1);

  FILE *MD = NULL;
  FILE *fullDict = NULL;
  FILE *subscrDict = NULL;

  std::vector<TestSecurityCtx> security;
};

struct TestRunGroup {
  std::string optArgStr;
};

struct TestCtx {
  bool print_tob_updates = false;
  bool subscribe_all = false;
  bool verbose_statistics = false;
  volatile bool keep_work = true;

  int fatalErrorCnt = 0;
  const int MaxFatalErrors = 4;

  std::vector<std::string> underlyings;
  std::vector<uint64_t> securities;

  McGrpCtx *grCtx[MAX_EXCH][MAX_GROUPS] = {};
};

/* ------------------------------------------------------------
 */

#endif

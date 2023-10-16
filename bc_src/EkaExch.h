#ifndef _EKA_EXCH_H_
#define _EKA_EXCH_H_

#include <inttypes.h>
#include <thread>

#include "eka_macros.h"

#include "EkaBc.h"
#include "EkaDev.h"
#include "EkaHwInternalStructs.h"

typedef enum {
  BID = 0,
  ASK = 1,
  IRRELEVANT = 3,
  ERROR = 101,
  BOTH = 200,
  NONE = 300
} SIDE;

/* ----------------------------------------------
typedef enum {
  Invalid = 0,
  PrintOnly,
  UpdateBookOnly,
  PrintAndUpdateBook,
  UpdateBookAndCheckIntegrity,
  PrintAndUpdateBookAndCheckIntegrity
} ProcessAction;

class EkaBook;
class EkaParser;
class EkaProd;
class EkaStrat;
class EkaEpm;
class EpmTemplate;

/* ----------------------------------------------

struct MdOut {
  EkaDev *dev;
  uint8_t coreId;
  uint8_t pad1[3];
  uint32_t securityId;
  SIDE side;
  uint64_t price;
  uint32_t size;
  uint8_t priceLevel;
  uint8_t pad2[3];
  uint64_t sequence;
  uint32_t pktSeq;
  uint64_t transactTime;
  bool lastEvent;
  bool tobChanged;
  uint16_t msgNum;
  uint16_t sessNum;
  EkaBook *book;
};

/* ----------------------------------------------

typedef int (*EkaOnTobChangeCb)(MdOut *mdOut, EkaBook *book,
                                SIDE side);

/* ---------------------------------------------- */

class EkaExch {
protected:
  //  uint8_t                  hwBookParams; // place holder
  //  uint8_t                  fireParams; // place holder
  /* static const uint     fireParamsSize   = 1; */
  /* static const uint     hwBookParamsSize = 1; */
  /* hw_product_param_entry_t hwProductParamEntry; */

public:
  // used for "unified" findBook

  typedef enum {
    FireLogic = 0,
    FeedServer,
    CancelMsgs
  } StrategyType;

  typedef uint64_t GlobalExchSecurityId;
  /* ---------------------------------------------- */

  static const uint MAX_BOOKS = 32;
  static const uint CORES = 8;

  using ProdId = uint32_t;

  /* ---------------------------------------------- */
  EkaExch(EkaDev *_dev) {
    dev = _dev;
    hwFeedVer = dev->hwFeedVer;
  }
  /* ---------------------------------------------- */
  virtual ~EkaExch() {
    EKA_LOG("Deleting EkaExch");
    /* if (fireLogic  != NULL) delete fireLogic; */
    /* if (feedServer != NULL) delete feedServer; */
    /* if (epm        != NULL) delete epm; */
  }

  /* ---------------------------------------------- */
  virtual EkaParser *newParser(EkaStrat *strat) = 0;
  /* ---------------------------------------------- */
  virtual EkaBook *newBook(uint8_t coreId, EkaStrat *strat,
                           EkaProd *prod) = 0;
  /* ---------------------------------------------- */
  int initStrat(StrategyType type);
  /* ---------------------------------------------- */
  uint booksPerCore(uint8_t coreId);
  /* ---------------------------------------------- */
  virtual int
  ekaDownloadProductParams(eka_product_t product_id) = 0;
  /* ---------------------------------------------- */
  virtual int
  jumpParamsApi2Internal(const jump_params_t *apiParams,
                         volatile void *dst) = 0;
  /* ---------------------------------------------- */
  virtual int rjumpParamsApi2Internal(
      const reference_jump_params_t *apiParams,
      volatile void *dst) = 0;
  /* ---------------------------------------------- */
  virtual int sendDate2Hw(EkaDev *dev) = 0;
  /* ---------------------------------------------- */
  int prepareFireReportJump(void *buf, void *payload);
  /* ---------------------------------------------- */
  virtual uint32_t getFireReportSeq(uint32_t seq) {
    return be32toh(seq);
  }
  /* ---------------------------------------------- */
  int clearHw() {
    EKA_LOG("Clearing the HW TOB (SW updated)");
    if (getHwBookParamsSize() == 1)
      on_error("hwBookParams is not set");

    int iter =
        getHwBookParamsSize() / 8 +
        !!(getHwBookParamsSize() % 8); // num of 8Byte words
    for (int i = 0; i < iter; i++) {
      eka_write(dev, 0x70000 + i * 8, 0ULL); // data loop
    }
    union large_table_desc tob_desc = {};
    tob_desc.ltd.src_bank = 0;
    tob_desc.ltd.src_thread = 0;
    for (int p = 0; p < EKA_MAX_BOOK_PRODUCTS; p++) {
      for (int s = 0; s < 2; s++) { // side
        tob_desc.ltd.target_idx = p * 2 + s;
        eka_write(dev, 0xf0410, tob_desc.lt_desc); // desc
      }
    }

    EKA_LOG("Clearing MACs");
    for (int c = 0; c < EkaDev::CONF::MAX_CORES; c++) {
      // macda
      eka_write(dev,
                CORE_CONFIG_BASE + CORE_CONFIG_DELTA * c +
                    CORE_MACDA_OFFSET,
                0ULL);
      // macsa
      eka_write(dev,
                CORE_CONFIG_BASE + CORE_CONFIG_DELTA * c +
                    CORE_MACSA_OFFSET,
                0ULL);
    }

    EKA_LOG("Clearing strategy parameter tables");
    for (uint64_t i = 0; i < EKA_HW_JUMP_DEPTH; i++) {
      eka_write(dev, 0x90000 + i * 8, 0ULL);
    }
    for (uint64_t i = 0; i < EKA_HW_RJUMP_DEPTH; i++) {
      eka_write(dev, 0xa0000 + i * 8, 0ULL);
    }

    EKA_LOG("Clearing subscription table");
    for (uint64_t i = 0; i < EKA_MAX_TOTAL_PRODUCTS; i++) {
      uint64_t subscr_wr = (0ULL | (i << 56));
      eka_write(dev, 0xf0400, subscr_wr);
      //    EKA_LOG ("writing 0x%016jx ot
      //    0x%x",subscr_wr,0xf0400);
    }

    clearFireParams();

    clearState();

    return 0;
  }
  /* ---------------------------------------------- */
  int clearProductParams() {
    uint64_t base_addr = 0x83000;
    int iter = getHwProductParamEntry() / 8 +
               !!(getHwProductParamEntry() %
                  8); // num of 8Byte words

    for (int s = 0; s < EKA_MAX_BOOK_PRODUCTS; s++)
      for (int z = 0; z < iter; z++)
        eka_write(dev, base_addr + s * iter * 8 + z * 8,
                  0ULL);
    return 0;
  }

  int clearSessionParams() {
    uint64_t base_addr = 0x86000;
    int iter = sizeof(struct hw_session_param_entry_t) / 8 +
               !!(sizeof(struct hw_session_param_entry_t) %
                  8); // num of 8Byte words

    for (int s = 0; s < EKA_MAX_SESSION_SEQ; s++)
      for (int z = 0; z < iter; z++)
        eka_write(dev, base_addr + s * iter * 8 + z * 8,
                  0ULL);
    return 0;
  }

  /* ---------------------------------------------- */
  int setFireParams(int sock, void *params);
  /* ---------------------------------------------- */
  void clearFireParams() {
    uint64_t baseAddr = 0x20000;
    int iter =
        getFireParamsSize() / 8 +
        !!(getFireParamsSize() % 8); // num of 8Byte words
    for (int c = 0; c < EkaDev::CONF::MAX_CORES; c++) {
      for (int s = 0; s < EKA_MAX_CME_FIRE_PARAMS; s++) {
        for (int z = 0; z < iter; ++z)
          eka_write(dev, baseAddr + c * 0x1000 + z * 8,
                    0ULL); // data
        eka_write(dev, 0x2f000 + c * 0x100,
                  s * 0x10000); // desc
      }
    }
  }
  /* ---------------------------------------------- */
  void clearState() {
    EKA_LOG("Disarming all products");
    // Disarm all products
    for (uint64_t p = 0; p < EKA_MAX_BOOK_PRODUCTS; p++)
      eka_write(dev, P4_ARM_DISARM,
                (uint64_t)(p & 0xff) << 56);

    EKA_LOG("Clearing product parameter tables");
    //    eka_clear_product_params(dev);
    clearProductParams();
    clearSessionParams(); // must come before STAT_CLEAR
    eka_write(dev, BOOK_CLEAR,
              (uint64_t)1); // Clearing HW TOB entries
    eka_write(dev, STAT_CLEAR,
              (uint64_t)1); // Clearing HW Statistics
    eka_write(dev, SW_STATISTICS,
              (uint64_t)0); // Clearing SW Statistics
    eka_write(dev, P4_STRAT_CONF,
              (uint64_t)0); // Clearing Strategy params

    EKA_LOG("Clearing interrupts");
    eka_read(dev, 0xf0728); // Clearing Interrupts
    eka_read(dev, 0xf0798); // Clearing Interrupts

    EKA_LOG("Clearing scratchpad");
    for (uint64_t p = 0; p < SW_SCRATCHPAD_SIZE / 8; p++)
      eka_write(dev, SW_SCRATCHPAD_BASE + 8 * p,
                (uint64_t)0);
  }
  /* ---------------------------------------------- */
  virtual uint16_t getHwRawFireSize() = 0;

  /* ---------------------------------------------- */
  virtual uint16_t getHwBookParamsSize() = 0;
  /* ---------------------------------------------- */
  virtual uint16_t getFireParamsSize() = 0;
  /* ---------------------------------------------- */
  virtual uint16_t getHwProductParamEntry() = 0;
  /* ---------------------------------------------- */

  virtual int onTobChange(MdOut *mdOut, EkaBook *b,
                          SIDE side) = 0;

  /* ---------------------------------------------- */
  virtual int createCancelMsg(uint8_t *dst, void *params) {
    return 0;
  }
  /* ---------------------------------------------- */
  virtual int createNewOrder(uint8_t *dst, void *params) {
    return 0;
  }
  /* ---------------------------------------------- */
  /* ---------------------------------------------- */
  virtual int updateCancelMsg(uint prodIdx, uint8_t *dst,
                              void *params) {
    return 0;
  }
  /* ---------------------------------------------- */
  virtual int updateNewOrder(uint prodIdx, uint8_t *dst,
                             void *params) {
    return 0;
  }
  /* ---------------------------------------------- */
  virtual EpmTemplate *
  addCancelMsgTemplate(uint templateId) {
    return NULL;
  }
  /* ---------------------------------------------- */
  virtual EpmTemplate *
  addNewOrderMsgTemplate(uint templateId) {
    return NULL;
  }
  /* ---------------------------------------------- */

  EkaStrat *fireLogic;
  EkaStrat *feedServer;
  EkaStrat *cancelMsgs;
  //  EkaEpm*        epm;

  EkaDev *dev;
  uint8_t hwFeedVer;
};

#endif

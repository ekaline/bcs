#ifndef _EPM_TEMPLATE_H_
#define _EPM_TEMPLATE_H_

#include <inttypes.h>
#include <string.h>
#include "EkaHwInternalStructs.h"
#include "EkaEpm.h"

typedef const char*    EpmFieldName;
typedef uint16_t       EpmFieldSize;

//#define EpmMaxRawTcpSize 1536
//#define EpmMaxRawTcpSize 80

/* parameter EPM_SRC_IMMEDIATE    = 0; */
/* parameter EPM_SRC_LOCAL_SEQ    = 1; */
/* parameter EPM_SRC_REMOTE_SEQ   = 2; */
/* parameter EPM_SRC_TCP_WINDOW   = 3; */
/* parameter EPM_SRC_TCP_CHCK_SUM = 4; */
/* parameter EPM_SRC_SECURITY_ID  = 5; */
/* parameter EPM_SRC_PRICE        = 6;// for orders use this, for quotes this is buy side */
/* parameter EPM_SRC_SIZE         = 7;// for orders use this, for quotes this is buy side  */
/* parameter EPM_SRC_SIDE         = 8; */
/* parameter EPM_SRC_TIME         = 9; */
/* parameter EPM_SRC_APP_SEQ      = 10; */
/* parameter EPM_SRC_ASK_PRICE    = 11;// for orders use this, for quotes this is ask side */
/* parameter EPM_SRC_ASK_SIZE     = 12;// for orders use this, for quotes this is ask side */
// parameter EPM_SRC_ASCII_CNT    = 14;

enum class HwField : uint8_t {
    IMMEDIATE     = 0,
    LOCAL_SEQ     = 1,
    REMOTE_SEQ    = 2,
    TCP_WINDOW    = 3,
    TCP_CHCK_SUM  = 4,
    RESERVED      = 5,
    PRICE         = 6,  // for orders use this, for quotes this is buy side
    SIZE          = 7,  // for orders use this, for quotes this is buy side
    SIDE          = 8,
    TIME          = 9,
    APPSEQ        = 10,
    ASK_PRICE     = 11, // for orders use this, for quotes this is ask side
    ASK_SIZE      = 12, // for orders use this, for quotes this is ask side
    SECURITY_ID   = 13,
    APP_SEQ_ASCII = 14
};

#define EpmHwField2Str(x) \
  x == HwField::IMMEDIATE       ? "IMM" : \
    x == HwField::LOCAL_SEQ     ? "LOCAL_SEQ" : \
    x == HwField::REMOTE_SEQ    ? "REMOTE_SEQ" : \
    x == HwField::TCP_WINDOW    ? "TCP_WINDOW" : \
    x == HwField::TCP_CHCK_SUM  ? "TCP_CHCK_SUM" : \
    x == HwField::SECURITY_ID   ? "SECURITY_ID" : \
    x == HwField::PRICE         ? "PRICE" : \
    x == HwField::SIZE          ? "SIZE" : \
    x == HwField::SIDE          ? "SIDE" : \
    x == HwField::TIME          ? "TIME" : \
    x == HwField::APPSEQ        ? "APPSEQ" : \
    x == HwField::APPSEQ        ? "APPSEQ" : \
    x == HwField::ASK_PRICE     ? "ASK_PRICE" : \
    x == HwField::ASK_SIZE      ? "ASK_SIZE" : \
    "UNDEFINED"

struct EpmTemplateField {
  EpmFieldName  name;
  EpmFieldSize  size;
  HwField       source;
  bool          swap;
  bool          clear; // to be cleared when written to FPGA
} __attribute__((packed));

class EpmTemplate {
 public:
  static const uint EpmMaxRawTcpSize = EkaEpm::MAX_ETH_FRAME_SIZE;
  static const uint EpmNumHwFields   = EkaEpm::EpmNumHwFields;
  static const uint EpmHwFieldSize   = EkaEpm::EpmHwFieldSize;

  struct EpmHwField {
    uint8_t cksmMSB[EpmHwFieldSize];
    uint8_t cksmLSB[EpmHwFieldSize];
  } __attribute__((packed));

 protected:
  EpmTemplate(uint idx, const char* name);

 public:
  int init();

  int clearHwFields(uint8_t* pkt);

  uint64_t getDataTemplateAddr() {
    return EPM_DATA_TEMPLATE_BASE_ADDR + id * EpmMaxRawTcpSize;
  }  

  uint64_t getCsumTemplateAddr() {
    return EPM_CSUM_TEMPLATE_BASE_ADDR + id * ( 2/*msb+lsb*/ * EpmNumHwFields * EpmHwFieldSize / 8/*1bit per value*/);
  }

  virtual uint16_t payloadSize() {
    return (uint16_t) (EpmMaxRawTcpSize - sizeof(EkaEthHdr) - sizeof(EkaIpHdr) - sizeof(EkaTcpHdr));
  }

  /* --------------------------------------------- */
  static const uint MAX_FIELDS = 512;

  uint8_t    data[EpmMaxRawTcpSize]  = {};
  EpmHwField hwField[EpmNumHwFields] = {};
  char       name[32]                = {};
  uint       id                      = (uint)-1; // free running idx

  EpmTemplateField templateStruct[MAX_FIELDS] = {};
  uint tSize = 0;
};

#endif

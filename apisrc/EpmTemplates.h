#ifndef _EPM_TEMPLATES_H_
#define _EPM_TEMPLATES_H_

#include <string.h>

typedef const char*    EpmFieldName;
typedef uint16_t       EpmFieldSize;

//#define EpmMaxRawTcpSize 1536
#define EpmMaxRawTcpSize 80

#define EpmNumHwFields 16
#define EpmHwFieldSize 16


enum class HwField : uint8_t {
  IMMEDIATE      = 0,
    LOCAL_SEQ    = 1,
    REMOTE_SEQ   = 2,
    TCP_WINDOW   = 3,
    TCP_CHCK_SUM = 4,
    SECURITY_ID  = 5,
    PRICE        = 6,
    SIZE         = 7,
    SIDE         = 8,
    TIME         = 9
};

#define EpmHwField2Str(x) \
  x == HwField::IMMEDIATE       ? "IMMEDIATE" : \
    x == HwField::LOCAL_SEQ     ? "LOCAL_SEQ" : \
    x == HwField::REMOTE_SEQ    ? "REMOTE_SEQ" : \
    x == HwField::TCP_WINDOW    ? "TCP_WINDOW" : \
    x == HwField::TCP_CHCK_SUM  ? "TCP_CHCK_SUM" : \
    x == HwField::SECURITY_ID   ? "SECURITY_ID" : \
    x == HwField::PRICE         ? "PRICE" : \
    x == HwField::SIZE          ? "SIZE" : \
    x == HwField::SIDE          ? "SIDE" : \
    x == HwField::TIME          ? "TIME" : \
    "UNDEFINED"

struct EpmHwField {
  uint8_t cksmEnable[EpmHwFieldSize];
  uint8_t cksmMSB[EpmHwFieldSize];
  uint8_t cksmLSB[EpmHwFieldSize];
} __attribute__((packed));

struct EpmTemplateField {
  EpmFieldName  name;
  EpmFieldSize  size;
  HwField       source;
} __attribute__((packed));


EpmTemplateField EpmEtherHdr[] = {
  {"macDa"  , 6, HwField::IMMEDIATE},
  {"macSa"  , 6, HwField::IMMEDIATE},
  {"ethType", 2, HwField::IMMEDIATE}
};

EpmTemplateField EpmIpHdr[] = {
  {"v_hl"  , 1 , HwField::IMMEDIATE},
  {"tos"   , 1 , HwField::IMMEDIATE},
  {"len"   , 2 , HwField::IMMEDIATE},
  {"id"    , 2 , HwField::IMMEDIATE},
  {"offset", 2 , HwField::IMMEDIATE},
  {"ttl"   , 1 , HwField::IMMEDIATE},
  {"proto" , 1 , HwField::IMMEDIATE},
  {"chksum", 2 , HwField::IMMEDIATE},
  {"src"   , 4 , HwField::IMMEDIATE},
  {"dst"   , 4 , HwField::IMMEDIATE},
};


EpmTemplateField EpmTcpHdr[] = {
  {"src"   , 2 , HwField::IMMEDIATE},
  {"dest"  , 2 , HwField::IMMEDIATE},
  {"seqno" , 4 , HwField::LOCAL_SEQ},
  {"ackno" , 4 , HwField::REMOTE_SEQ},
  {"_hdrlen_rsvd_flags"  , 2 , HwField::IMMEDIATE},
  {"wnd"   , 2 , HwField::TCP_WINDOW},
  {"chksum", 2 , HwField::TCP_CHCK_SUM},
  {"urgp"  , 2 , HwField::IMMEDIATE},

};

EpmTemplateField payload = {"payload"  , 1536 - 14 - 20 - 20 , HwField::IMMEDIATE};

EpmTemplateField TemplateTcpPkt[] = {
  {"macDa"  , 6, HwField::IMMEDIATE},
  {"macSa"  , 6, HwField::IMMEDIATE},
  {"ethType", 2, HwField::IMMEDIATE},

  {"v_hl"  , 1 , HwField::IMMEDIATE},
  {"tos"   , 1 , HwField::IMMEDIATE},
  {"len"   , 2 , HwField::IMMEDIATE},
  {"id"    , 2 , HwField::IMMEDIATE},
  {"offset", 2 , HwField::IMMEDIATE},
  {"ttl"   , 1 , HwField::IMMEDIATE},
  {"proto" , 1 , HwField::IMMEDIATE},
  {"chksum", 2 , HwField::IMMEDIATE},
  {"src"   , 4 , HwField::IMMEDIATE},
  {"dst"   , 4 , HwField::IMMEDIATE},

  {"src"   , 2 , HwField::IMMEDIATE},
  {"dest"  , 2 , HwField::IMMEDIATE},
  {"seqno" , 4 , HwField::LOCAL_SEQ},
  {"ackno" , 4 , HwField::REMOTE_SEQ},
  {"_hdrlen_rsvd_flags"  , 2 , HwField::IMMEDIATE},
  {"wnd"   , 2 , HwField::TCP_WINDOW},
  {"chksum", 2 , HwField::TCP_CHCK_SUM},
  {"urgp"  , 2 , HwField::IMMEDIATE},

  {"payload"  , EpmMaxRawTcpSize - 14 - 20 - 20 , HwField::IMMEDIATE}
};



#endif

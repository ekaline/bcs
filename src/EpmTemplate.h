#ifndef _EPM_TEMPLATE_H_
#define _EPM_TEMPLATE_H_

#include <inttypes.h>
#include <string.h>
#include "EkaHwInternalStructs.h"
#include "EkaEpm.h"

#include "EpmHwFieldsMap.h"

typedef const char*    EpmFieldName;
typedef uint16_t       EpmFieldSize;

//#define EpmMaxRawTcpSize 1536
//#define EpmMaxRawTcpSize 80



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
  EpmTemplate(uint idx);

 public:
  int init();
  int init_old();

  int clearHwFields(uint8_t* pkt);
  
  uint64_t getDataTemplateAddr() {
    return EPM_DATA_TEMPLATE_BASE_ADDR + id * EpmMaxRawTcpSize;
  }  

  uint64_t getCsumTemplateAddr() {
    return EPM_CSUM_TEMPLATE_BASE_ADDR + id * ( 2/*msb+lsb*/ * EpmNumHwFields * EpmHwFieldSize / 8/*1bit per value*/);
  }

  uint getByteSize() const {
    return (uint16_t) (byteSize - sizeof(EkaEthHdr) - sizeof(EkaIpHdr) - sizeof(EkaTcpHdr));
  }

  virtual uint16_t payloadSize() {
    return (uint16_t) (EpmMaxRawTcpSize - sizeof(EkaEthHdr) - sizeof(EkaIpHdr) - sizeof(EkaTcpHdr));
  }

  inline void setCsBitmap(uint idx, uint8_t hwIdx) {
    int row = hwIdx / 16;
    int col = hwIdx % 16;
    uint16_t bitmapSet = ((uint16_t)1)<<col;
    if (idx % 2 == 0) {
      // hw_tcpcs_template.low.row[row].bitmap |= ((uint16_t)1)<<col;
      uint16_t bitmapTmp = hw_tcpcs_template.low.row[row].bitmap | bitmapSet;
      hw_tcpcs_template.low.row[row].bitmap =  bitmapTmp;
    } else {
      // hw_tcpcs_template.high.row[row].bitmap |= ((uint16_t)1)<<col;
      uint16_t bitmapTmp = hw_tcpcs_template.high.row[row].bitmap | bitmapSet;
      hw_tcpcs_template.high.row[row].bitmap = bitmapTmp;
    }
  }
  
  /* --------------------------------------------- */
  static const uint MAX_FIELDS = 512;

  uint8_t    data[EpmMaxRawTcpSize]  = {};
  EpmHwField hwField[EpmNumHwFields] = {};
  char       name[32]                = {};
  uint       id                      = (uint)-1; // free running idx

  EpmTemplateField templateStruct[MAX_FIELDS] = {};
  uint tSize = 0;
  uint byteSize = 0;
  EpmHwFieldsMap* fMap = NULL;

  volatile epm_tcpcs_template_t hw_tcpcs_template = {};

};

#endif

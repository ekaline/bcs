#ifndef _EPM_HW_FIELDS_MAP_H_
#define _EPM_HW_FIELDS_MAP_H_
#include <stdio.h>

#include "eka_macros.h"


// Parameter EPM_SIZE_IMMEDIATE bound to: 8 - type: integer 
// Parameter EPM_SIZE_LOCAL_SEQ bound to: 4 - type: integer 
// Parameter EPM_SIZE_REMOTE_SEQ bound to: 4 - type: integer 
// Parameter EPM_SIZE_TCP_WINDOW bound to: 2 - type: integer 
// Parameter EPM_SIZE_TCP_CHCK_SUM bound to: 2 - type: integer

// Parameter EPM_SIZE_PRICE bound to: 8 - type: integer 
// Parameter EPM_SIZE_SIZE bound to: 8 - type: integer 
// Parameter EPM_SIZE_SIDE bound to: 1 - type: integer 
// Parameter EPM_SIZE_TIME bound to: 16 - type: integer 
// Parameter EPM_SIZE_APP_SEQ bound to: 8 - type: integer 
// Parameter EPM_SIZE_ASK_PRICE bound to: 8 - type: integer 
// Parameter EPM_SIZE_ASK_SIZE bound to: 8 - type: integer 
// Parameter EPM_SIZE_SECID bound to: 8 - type: integer 
// Parameter EPM_SIZE_ASCII_CNT bound to: 8 - type: integer 

enum class HwField : uint8_t {
    IMMEDIATE     = 0,
    LOCAL_SEQ,
    REMOTE_SEQ,
    TCP_WINDOW,
    TCP_CHCK_SUM,
    
    PRICE, 
    SIZE, 
    SIDE,
    TIME,

    APPSEQ,

    ASK_PRICE,
    ASK_SIZE,

    SECURITY_ID,
    ASCII_CNT,

    Count
};

#define EpmHwField2Str(x)				\
    x == HwField::IMMEDIATE       ? "IMM" :		\
	x == HwField::LOCAL_SEQ     ? "LOCAL_SEQ" :	\
	x == HwField::REMOTE_SEQ    ? "REMOTE_SEQ" :	\
	x == HwField::TCP_WINDOW    ? "TCP_WINDOW" :	\
	x == HwField::TCP_CHCK_SUM  ? "TCP_CHCK_SUM" :	\
							\
	x == HwField::PRICE    ? "PRICE" :		\
	x == HwField::SIZE     ? "SIZE" :		\
	x == HwField::SIDE     ? "SIDE" :		\
	x == HwField::TIME     ? "TIME" :		\
							\
	x == HwField::APPSEQ   ? "APPSEQ" :		\
							\
	x == HwField::ASK_PRICE    ? "ASK_PRICE" :	\
	x == HwField::ASK_SIZE     ? "ASK_SIZE" :	\
							\
	x == HwField::SECURITY_ID    ? "SECURITY_ID" :		\
	x == HwField::ASCII_CNT    ? "ASCII_CNT" :	\
	"UNDEFINED"

struct EpmHwField {
  HwField fIdx;
  char    name[32];
  int     start;
  int     size;
};

class EpmHwFieldsMap {
 public:
  EpmHwFieldsMap() {
    map[(int)HwField::IMMEDIATE].size = 8;
    map[(int)HwField::LOCAL_SEQ].size = 4;
    map[(int)HwField::REMOTE_SEQ].size = 4;
    map[(int)HwField::TCP_WINDOW].size = 2;
    map[(int)HwField::TCP_CHCK_SUM].size = 2;
    
    map[(int)HwField::PRICE].size = 8;
    map[(int)HwField::SIZE].size = 8;
    map[(int)HwField::SIDE].size = 1;
    map[(int)HwField::TIME].size = 16;

    map[(int)HwField::APPSEQ].size = 8;
    
    map[(int)HwField::ASK_PRICE].size = 8;
    map[(int)HwField::ASK_SIZE].size = 8;
    
    map[(int)HwField::SECURITY_ID].size = 8;
    map[(int)HwField::ASCII_CNT].size = 8;
    
    for (uint8_t i = 0; i < (const int)HwField::Count; i++) {
      map[i].fIdx = (HwField)i;
      strcpy(map[i].name,EpmHwField2Str(map[i].fIdx));
      map[i].start = i == 0 ? 0 : map[i-1].start + map[i-1].size;
    }

    print();
  }

  void print() {
    for (uint8_t i = 0; i < (const int)HwField::Count; i++) {
      TEST_LOG("%2u: %20s %3d %3d",
	       (uint)map[i].fIdx,
	       map[i].name,
	       map[i].start,
	       map[i].size
	       );
    }
  }
  
  EpmHwField map[(const int)HwField::Count] = {};
};
#endif

#include <stdio.h>

#include "EpmTemplate.h"
#include "eka_macros.h"
#include "EkaDev.h"

EpmTemplate::EpmTemplate (uint idx, const char* _name) {
  memset(&data,0,sizeof(data));
  memset(&hwField,0,sizeof(hwField));
  id = idx;
  strcpy(name,_name);
  
  //TEST_LOG("Creating template %u \'%s\'",id,name);
}

int EpmTemplate::init() {
  //TEST_LOG("%s with %u fields",name,tSize);
  uint idx = 0;
  EpmTemplateField* t = templateStruct;
  EpmTemplateField* f = (EpmTemplateField*)t;
  for (int i = 0; i < (int)tSize; i++) {
    /* EKA_LOG("%2d: %s: size = %u, source = %s, swap = %d", */
    /* 	   i,f->name,f->size, */
    /* 	    f->source == HwField::IMMEDIATE ? "IMM" : "HW", */
    /* 	    f->swap); */

    for (uint16_t b = 0; b < f->size; b++) {
      uint8_t shiftedSrc = ((uint8_t)f->source) << 4;
      uint8_t byteOffs   = (f->swap)? (b) & 0xf : (f->size - b - 1) & 0xf;
      data[idx] = shiftedSrc | 
	(f->source != HwField::IMMEDIATE ? byteOffs : idx % 4);

      if (f->source != HwField::IMMEDIATE && f->source != HwField::TCP_CHCK_SUM) {
	if (f->size > EpmHwFieldSize) 
	  on_error("%s size (%u) > EpmHwFieldSize %u",f->name,f->size,EpmHwFieldSize);
	int fieldIdx = (int)f->source;
	if (f->swap) {
	  hwField[fieldIdx].cksmLSB[b]    = (uint8_t)(idx % 2 == 0);
	  hwField[fieldIdx].cksmMSB[b]    = (uint8_t)(idx % 2 == 1);
	} else {
	  hwField[fieldIdx].cksmLSB[b]    = (uint8_t)(idx % 2 == 1);
	  hwField[fieldIdx].cksmMSB[b]    = (uint8_t)(idx % 2 == 0);
	}
      }
      idx++;
    }
    f++;
  }

#if 0
  for (int i = 0; i < EpmMaxRawTcpSize; i++) {
    if (i % 8 == 0) printf ("\n");
    printf ("{%14s, %2d} (0x%02x),",EpmHwField2Str((HwField)(data[i] >> 4)), data[i] & 0xf,data[i]);
  }
  printf ("\n\n");
#endif

#if 0
  for (int i = 0; i < EpmNumHwFields; i++) {
    printf ("%-20s: ",EpmHwField2Str((HwField)i));
    for (int b = EpmHwFieldSize - 1; b >= 0; b--) {
      printf ("%c, ",hwField[i].cksmMSB[b] ? 'M' : '-');
    }
    printf ("   ");

    for (int b = EpmHwFieldSize - 1; b >= 0; b--) {
      printf ("%c, ",hwField[i].cksmLSB[b] ? 'L' : '-');
    }
    printf ("\n");
  }
#endif

  return 0;
}

int EpmTemplate::clearHwFields(uint8_t* pkt) {
  //  hexDump("Packet before",pkt,256);
  EpmTemplateField* t = templateStruct;
  uint idx = 0;
  EpmTemplateField* f = (EpmTemplateField*)t;
  for (int i = 0; i < (int)tSize; i++) {
    for (uint16_t b = 0; b < f->size; b++) {
      if (f->clear) pkt[idx] = 0;
      idx++;
    }
    f++;
  }
  //  EKA_LOG("%s pktSize = %u with %u fields",name,idx,tSize);
  //  hexDump("Packet after",pkt,256);

  return 0;
}

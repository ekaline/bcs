#include <stdio.h>

#include "EkaDev.h"
#include "EpmTemplate.h"
#include "eka_macros.h"

EpmTemplate::EpmTemplate(uint idx) {
  memset(&data, 0, sizeof(data));
  memset(&hwField, 0, sizeof(hwField));
  id = idx;
}

#define _EPM_TEMPLATE_DEBUG_PRINTOUT_ 1

int EpmTemplate::init() {
  EKA_LOG("\n================================\n%s with %u "
          "fields\n================================",
          name, tSize);

  fMap = new (EpmHwFieldsMap);
  if (!fMap)
    on_error("! fMap");

  uint idx = 0;
  EpmTemplateField *t = templateStruct;
  byteSize = 0;

  for (int i = 0; i < (int)tSize; i++) {
    EpmTemplateField *f = &t[i];

    /* EKA_LOG("%2d: %s: start=%u size = %u, source = %s,
     * swap = %d", */
    /* 	     i,f->name,idx,f->size, */
    /* 	     f->source == HwField::IMMEDIATE ? "IMM" : "HW",
     */
    /* 	     f->swap); */

    auto fType = f->source;
    if (fType != HwField::IMMEDIATE &&
        f->size != fMap->map[(int)fType].size)
      EKA_LOG("WARNING: %s template field %s size %d != "
              "HW field size %d",
              name, fMap->map[(int)fType].name, f->size,
              fMap->map[(int)fType].size);

    for (uint16_t b = 0; b < f->size; b++) {
      if (f->source == HwField::IMMEDIATE) {
        data[idx] = (uint8_t)(idx % 8);
      } else {
        auto hwFiledByteIdx =
            f->swap ? fMap->map[(int)fType].start + b
                    : fMap->map[(int)fType].start +
                          f->size - b - 1;
        data[idx] = (uint8_t)hwFiledByteIdx;
        if (f->source != HwField::TCP_CHCK_SUM)
          setCsBitmap(idx, hwFiledByteIdx);
      }
      idx++;
    }
    byteSize += f->size;
  }

  //  EKA_LOG("%s byteSize = %u",name,byteSize);

#if 0

  for (uint i = 0; i < byteSize; i++) {
    if (i % 8 == 0) printf ("\n");
    printf ("0x%02x,",
	    data[i]);
  }
  printf ("\n\n");
  EKA_LOG("Im here");
#endif

#if 0
  printf ("%s HW bytes map:\n",name);
  for (uint i = 0; i < byteSize; i++) {
    if (i % 8 == 0) printf ("\n");
    //    printf ("{%14s, %2d} (0x%02x),",EpmHwField2Str((HwField)(data[i] >> 4)), data[i] & 0xf,data[i]);
    printf ("0x%02x ",data[i]);
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

int EpmTemplate::clearHwFields(uint8_t *pkt) {
  //  hexDump("Packet before",pkt,256);
  uint idx = 0;
  EpmTemplateField *f = templateStruct;
  for (size_t i = 0; i < tSize; i++) {
    for (uint16_t b = 0; b < f->size; b++) {
      if (f->clear)
        pkt[idx] = 0;
      idx++;
    }
    f++;
  }
  //  EKA_LOG("%s pktSize = %u with %u
  //  fields",name,idx,tSize); hexDump("Packet
  //  after",pkt,256);

  return 0;
}

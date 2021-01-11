#ifndef _EPM_FIRE_SQF_TEMPLATE_H_
#define _EPM_FIRE_SQF_TEMPLATE_H_

#include "EpmTemplate.h"

// THIS IS A PLACE HOLDER!!!

class EpmFireSqfTemplate : public EpmTemplate {
 public:
 EpmFireSqfTemplate(uint idx, const char* _name) : EpmTemplate(idx,_name) {
  
    EpmTemplateField myTemplateStruct[] = {
      {"rawL2PktData"  , EpmMaxRawTcpSize , HwField::IMMEDIATE, false, false}
    };
    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    init();
  }

};

#endif

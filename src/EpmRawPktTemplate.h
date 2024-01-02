#ifndef _EPM_RAW_PKT_TEMPLATE_H_
#define _EPM_RAW_PKT_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmRawPktTemplate : public EpmTemplate {
 public:
 EpmRawPktTemplate(uint idx) : EpmTemplate(idx) {
  
    EpmTemplateField myTemplateStruct[] = {
      {"rawL2PktData"  , EpmMaxRawTcpSize , HwField::IMMEDIATE, false, false}
    };
    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    strcpy(name,"EpmRawPktTemplate");
    init();
  }

};

#endif

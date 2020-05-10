#ifndef _EPM_CAPS_H_
#define _EPM_CAPS_H_


#define EPM_PayloadMemorySize   static_cast<uint64_t>(4096)
#define EPM_PayloadAlignment    static_cast<uint64_t>(8)
#define EPM_DatagramOffset      static_cast<uint64_t>(14+20+20)
#define EPM_RequiredTailPadding static_cast<uint64_t>(4)
#define EPM_MaxStrategies       static_cast<uint64_t>(8)
#define EPM_MaxActions          static_cast<uint64_t>(4)


#endif

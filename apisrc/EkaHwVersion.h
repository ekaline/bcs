#ifndef _EKA_HW_VERSION_H_
#define _EKA_HW_VERSION_H_

class EkaDev;

class EkaHwVersion {
  static const uint64_t VERSION_MASK      = 0xFF;

  static const uint64_t EPM_VERSION_ADDR  = 0xf0fe0;
  static const uint64_t EPM_VERSION_SHIFT = 16;

  static const uint64_t DMA_VERSION_ADDR  = 0xf0fe0;
  static const uint64_t DMA_VERSION_SHIFT = 24;

/* ------------------------------ */
 public:
  static bool check(EkaDev* dev);

};

#endif

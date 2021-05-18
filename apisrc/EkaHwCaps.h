#ifndef _EKA_HW_CAPS_H_
#define _EKA_HW_CAPS_H_

class EkaDev;

class EkaHwCaps {
  static const uint64_t HwCapabilitiesAddr = 0x8f000;

  struct hw_version_capabilities_t {
    uint8_t  hwcaps;
    uint8_t  hwparser; //hw engine revision
    uint8_t  strategy;
    uint8_t  parser; //soft modified
    uint8_t  sniffer;
    uint8_t  dma;
    uint8_t  epm;
    uint16_t build_seed;
    uint8_t  build_time_sec;
    uint8_t  build_time_min;
    uint8_t  build_date_day;
    uint8_t  build_date_month;
    uint8_t  build_date_year;
    uint32_t ekaline_git;
    uint32_t silicom_git;
  } __attribute__((packed));

  struct hw_epm_capabilities_t {
    uint16_t numof_actions;
    uint8_t tcpcs_numof_templates;
    uint16_t data_template_total_bytes;
    uint32_t heap_total_bytes;
    uint8_t max_threads;
    uint16_t numof_regions;
  } __attribute__((packed));

  struct hw_core_capabilities_t {
    uint16_t tcp_sessions_percore;
    uint8_t bitmap_tcp_cores;
    uint8_t bitmap_md_cores;
  } __attribute__((packed));

  struct hw_entity_capabilities_t {
    uint8_t numof_entities;
  } __attribute__((packed));

  struct hw_scratchpad_capabilities_t {
    uint32_t size;
  } __attribute__((packed));

  struct hw_capabilities_t {
    hw_version_capabilities_t version;
    hw_scratchpad_capabilities_t scratchpad;
    hw_entity_capabilities_t entity;
    hw_core_capabilities_t core;
    hw_epm_capabilities_t epm;
  } __attribute__((packed));


  /* ------------------------------ */
 public:
  EkaHwCaps(EkaDev* _dev);
  void print();
  void printStdout();

  bool check();

  hw_capabilities_t hwCaps = {};

  uint64_t snDriverVerNum = -1;

 private:
  void print2buf();

  EkaDev* dev = NULL;

  static const int bufSize = 10000;

  uint idx = 0;
  char buf[bufSize] = {};
};

#endif

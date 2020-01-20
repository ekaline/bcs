#ifndef _EKA_SN_DEV_H
#define _EKA_SN_DEV_H

#include "smartnic.h"
#include "eka_macros.h"
#include "ctls.h"
#include "eka.h"


class eka_sn_dev {
 public:
  eka_sn_dev(EkaDev* dev);
  ~eka_sn_dev();

  void       reset();
  uint64_t   read(uint64_t addr);
  void       write(uint64_t addr, uint64_t val);
  bool       get_link_status(uint8_t lane, bool* sfp_present, bool* link);

  void       get_ip_mac(uint8_t lane, uint32_t* ip, uint8_t* mac);
  void       set_fast_session (uint8_t core, uint8_t sess);
  void       ioremap_wc_tx_pkts ();

  volatile uint8_t* txPktBuf;

  SN_DeviceId dev_id;
 private:
  EkaDev* dev;
};

#endif

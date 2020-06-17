#ifndef _EKA_API_FUNCS_H
#define _EKA_API_FUNCS_H
//#include "efh.h"
//#include "exc_socket_def.h"
#include "ekaline.h"
#include "EkaDev.h"

//#include "EkaOpResult.h"
//#include "EfcCtxs.h"
//#include "Efc.h"

  //  EkaOpResult efc_hw_init(EfcCtx* pEfcCtx);
  //  void efc_run (EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx);

  uint64_t eka_read(eka_dev_t* dev, uint64_t addr); // FPGA specific access to mmaped space
  void eka_write(eka_dev_t* dev, uint64_t addr, uint64_t val); // FPGA specific access to mmaped space
  int eka_open_udp_sockets(eka_dev_t* dev);
  void eka_enable_rx(eka_dev_t* dev); // obsolete
  void eka_disable_rx(eka_dev_t* dev); // obsolete
  int32_t eka_add_fh_subscription(eka_dev_t* dev, int32_t group, uint64_t sec_id);
  uint32_t get_subscription_id(eka_dev_t* dev, uint64_t id);
  void print_fire_report (fire_report_t* p,int len);
  int eka_set_session_fire_app_ctx(eka_dev_t* dev, uint16_t sess_id , void *ctx);
  void eka_arm_controller(eka_dev_t* dev, uint8_t arm);
  int eka_set_security_ctx (eka_dev_t* dev,uint64_t id, struct sec_ctx *ctx, uint8_t ch);
/* eka_add_conf_t eka_conf_parse(eka_dev_t* dev, eka_conf_type_t conf_type, const char *key, const char *value); */
eka_add_conf_t eka_conf(eka_dev_t* dev, const char *key, const char *value);

  int eka_init_strategy_params (eka_dev_t* dev,const struct global_params *params);
  uint64_t eka_subscr_security2fire(eka_dev_t* dev, uint64_t id);
  void download_subscr_table(eka_dev_t* dev, bool clear_all);
  int eka_fh_recv_start_group (eka_dev_t* dev, uint8_t gr_id);
  int eka_fh_recv_start(eka_dev_t* dev);
  int eka_fh_recv_stop(eka_dev_t* dev);
  void eka_fh_close(eka_dev_t* dev); 
  void* eka_fh_open (eka_dev_t* dev);  
  eka_subscr_status_t eka_fh_subscribe (eka_dev_t* dev, uint8_t gr_id, uint32_t secid);
  void prepare_fire_report(fire_report_t *target, fire_report_t *source);
  int eka_set_group_session(eka_dev_t* dev, uint8_t gr, uint16_t sess_id);
  size_t eka_get_fire_report(eka_dev_t* dev, uint8_t* buf);

  int eka_socket(eka_dev_t* dev, uint8_t core);
  uint16_t eka_connect (eka_dev_t* dev, int sock, const struct sockaddr *dst, socklen_t addrlen);
  ssize_t eka_send(eka_dev_t* dev, uint16_t sess_id, void *buf, size_t size);
  ssize_t eka_recv(eka_dev_t* dev, uint16_t sess_id, void *buffer, size_t size);

  void eka_write_cpp(SN_DeviceId dev_id, uint64_t addr, uint64_t val);
  uint64_t eka_read_cpp(SN_DeviceId dev_id, uint64_t addr);
  int convert_ts(char* dst, uint64_t ts);

  int eka_set_security_ctx_with_idx (eka_dev_t* dev, uint32_t ctx_idx, struct sec_ctx *ctx, uint8_t ch);
  void eka_get_ip_for_nic_cores (eka_dev_t* dev, const char* name, int name_len);
  bool eka_is_all_zeros (const void* buf, ssize_t size);
  uint8_t session2core (uint16_t id);
  uint8_t session2sess (uint16_t id);
  int decode_session_id (uint16_t id, uint8_t* core, uint8_t* sess);
  uint16_t encode_session_id (uint8_t core, uint8_t sess);
  uint16_t socket2session (eka_dev_t* dev, int sock_fd);
  void errno_decode(int errsv, char* reason);
  void eka_enable_cores(eka_dev_t* dev);
  void eka_disable_cores(eka_dev_t* dev);
//void hexDump (eka_dev_t* dev, char *desc, void *addr, int len);
void hexDump (const char* desc, void *addr, int len);

#endif

#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <sched.h>
#include <sys/resource.h>
#include <stdarg.h>

#include "eka_data_structs.h"
#include "eka.h"
#include "ctls.h"
//#include "eka_fh.h"

#include "eka_user_channel.h"
#include "eka_dev.h"

#include "EfcCtxs.h"
#include "EfcMsgs.h"
#include "Efc.h"

void eka_set_affinity(EkaDev* pEkaDev, uint8_t gr_id);
//void efc_run_thread(EkaDev* pEkaDev, struct EfcRunCtx* pEfcRunCtx); // Used for FIRE REPORTS and FAST PATH 0xf0208 (HB counter) 0xe0218>>32 MASK 32(Fire counter) 
struct pico_socket* session2picosock (EkaDev* pEkaDev, uint16_t id);
void cb_tcpclient(uint16_t ev, struct pico_socket *s);
void ekaExcInit (EkaDev* dev);
void download_subscr_table(EkaDev* dev, bool clear_all);
eka_add_conf_t eka_conf_parse(eka_dev_t* dev, eka_conf_type_t conf_type, const char *key, const char *value);
uint32_t get_subscription_id(EkaDev* dev, uint64_t id);
void eka_enable_cores(eka_dev_t* dev);

int decode_session_id (uint16_t id, uint8_t* core, uint8_t* sess);
bool eka_is_all_zeros (const void* buf, ssize_t size);
uint8_t session2core (uint16_t id);
uint8_t session2sess (uint16_t id);
uint16_t encode_session_id (uint8_t core, uint8_t sess);
uint16_t socket2session (eka_dev_t* dev, int sock_fd);
void eka_write(EkaDev* dev, uint64_t addr, uint64_t val);
uint64_t eka_read(eka_dev_t* dev, uint64_t addr);
bool find_tcp_core_sess_tx (EkaDev* dev, uint8_t* pkt, uint8_t* core, uint8_t* sess);
int send_dummy_pkt(EkaDev* dev,uint8_t core, uint8_t sess, uint8_t* pkt,uint size);

uint16_t eka_create_packet (EkaDev* dev, uint8_t* pkt_buf, uint8_t core, uint8_t sess, uint8_t* data, size_t data_len);
void prepare_fire_report(fire_report_t *target, fire_report_t *source);
void ekaLwipPollThread (EkaDev* dev);
void ekaExcInitLwip (EkaDev* dev);
void hexDump (const char* desc, void *addr, int len);
bool eka_get_uch_pkt(EkaDev* dev, uint8_t** ptr, uint32_t* pLen);
void eka_next_uch_pkt(EkaDev* dev);
void ekaProcesTcpRx (EkaDev* dev, uint8_t* buf, uint32_t len);



/* uint16_t eka_get_window_size (EkaDev* dev, uint8_t core, uint8_t sess) { */
/*   int fd = SN_GetFileDescriptor(dev->dev_id); */
/*   eka_ioctl_t state = {}; */
/*   state.cmd = EKA_DUMP; */
/*   state.nif_num = core; */
/*   state.session_num = sess; */
/*   int rc = ioctl(fd,SC_IOCTL_EKALINE_DATA,&state); */
/*   if (rc < 0) on_error("error ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) EKA_DUMP"); */
/*   return state.eka_session.tcp_window; */
/* } */


int four_tuples2sock (EkaDev* dev,struct in_addr* src, struct in_addr* dst,uint16_t sport,uint16_t dport) {
  for (int c=0;c<dev->hw.enabled_cores;c++) {
    for (int s=0;s<EKA_MAX_TCP_SESSIONS_PER_CORE;s++) {
      //      EKA_LOG("sport 0x%x (db 0x%x) dport 0x%x (db 0x%x)",sport,dev->core[c].tcp_sess[s].src.sin_port,dport,dev->core[c].tcp_sess[s].dst.sin_port);
      if (src->s_addr == dev->core[c].tcp_sess[s].src_ip && dst->s_addr == dev->core[c].tcp_sess[s].dst_ip && 
	  sport == dev->core[c].tcp_sess[s].src_port && dport == dev->core[c].tcp_sess[s].dst_port) 
	return dev->core[c].tcp_sess[s].sock_fd;
    }
  }
  return -1;
}

uint16_t  four_tuples2session (EkaDev* dev,struct in_addr* src, struct in_addr* dst,uint16_t sport,uint16_t dport) {
  for (int c=0;c<dev->hw.enabled_cores;c++) {
    for (int s=0;s<EKA_MAX_TCP_SESSIONS_PER_CORE;s++) {
      //      EKA_LOG("sport 0x%x (db 0x%x) dport 0x%x (db 0x%x)",sport,dev->core[c].tcp_sess[s].src.sin_port,dport,dev->core[c].tcp_sess[s].dst.sin_port);
      if (src->s_addr == dev->core[c].tcp_sess[s].src_ip && dst->s_addr == dev->core[c].tcp_sess[s].dst_ip && 
	  sport == dev->core[c].tcp_sess[s].src_port && dport == dev->core[c].tcp_sess[s].dst_port) 
	return encode_session_id (c,s);
    }
  }
  on_error ("Not found");
  return 0xFFFF;
}

void create_equote_msg (EkaDev* dev, fire_report_t* fire_report, struct equote_fire* equote_msg, uint8_t c, uint8_t sess) {
  equote_msg->ses.length = sizeof(equote_msg->ses.type) + sizeof(struct equote);
  equote_msg->ses.type = 'U';
  equote_msg->eq.message_type[0] = 'E';
  equote_msg->eq.message_type[1] = 'Q';
  equote_msg->eq.client_message_id = bswap_32(dev->core[c].tcp_sess[sess].app_ctx.clid);
  equote_msg->eq.mpid = bswap_32(dev->core[c].tcp_sess[sess].app_ctx.equote_mpid_sqf_badge);
  equote_msg->eq.product_id = fire_report->normalized_report.security_id;
  equote_msg->eq.action = 'N';
  equote_msg->eq.type = 'I';
  equote_msg->eq.event_id = 0;
  equote_msg->eq.target_message_id = 0;
  equote_msg->eq.price = fire_report->normalized_report.ei_price;
  equote_msg->eq.size = fire_report->normalized_report.ei_size;
  equote_msg->eq.side = ((int8_t)fire_report->normalized_report.side == EFH_ORDER_ASK) ? 'B' : 'A';
  return;
}

void create_sqf_msg (EkaDev* dev, fire_report_t* fire_report, struct sqf_fire* sqf_msg, uint8_t c, uint8_t sess) {
  sqf_msg->sb.length = sizeof(struct sqf) + sizeof(struct soupbin) - sizeof(sqf_msg->sb.length);
  sqf_msg->sb.type = 'U';
  sqf_msg->sq.type_sub[0] = 'Q';
  sqf_msg->sq.type_sub[1] = 'Q';
  sqf_msg->sq.badge = bswap_32(dev->core[c].tcp_sess[sess].app_ctx.equote_mpid_sqf_badge);
  sqf_msg->sq.message_id = bswap_64(dev->core[c].tcp_sess[sess].app_ctx.clid);
  sqf_msg->sq.quote_count = bswap_16(2);
  sqf_msg->sq.option_id1 =  bswap_32(fire_report->normalized_report.security_id);
  if ((int8_t)fire_report->normalized_report.side == EFH_ORDER_ASK) {
    sqf_msg->sq.bid_price1 = bswap_32(100 * (uint32_t)fire_report->secctx_report.ask_max_price);
    sqf_msg->sq.bid_size1  = bswap_32((uint32_t)fire_report->secctx_report.size);
    sqf_msg->sq.ask_price1 = bswap_32((uint32_t)(100 *(500 + fire_report->secctx_report.ask_max_price)));
    sqf_msg->sq.ask_size1 = 10;
  } else {
    sqf_msg->sq.ask_price1 = bswap_32(100 * (uint32_t)(fire_report->secctx_report.bid_min_price));
    sqf_msg->sq.ask_size1  = bswap_32((uint32_t)fire_report->secctx_report.size);
    if (fire_report->secctx_report.bid_min_price <= 500) {
      sqf_msg->sq.bid_price1 = bswap_32((uint32_t)(0)); 
      sqf_msg->sq.bid_size1  = bswap_32((uint32_t)(0));
    } else {
      sqf_msg->sq.bid_price1 = bswap_32((uint32_t)(100 * (fire_report->secctx_report.bid_min_price - 500)));
      sqf_msg->sq.bid_size1 = bswap_32((uint32_t)(10));
    }
  }
  sqf_msg->sq.reentry1 = 'R';
  /* sqf_msg->sq.option_id2 = sqf_msg->sq.option_id1; */
  /* sqf_msg->sq.bid_price2 = 0; */
  /* sqf_msg->sq.bid_size2 = 0; */
  /* sqf_msg->sq.ask_price2 = 0; */
  /* sqf_msg->sq.ask_size2 = 0; */
  /* sqf_msg->sq.reentry2 = 'R'; */
  return;
}

ssize_t eka_get_new_fire_report(EkaDev* dev, uint8_t* buf, fire_report_t* source) {
  //  hexDump("eka_get_new_fire_report",source,96);
  uint8_t* b =  buf;
  EKA_LOG("b=%p, buf=%p, b-buf = %ld",b,buf,b-buf);
  bool exception_report = (source->normalized_report).fire_reason & 0x10; // * FIRE_REASON_EXCEPTION    4th bit
  if (exception_report) {
    ((EkaContainerGlobalHdr*)b)->type = EkaEventType::kExceptionReport;
    ((EkaContainerGlobalHdr*)b)->num_of_reports = 0;
    return sizeof(EkaContainerGlobalHdr);
  }

  ((EkaContainerGlobalHdr*)b)->type = EkaEventType::kFireReport;
  //((EkaContainerGlobalHdr*)buf)->num_of_reports is set at the end
  b += sizeof(EkaContainerGlobalHdr);
  EKA_LOG("sizeof(((EkaContainerGlobalHdr*)b)->type)=%ju, sizeof(EkaContainerGlobalHdr)=%ju",sizeof(((EkaContainerGlobalHdr*)b)->type),sizeof(EkaContainerGlobalHdr));
  //--------------------------------------------------------------------------

  ((EfcReportHdr*)b)->type = EfcReportType::kControllerState;
  ((EfcReportHdr*)b)->idx = 1;
  ((EfcReportHdr*)b)->size = 1; // 1 byte for uint8_t unarm_reson;
  b += sizeof(EfcReportHdr);

  ((EfcControllerState*)b)->unarm_reason = source->normalized_report.last_unarm_reason;
  b += sizeof(EfcControllerState);
  //--------------------------------------------------------------------------
  ((EfcReportHdr*)b)->type = EfcReportType::kMdReport;
  ((EfcReportHdr*)b)->idx = 2;
  ((EfcReportHdr*)b)->size = sizeof(EfcMdReport);
  b += sizeof(EfcReportHdr);

  uint64_t md_seconds;
  uint64_t md_nanoseconds;
  uint64_t md_final_nanoseconds;
  switch (dev->hw.feed_ver) {
  case SN_NASDAQ:
  case SN_GEMX:      
  case SN_MIAX:
    ((EfcMdReport*)b)->timestamp = be64toh(source->normalized_report.md_timestamp);
    break;
  case SN_PHLX:
  case SN_CBOE:
    md_seconds     = be32toh((source->normalized_report.md_timestamp    ) & 0xFFFFFFFF);
    md_nanoseconds = be32toh((source->normalized_report.md_timestamp>>32) & 0xFFFFFFFF);
    md_final_nanoseconds = md_seconds*1e9 + md_nanoseconds;
    ((EfcMdReport*)b)->timestamp = md_final_nanoseconds;
    EKA_LOG("md_seconds=0x%jx md_nanoseconds=0x%jx ((MdReport*)b)->timestamp=0x%jx",md_seconds,md_nanoseconds,((EfcMdReport*)b)->timestamp);
    break;
  default:
    on_error("Unknown feed type");
  }      
  
  {
    auto msg{ reinterpret_cast< EfcMdReport* >( b ) };
    msg->sequence = be64toh(source->normalized_report.md_sequence);
    msg->side     =         source->normalized_report.side;
    msg->price    = be64toh(source->normalized_report.md_price);
    msg->size     = be64toh(source->normalized_report.md_size);
    msg->group_id =         source->normalized_report.group_id;
    msg->core_id  =         source->normalized_report.core_id; 
    b += sizeof(*msg);
  }
  //--------------------------------------------------------------------------
  {
    auto* msg{ reinterpret_cast< EfcReportHdr* >( b ) };
    msg->type = EfcReportType::kSecurityCtx;
    msg->idx  = 3;
    msg->size = sizeof(EfcSecurityCtx);
    b += sizeof(*msg);
  }

  {
    auto* msg{ reinterpret_cast< EfcSecurityCtx* >( b ) };
    msg->lower_bytes_of_sec_id =         source->secctx_report.lower_bytes_of_sec_id;
    msg->ver_num               =         source->secctx_report.ver_num;
    msg->size                  =         source->secctx_report.size;
    msg->ask_max_price         = be16toh(source->secctx_report.ask_max_price);
    msg->bid_min_price         = be16toh(source->secctx_report.bid_min_price);
    b += sizeof(*msg);
  }
  //--------------------------------------------------------------------------



  uint8_t num_of_fires = 0;
  for (int c=0;c<dev->hw.enabled_cores;c++) {
    uint8_t num_of_fires_per_core = 0;
    if (! dev->core[c].connected) continue;
    if (dev->core[c].tcp_sessions == 0) continue;
    uint8_t sess = dev->core[c].udp_sess[source->normalized_report.group_id].first_session_id;
    while (sess != 0xFF) { 
 //--------------------------------------------------------------------------
    {
      auto* msg{ reinterpret_cast< EfcReportHdr* >( b ) };
      msg->type = EfcReportType::kFireReport;
      msg->idx  = 3 + 2 * num_of_fires + 1;
      msg->size = sizeof(EfcFireReport);
      b += sizeof(*msg);
    }
     
    {
      auto* msg{ reinterpret_cast< EfcFireReport* >( b ) };
      msg->reason      =         source->normalized_report.fire_reason & (~0x10); // turning off FIRE_REASON_EXCEPTION(4th bit) for backward compat
      msg->size        = be64toh(source->normalized_report.ei_size);
      msg->price       = be64toh(source->normalized_report.ei_price);
      msg->security_id = be64toh(source->normalized_report.security_id); 
      msg->session     = c * 128 + sess;
      msg->timestamp   = be64toh(source->normalized_report.timestamp);
      b += sizeof(*msg);
    }
  //--------------------------------------------------------------------------
      switch (dev->hw.feed_ver) {
      case SN_MIAX:
        {
          auto* msg{ reinterpret_cast< EfcReportHdr* >( b ) };
          msg->type = EfcReportType::kMiaxSessionCtx;
          msg->idx  = 3 + 2 * num_of_fires + 2;
          msg->size = sizeof(EfcMiaxSessionCtx);
          b += sizeof(*msg);
        }

        {
          auto* msg{ reinterpret_cast< EfcMiaxSessionCtx* >( b ) };
          msg->fired_session = c * 128 + sess;
          msg->cl_ord_id     = dev->core[c].tcp_sess[sess].app_ctx.clid;
          b += sizeof(*msg);
        }
        break;

      case SN_NASDAQ:
      case SN_GEMX:      
      case SN_PHLX:
        {
          auto* msg{ reinterpret_cast< EfcReportHdr* >( b ) };
          msg->type = EfcReportType::kSqfSessionCtx;
          msg->idx  = 3 + 2 * num_of_fires + 2;
          msg->size = sizeof(EfcSqfSessionCtx);
          b += sizeof(*msg);
        }

        {
          auto* msg{ reinterpret_cast< EfcSqfSessionCtx* >( b ) };
          msg->fired_session = c * 128 + sess;
          msg->cl_ord_id     = dev->core[c].tcp_sess[sess].app_ctx.clid;
          b += sizeof(*msg);
        }
        break;

      case SN_CBOE: 
  // TBD BOE
        {
          auto* msg{ reinterpret_cast< EfcReportHdr* >( b ) };
          msg->type = EfcReportType::kSqfSessionCtx;
          msg->idx  = 3 + 2 * num_of_fires + 2;
          msg->size = sizeof(EfcSqfSessionCtx);
          b += sizeof(EfcReportHdr);
        }

        {
          auto* msg{ reinterpret_cast< EfcSqfSessionCtx* >( b ) };
          msg->fired_session = c * 128 + sess;
          msg->cl_ord_id     = dev->core[c].tcp_sess[sess].app_ctx.clid;
          b += sizeof(EfcSqfSessionCtx);
        }
        break;
  break;
      default:
  on_error("Unknown feed type");
      }      

  //--------------------------------------------------------------------------
      num_of_fires ++;
      num_of_fires_per_core++;
      sess = dev->core[0].tcp_sess[sess].app_ctx.next_session;
      if (be64toh(source->normalized_report.md_size) > MIN_SIZE_TO_ONLY_FIRE_ONCE) sess = 0xFF;
      if (be64toh(source->normalized_report.md_size) > source->secctx_report.size) sess = 0xFF;
    }
    if (num_of_fires_per_core != source->normalized_report.fire_counter) 
      on_error("LinkList num_of_fires_per_core %d != source->normalized_report.fire_counter %d",num_of_fires_per_core,source->normalized_report.fire_counter);
    if (be64toh(source->normalized_report.md_size) > MIN_SIZE_TO_ONLY_FIRE_ONCE) break; // No Fire reports for Core#1 and higher
    if (be64toh(source->normalized_report.md_size) > source->secctx_report.size) break; // No Fire reports for Core#1 and higher

  } 
  
  ((EkaContainerGlobalHdr*)buf)->num_of_reports = 3 + 2 * num_of_fires;
  ssize_t s = b-buf;
  EKA_LOG("b=%p, buf=%p, b-buf = %ld, ssize_t s = %ld",b,buf,b-buf,s);

  //  return ((uint8_t*)b - (uint8_t*)buf);
  return (b - buf);
}

size_t eka_get_fire_report(EkaDev* dev, uint8_t* buf) {
  if (! dev->fr_ch->has_data()) on_error ("No Fire report at the FR Ch");
  const uint8_t* payload = dev->fr_ch->get();
  if (dev->fr_ch->get_payload_size() != sizeof(fire_report_t)) on_error ("accepted Fire Report of %u bytes != expected %ju bytes",dev->fr_ch->get_payload_size(),sizeof(fire_report_t));
  dev->fr_ch->next();
  EKA_LOG("Fire Report!");
  return  eka_get_new_fire_report(dev, buf, (fire_report_t*)payload);
}

bool eka_check_exceptions (EkaDev* dev) {

    uint64_t var_global_shadow = eka_read(dev,EKA_ADDR_INTERRUPT_SHADOW_RO);
    if (var_global_shadow) {
      EKA_LOG("FATAL ERROR: FPGA Interrupts discovered: interrupt vector = 0x%016jx",var_global_shadow);
      char exception_report_buf[256];
      sprintf (exception_report_buf,"FPGA Interrupts discovered: interrupt vector = 0x%016jx",var_global_shadow);
      size_t report_size = sizeof(exception_report_buf);
      dev->pEfcRunCtx->onEkaExceptionReportCb(
					      dev->pEfcCtx,
					      reinterpret_cast< EkaExceptionReport* >( exception_report_buf ), 
					      report_size );
      return true;
    }
    return false;
}

void efc_run_thread(struct EfcCtx* pEfcCtx, struct EfcRunCtx* pEfcRunCtx) { // Used for FIRE REPORTS and FAST PATH 0xf0208 (HB counter) 0xe0218>>32 MASK 32(Fire counter) 
  assert (pEfcCtx != NULL);
  EkaDev* dev = pEfcCtx->dev;
  assert (dev != NULL);

  uint32_t fire_counter = 0;
  dev->efc_run_threadIsUp = true;
  EKA_LOG("efc_run_threadIsUp");
  fflush(stderr);

  (dev->serv_thr).active = true;

  while (dev->serv_thr.active) {
    if (eka_check_exceptions(dev)) on_error("Pizdets");
    uint8_t* data;
    uint32_t len;
    if (! eka_get_uch_pkt(dev, &data, &len)) continue;

    // Sanity check
    if (((((dma_report_t*)data)->type == FAST_PATH) || (((dma_report_t*)data)->type == FPGA_FIRE)) &&
	(((dma_report_t*)data)->subtype == 0xFF && be16toh(((dma_report_t*) data)->length) != len - sizeof(dma_report_t))) {
      on_error("For DMAtype %u and Subtype 0x%x DMAlength %u doesnt match Data length %u",((dma_report_t*)data)->type,((dma_report_t*)data)->subtype,((dma_report_t*) data)->length,len);
      hexDump("Bad RX data",data,len);
    }

    if (((((dma_report_t*)data)->type == FAST_PATH) || (((dma_report_t*)data)->type == FPGA_FIRE)) && ((dma_report_t*)data)->subtype == 0xFF) {
      if (be16toh(((dma_report_t*) data)->length) != len - sizeof(dma_report_t)) {
	hexDump("Bad RX data",data,len);
	on_error("For DMAtype %u and Subtype 0x%x DMAlength %u doesnt match Data length %u",((dma_report_t*)data)->type,((dma_report_t*)data)->subtype,((dma_report_t*) data)->length,len);
      }
      switch (((dma_report_t*) data)->type) {
  
      case FAST_PATH: {
	uint8_t core,sess;
	uint8_t* pkt = (uint8_t*)data + sizeof(dma_report_t);
	if (! find_tcp_core_sess_tx(dev,pkt,&core,&sess)) on_error ("Unknown Fast Path session ");//to: %s:%u",inet_ntoa(*(struct addr_in*) & EKA_IPH_SRC(pkt)), EKA_TCPH_DST(pkt));   
#if 0
	EKA_LOG("FAST_PATH pkt EKA_TCP_PAYLOAD_LEN(pkt) = %u, EKA_TCPH_PAYLOAD(pkt)=%d,EKA_TCPH_HDRLEN_BYTES(EKA_TCPH(pkt))=%u, EKA_TCPH(pkt) - pkt = %d",
		EKA_TCP_PAYLOAD_LEN(pkt),
		(int)(EKA_TCPH_PAYLOAD(pkt) - pkt),
		EKA_TCPH_HDRLEN_BYTES(EKA_TCPH(pkt)),
		(int)((uint8_t*)EKA_TCPH(pkt) - pkt)
		);
	hexDump("efc_run_thread: FAST_PATH pkt",EKA_TCPH_PAYLOAD(pkt),EKA_TCP_PAYLOAD_LEN(pkt));
	fflush(stderr);
	fflush(stdout);
#endif

	int rc = send_dummy_pkt(dev,core,sess,EKA_TCPH_PAYLOAD(pkt),EKA_TCP_PAYLOAD_LEN(pkt));
	dev->core[core].tcp_sess[sess].fast_path_dummy_bytes += EKA_TCP_PAYLOAD_LEN(pkt);

	if (rc != EKA_TCP_PAYLOAD_LEN(pkt)) on_error ("send_dummy_pkt for core %u sess %u tried to send %u bytes, sent %d",core,sess,EKA_TCP_PAYLOAD_LEN(pkt),rc);
	break;
      }
      
      case FPGA_FIRE:  { // 1=fire report
	fire_report_t fire_report = {};
	prepare_fire_report (&fire_report,(fire_report_t *) data);
	bool exception_report = fire_report.normalized_report.fire_reason & 0x10; // * FIRE_REASON_EXCEPTION    4th bit
	fire_counter++;
	uint8_t sw_fire_counter=0;
	for (int c=0;c<dev->hw.enabled_cores;c++) {
	  if (! dev->core[c].connected) continue;
	  if (dev->core[c].tcp_sessions == 0) continue;
	  uint8_t sess = dev->core[c].udp_sess[fire_report.normalized_report.group_id].first_session_id;
	  sw_fire_counter = 0;  
	  if (sess == 0xFF) on_error("firing on unitiliazied session, core = %d, group = %d, first_session = 0x%x",c,fire_report.normalized_report.group_id,sess);
	  while (sess != 0xFF) { 
	    struct sqf_fire sqf_msg = {};
	    struct equote_fire equote_msg = {};
	    struct boe_fire boe_msg = {};

	    uint8_t* fire_msg = NULL;
	    ssize_t fire_size = -1;

	    switch (dev->hw.feed_ver) {
	    case SN_PHLX:
	    case SN_NASDAQ:
	    case SN_GEMX:      
	      //      create_sqf_msg(dev,&fire_report,&sqf_msg,c,sess);
	      fire_msg = (uint8_t*) &sqf_msg;
	      fire_size = sizeof(struct sqf_fire);
	      break;
	    case SN_MIAX:
	      //      create_equote_msg(dev,&fire_report,&equote_msg,c,sess);
	      fire_msg = (uint8_t*) &equote_msg;
	      fire_size = sizeof(struct equote_fire);
	      break;
	    case SN_CBOE:
	      fire_msg = (uint8_t*) &boe_msg;
	      fire_size = sizeof(struct boe_fire);
	      break;
	    default:
	      on_error("Unknown feed type");
	    }      
	    if (dev->fire_params.external_params.report_only == 0 && !exception_report) {
	      int rc = send_dummy_pkt(dev,c,sess,fire_msg,fire_size);
	      if (rc != fire_size) on_error ("send_dummy_pkt tried to send %ld bytes, sent %d",fire_size,rc);
	      dev->core[c].tcp_sess[sess].fire_dummy_bytes += fire_size;

	      EKA_LOG("Fire Report: session fired: %d, sock_fd=%d, next sess=%d",sess,dev->core[c].tcp_sess[sess].sock_fd,dev->core[c].tcp_sess[sess].app_ctx.next_session);
	    }
	    if (dev->fire_params.external_params.report_only == 0 && exception_report) {
	      EKA_LOG("core: %u, sess: %u :Fire Report: exception report",c,sess);
	    }
	    if (dev->fire_params.external_params.report_only == 1 && exception_report) {
	      EKA_LOG("core: %u, sess: %u :Exception Report: Report Only",c,sess);
	    }
	    if (dev->fire_params.external_params.report_only == 1 && !exception_report) {
	      EKA_LOG("core: %u, sess: %u :Fire Report: Report Only",c,sess);
	    }
	    sess = dev->core[c].tcp_sess[sess].app_ctx.next_session;
	    if (fire_report.normalized_report.md_size > MIN_SIZE_TO_ONLY_FIRE_ONCE) sess = 0xFF;
	    if (fire_report.normalized_report.md_size > fire_report.secctx_report.size) sess = 0xFF;
	    sw_fire_counter++;
	    if (((dma_report_t*) data)->type != FPGA_FIRE) on_error("((dma_report_t*) data)->type != FPGA_FIRE");
	  } // while (sess != 0xFF) 
	  if (fire_report.normalized_report.md_size > MIN_SIZE_TO_ONLY_FIRE_ONCE) break; // Don't Fire on Core#1
	  if (fire_report.normalized_report.md_size > fire_report.secctx_report.size) break;
	} //  for (int c=0;c<dev->hw.enabled_cores;c++)
	EKA_LOG("Fire Report: first session fired: %d %d times", dev->core[0].udp_sess[fire_report.normalized_report.group_id].first_session_id,sw_fire_counter);
	if (fire_report.normalized_report.fire_counter!=sw_fire_counter && dev->fire_params.external_params.report_only==0 && !exception_report) 
	  on_error("sw fire %d times, hw fired %d times",sw_fire_counter,fire_report.normalized_report.fire_counter);

	char fire_report_buf[1000];
	size_t report_size = eka_get_fire_report(dev, (uint8_t*)fire_report_buf);
	dev->pEfcRunCtx->onEfcFireReportCb(
					   pEfcCtx,
					   //						       reinterpret_cast< EkaExceptionReport* >( fire_report_buf ), 
					   reinterpret_cast< EfcFireReport* >( fire_report_buf ), 
					   report_size );
	break;
      } 

      default: 
	on_error ("Corrupted RX data");
      }
    } else {
      ekaProcesTcpRx (dev, data, len);
    }
    dev->fp_ch->next();

    usleep(0);

  }
  EKA_LOG("Exiting");
  return;
}

void efc_fire_report_thread(struct EfcCtx* pEfcCtx, struct EfcRunCtx* pEfcRunCtx) { // Used for FIRE REPORTS and FAST PATH 0xf0208 (HB counter) 0xe0218>>32 MASK 32(Fire counter) 
  assert (pEfcCtx != NULL);
  EkaDev* dev = pEfcCtx->dev;
  assert (dev != NULL);


  dev->efc_fire_report_threadIsUp = true;
  EKA_LOG("efc_fire_report_threadIsUp");
  fflush(stderr);
  while (dev->serv_thr.active) {
    if (! dev->fr_ch->has_data()) continue;
    char fire_report_buf[1000];
    size_t report_size = eka_get_fire_report(dev, (uint8_t*)fire_report_buf);
    dev->pEfcRunCtx->onEfcFireReportCb(
				       pEfcCtx,
				       //						       reinterpret_cast< EkaExceptionReport* >( fire_report_buf ), 
				       reinterpret_cast< EfcFireReport* >( fire_report_buf ), 
				       report_size );
    usleep(0);
  }
}

#if 0
void efc_run_thread_old(struct EfcCtx* pEfcCtx, struct EfcRunCtx* pEfcRunCtx) { // Used for FIRE REPORTS and FAST PATH 0xf0208 (HB counter) 0xe0218>>32 MASK 32(Fire counter) 
  assert (pEfcCtx != NULL);
  EkaDev* dev = pEfcCtx->dev;
  assert (dev != NULL);

  uint32_t fire_counter = 0;
  dev->efc_run_threadIsUp = true;
  EKA_LOG("efc_run_threadIsUp");
  fflush(stderr);

  while (dev->serv_thr.active) {
    if (eka_check_exceptions(dev)) on_error("Pizdets");

    if (! dev->fp_ch->has_data()) continue;
    const uint8_t* data = dev->fp_ch->get();
    uint8_t* pkt = (uint8_t*)data + sizeof(dma_report_t);

    switch (((dma_report_t*) data)->type) {
  
    case FAST_PATH: {
      uint8_t core,sess;
      if (! find_tcp_core_sess_tx(dev,pkt,&core,&sess)) on_error ("Unknown Fast Path session ");//to: %s:%u",inet_ntoa(*(struct addr_in*) & EKA_IPH_SRC(pkt)), EKA_TCPH_DST(pkt));   
#if 1
      EKA_LOG("FAST_PATH pkt EKA_TCP_PAYLOAD_LEN(pkt) = %u, EKA_TCPH_PAYLOAD(pkt)=%d,EKA_TCPH_HDRLEN_BYTES(EKA_TCPH(pkt))=%u, EKA_TCPH(pkt) - pkt = %d",
      	      EKA_TCP_PAYLOAD_LEN(pkt),
      	      (int)(EKA_TCPH_PAYLOAD(pkt) - pkt),
      	      EKA_TCPH_HDRLEN_BYTES(EKA_TCPH(pkt)),
      	      (int)((uint8_t*)EKA_TCPH(pkt) - pkt)
      	      );
      hexDump("efc_run_thread: FAST_PATH pkt",EKA_TCPH_PAYLOAD(pkt),EKA_TCP_PAYLOAD_LEN(pkt));
      fflush(stderr);
      fflush(stdout);
#endif

      int rc = send_dummy_pkt(dev,core,sess,EKA_TCPH_PAYLOAD(pkt),EKA_TCP_PAYLOAD_LEN(pkt));
      if (rc != EKA_TCP_PAYLOAD_LEN(pkt)) on_error ("send_dummy_pkt for core %u sess %u tried to send %u bytes, sent %d",core,sess,EKA_TCP_PAYLOAD_LEN(pkt),rc);
      break;
    }
      
    case FPGA_FIRE:  { // 1=fire report
      fire_report_t fire_report = {};
      prepare_fire_report (&fire_report,(fire_report_t *) data);
      bool exception_report = fire_report.normalized_report.fire_reason & 0x10; // * FIRE_REASON_EXCEPTION    4th bit
      fire_counter++;
      uint8_t sw_fire_counter=0;
      for (int c=0;c<dev->hw.enabled_cores;c++) {
	if (! dev->core[c].connected) continue;
	if (dev->core[c].tcp_sessions == 0) continue;
	uint8_t sess = dev->core[c].udp_sess[fire_report.normalized_report.group_id].first_session_id;
	sw_fire_counter = 0;  
	if (sess == 0xFF) on_error("firing on unitiliazied session, core = %d, group = %d, first_session = 0x%x",c,fire_report.normalized_report.group_id,sess);
	while (sess != 0xFF) { 
	  struct sqf_fire sqf_msg = {};
	  struct equote_fire equote_msg = {};
	  struct boe_fire boe_msg = {};

	  uint8_t* fire_msg = NULL;
	  ssize_t fire_size = -1;

	  switch (dev->hw.feed_ver) {
	  case SN_PHLX:
	  case SN_NASDAQ:
	  case SN_GEMX:      
	    //      create_sqf_msg(dev,&fire_report,&sqf_msg,c,sess);
	    fire_msg = (uint8_t*) &sqf_msg;
	    fire_size = sizeof(struct sqf_fire);
	    break;
	  case SN_MIAX:
	    //      create_equote_msg(dev,&fire_report,&equote_msg,c,sess);
	    fire_msg = (uint8_t*) &equote_msg;
	    fire_size = sizeof(struct equote_fire);
	    break;
	  case SN_CBOE:
	    fire_msg = (uint8_t*) &boe_msg;
	    fire_size = sizeof(struct boe_fire);
	    break;
	  default:
	    on_error("Unknown feed type");
	  }      
	  if (dev->fire_params.external_params.report_only == 0 && !exception_report) {
	    int rc = send_dummy_pkt(dev,c,sess,fire_msg,fire_size);
	    if (rc != fire_size) on_error ("send_dummy_pkt tried to send %ld bytes, sent %d",fire_size,rc);

	    EKA_LOG("Fire Report: session fired: %d, sock_fd=%d, next sess=%d",sess,dev->core[c].tcp_sess[sess].sock_fd,dev->core[c].tcp_sess[sess].app_ctx.next_session);
	  }
	  if (dev->fire_params.external_params.report_only == 0 && exception_report) {
	    EKA_LOG("core: %u, sess: %u :Fire Report: exception report",c,sess);
	  }
	  if (dev->fire_params.external_params.report_only == 1 && exception_report) {
	    EKA_LOG("core: %u, sess: %u :Exception Report: Report Only",c,sess);
	  }
	  if (dev->fire_params.external_params.report_only == 1 && !exception_report) {
	    EKA_LOG("core: %u, sess: %u :Fire Report: Report Only",c,sess);
	  }
	  sess = dev->core[c].tcp_sess[sess].app_ctx.next_session;
	  if (fire_report.normalized_report.md_size > MIN_SIZE_TO_ONLY_FIRE_ONCE) sess = 0xFF;
          if (fire_report.normalized_report.md_size > fire_report.secctx_report.size) sess = 0xFF;
	  sw_fire_counter++;
	  if (((dma_report_t*) data)->type != FPGA_FIRE) on_error("((dma_report_t*) data)->type != FPGA_FIRE");
	} // while (sess != 0xFF) 
	if (fire_report.normalized_report.md_size > MIN_SIZE_TO_ONLY_FIRE_ONCE) break; // Don't Fire on Core#1
        if (fire_report.normalized_report.md_size > fire_report.secctx_report.size) break;
      } //  for (int c=0;c<dev->hw.enabled_cores;c++)
      EKA_LOG("Fire Report: first session fired: %d %d times", dev->core[0].udp_sess[fire_report.normalized_report.group_id].first_session_id,sw_fire_counter);
      if (fire_report.normalized_report.fire_counter!=sw_fire_counter && dev->fire_params.external_params.report_only==0 && !exception_report) 
	on_error("sw fire %d times, hw fired %d times",sw_fire_counter,fire_report.normalized_report.fire_counter);

      char fire_report_buf[1000];
      size_t report_size = eka_get_fire_report(dev, (uint8_t*)fire_report_buf);
      dev->pEfcRunCtx->onEfcFireReportCb(
					 pEfcCtx,
					 //						       reinterpret_cast< EkaExceptionReport* >( fire_report_buf ), 
					 reinterpret_cast< EfcFireReport* >( fire_report_buf ), 
					 report_size );
      break;
    } 

    default: on_error ("Unexpected EFC descriptor");
    }
    dev->fp_ch->next();

    usleep(0);

  }
  EKA_LOG("Exiting");
  return;
}
#endif // 0

void prepare_fire_report(fire_report_t *target, fire_report_t *source) {

  // DMA Report
  (target->dma_report).length  = be16toh(source->dma_report.length);
  (target->dma_report).type    =         source->dma_report.type;
  (target->dma_report).subtype =         source->dma_report.subtype;
  (target->dma_report).core_id =         source->dma_report.core_id;
  
  // Normalized Report
  (target->normalized_report).fired_cores       =         source->normalized_report.fired_cores;
  (target->normalized_report).core_id           =         source->normalized_report.core_id;
  (target->normalized_report).group_id          =         source->normalized_report.group_id;
  (target->normalized_report).md_size           = be64toh(source->normalized_report.md_size);
  (target->normalized_report).md_price          = be64toh(source->normalized_report.md_price);
  (target->normalized_report).md_sequence       = be64toh(source->normalized_report.md_sequence);
  (target->normalized_report).md_timestamp      = be64toh(source->normalized_report.md_timestamp);
  (target->normalized_report).fire_counter      =         source->normalized_report.fire_counter;
  (target->normalized_report).last_unarm_reason =         source->normalized_report.last_unarm_reason;
  (target->normalized_report).fire_reason       =         source->normalized_report.fire_reason;
  (target->normalized_report).side              =         source->normalized_report.side;
  (target->normalized_report).ei_size           = be64toh(source->normalized_report.ei_size);
  (target->normalized_report).ei_price          = be64toh(source->normalized_report.ei_price);
  (target->normalized_report).security_id       = be64toh(source->normalized_report.security_id);
  (target->normalized_report).timestamp         = be64toh(source->normalized_report.timestamp);

  // Security Context Report
  (target->secctx_report).lower_bytes_of_sec_id                     =         source->secctx_report.lower_bytes_of_sec_id;
  (target->secctx_report).ver_num                                   =         source->secctx_report.ver_num;
  (target->secctx_report).size  =         source->secctx_report.size;
  (target->secctx_report).ask_max_price                             = be16toh(source->secctx_report.ask_max_price);
  (target->secctx_report).bid_min_price                             = be16toh(source->secctx_report.bid_min_price);

  // Session Context Report
  (target->sessionctx_report).equote_mpid_sqf_badge = be32toh(source->sessionctx_report.equote_mpid_sqf_badge);
  (target->sessionctx_report).next_session          =         source->sessionctx_report.next_session;
  (target->sessionctx_report).clid                  = be64toh(source->sessionctx_report.clid);

}

void print_new_compat_fire_report (EfcCtx* pEfcCtx, EfcReportHdr* p) {
  assert (pEfcCtx != NULL);
  EkaDev* dev = pEfcCtx->dev;
  assert (dev != NULL);

  EKA_LOG("\n\n\n");
  EKA_LOG("NEW COMPAT REPORT:");

  uint8_t* b = (uint8_t*)p;
  if (((EkaContainerGlobalHdr*)b)->type == EkaEventType::kExceptionReport) {
    EKA_LOG("EXCEPTION_REPORT");
    return;
  }
  if (((EkaContainerGlobalHdr*)b)->type != EkaEventType::kFireReport) on_error("UNKNOWN Event report: 0x%02x",static_cast< uint32_t >( ((EkaContainerGlobalHdr*)b)->type ) );

  EKA_LOG("\tTotal reports: %u",((EkaContainerGlobalHdr*)b)->num_of_reports);
  uint total_reports = ((EkaContainerGlobalHdr*)b)->num_of_reports;
  b += sizeof(EkaContainerGlobalHdr);
  //--------------------------------------------------------------------------
  if (((EfcReportHdr*)b)->type != EfcReportType::kControllerState) on_error("EfcControllerState report expected, 0x%02x received, sizeof(((EfcReportHdr*)b)->type = %ju)",static_cast< uint32_t >( ((EfcReportHdr*)b)->type ),sizeof(((EfcReportHdr*)b)->type));
  EKA_LOG("\treport_type = %u EfcControllerState, idx=%u, size=%ju",static_cast< uint32_t >( ((EfcReportHdr*)b)->type ),((EfcReportHdr*)b)->idx,((EfcReportHdr*)b)->size);
  b += sizeof(EfcReportHdr);
  EKA_LOG("\tunarm_reason=0x%02x",((EfcControllerState*)b)->unarm_reason);
  b += sizeof(EfcControllerState);
  total_reports--;
 //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kMdReport) on_error("MdReport report expected, %02x received",static_cast< uint32_t >( ((EfcReportHdr*)b)->type) );
  EKA_LOG("\treport_type = %u MdReport, idx=%u, size=%ju",static_cast< uint32_t >( ((EfcReportHdr*)b)->type ),((EfcReportHdr*)b)->idx,((EfcReportHdr*)b)->size);
  b += sizeof(EfcReportHdr);

  {
    auto msg{ reinterpret_cast< const EfcMdReport* >( b ) };
    EKA_LOG("\ttimestamp=%ju ", msg->timestamp);
    EKA_LOG("\tsequence=%ju ",  msg->sequence);
    EKA_LOG("\tside=%c ",       msg->side == 1 ? 'b' : 'a');
    EKA_LOG("\tprice = %ju ",   msg->price);
    EKA_LOG("\tsize=%ju ",      msg->size);
    EKA_LOG("\tgroup_id=%u ",   msg->group_id);
    EKA_LOG("\tcore_id=%u ",    msg->core_id);
    b += sizeof(*msg);
  }
  total_reports--;

  //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kSecurityCtx) on_error("SecurityCtx report expected, %02x received",((EfcReportHdr*)b)->type);
  EKA_LOG("\treport_type = %u SecurityCtx, idx=%u, size=%ju",((EfcReportHdr*)b)->type,((EfcReportHdr*)b)->idx,((EfcReportHdr*)b)->size);
  b += sizeof(EfcReportHdr);

  {
    auto msg{ reinterpret_cast< EfcSecurityCtx* >( b ) };
    EKA_LOG("\tlower_bytes_of_sec_id=0x%x ", msg->lower_bytes_of_sec_id);
    EKA_LOG("\tver_num=0x%x ", msg->ver_num);
    EKA_LOG("\tsize=%u ", msg->size);
    EKA_LOG("\task_max_price=%u ", msg->ask_max_price);
    EKA_LOG("\tbid_min_price=%u ", msg->bid_min_price);
    b += sizeof(*msg);
  }
  total_reports--;

  //--------------------------------------------------------------------------
  while (total_reports > 0) {
    EKA_LOG("--------------- Remaining reports: %u ------------",total_reports);
    if (((EfcReportHdr*)b)->type == kFireReport) {
      EKA_LOG("\treport_type = %u FireReport, idx=%u, size=%ju",((EfcReportHdr*)b)->type,((EfcReportHdr*)b)->idx,((EfcReportHdr*)b)->size);
      b += sizeof(EfcReportHdr);

      EKA_LOG("\treason=0x%x ",   ((EfcFireReport*)b)->reason);
      EKA_LOG("\tsize=%ju ",      ((EfcFireReport*)b)->size);
      EKA_LOG("\tprice=%ju ",     ((EfcFireReport*)b)->price);
      EKA_LOG("\tsecurity_id=%ju (0x%jx)",((EfcFireReport*)b)->security_id,((EfcFireReport*)b)->security_id);
      // Below for CBOE debug
      /* EKA_LOG("\tsecurity_id=%ju (0x%jx) (%c%c%c%c%c%c) ",((FireReport*)b)->security_id,((FireReport*)b)->security_id, */
      /*         (int)((((FireReport*)b)->security_id &0xFF00000000000000)>>56), */
      /*         (int)((((FireReport*)b)->security_id &0xFF000000000000)>>48), */
      /*         (int)((((FireReport*)b)->security_id &0xFF0000000000)>>40), */
      /*         (int)((((FireReport*)b)->security_id &0xFF00000000)>>32), */
      /*         (int)((((FireReport*)b)->security_id &0xFF000000)>>24), */
      /*         (int)((((FireReport*)b)->security_id &0xFF0000)>>16) */
      /*         ); */
      EKA_LOG("\tsession=0x%04x ", ((EfcFireReport*)b)->session);
      EKA_LOG("\ttimestamp=%ju ", ((EfcFireReport*)b)->timestamp);
      b += sizeof(EfcFireReport);
      total_reports--;


    } else if (((EfcReportHdr*)b)->type == kMiaxSessionCtx) {
      EKA_LOG("\treport_type = %u MiaxSessionCtx, idx=%u, size=%ju",((EfcReportHdr*)b)->type,((EfcReportHdr*)b)->idx,((EfcReportHdr*)b)->size);
      b += sizeof(EfcReportHdr);

      EKA_LOG("\tfired_session=0x%04x", ((EfcMiaxSessionCtx*)b)->fired_session);
      EKA_LOG("\tcl_ord_id=%ju ",            ((EfcMiaxSessionCtx*)b)->cl_ord_id);

      b += sizeof(EfcMiaxSessionCtx);
      total_reports--;

    } else if (((EfcReportHdr*)b)->type == kSqfSessionCtx) {
      EKA_LOG("\treport_type = %u SqfSessionCtx, idx=%u, size=%ju",((EfcReportHdr*)b)->type,((EfcReportHdr*)b)->idx,((EfcReportHdr*)b)->size);
      b += sizeof(EfcReportHdr);

      EKA_LOG("\tfired_session=0x%04x", ((EfcSqfSessionCtx*)b)->fired_session);
      EKA_LOG("\tcl_ord_id=%ju ",            ((EfcSqfSessionCtx*)b)->cl_ord_id);

      b += sizeof(EfcSqfSessionCtx);
      total_reports--;

    } else on_error ("Unexpected report %02x received",((EfcReportHdr*)b)->type);
  }
  return;
}

void print_new_compat_md_report (EfcCtx* pEfcCtx, EfcReportHdr* p) {
  assert (pEfcCtx != NULL);
  EkaDev* dev = pEfcCtx->dev;
  assert (dev != NULL);

  uint8_t* b = (uint8_t*)p;
  if (((EkaContainerGlobalHdr*)b)->type == EkaEventType::kExceptionReport) {
    EKA_LOG("EXCEPTION_REPORT");
    return;
  }
  if (((EkaContainerGlobalHdr*)b)->type != EkaEventType::kFireReport) on_error("UNKNOWN Event report: 0x%02x",static_cast< uint32_t >( ((EkaContainerGlobalHdr*)b)->type ) );

  uint total_reports = ((EkaContainerGlobalHdr*)b)->num_of_reports;
  b += sizeof(EkaContainerGlobalHdr);
  //--------------------------------------------------------------------------
  if (((EfcReportHdr*)b)->type != EfcReportType::kControllerState) on_error("EfcControllerState report expected, 0x%02x received, sizeof(((EfcReportHdr*)b)->type = %ju)",((EfcReportHdr*)b)->type,sizeof(((EfcReportHdr*)b)->type));
  b += sizeof(EfcReportHdr);
  b += sizeof(EfcControllerState);
  total_reports--;
 //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kMdReport) on_error("MdReport report expected, %02x received",((EfcReportHdr*)b)->type);
  b += sizeof(EfcReportHdr);

  uint64_t md_report_ts = ((EfcMdReport*)b)->timestamp;
  char md_report_side = ((EfcMdReport*)b)->side == 1 ? 'B' : 'S';
  uint64_t md_report_size = ((EfcMdReport*)b)->size;
  uint64_t md_report_price = ((EfcMdReport*)b)->price;

  b += sizeof(EfcMdReport);
  total_reports--;

  //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kSecurityCtx) on_error("SecurityCtx report expected, %02x received",((EfcReportHdr*)b)->type);
  b += sizeof(EfcReportHdr);
  b += sizeof(EfcSecurityCtx);
  total_reports--;

  //--------------------------------------------------------------------------
  uint64_t md_report_symbol = 0;
  while (total_reports > 0) {
    if (((EfcReportHdr*)b)->type == EfcReportType::kFireReport) {
      b += sizeof(EfcReportHdr);
      md_report_symbol = ((EfcFireReport*)b)->security_id;
      b += sizeof(EfcFireReport);
      total_reports--;


    } else if (((EfcReportHdr*)b)->type == EfcReportType::kMiaxSessionCtx) {
      b += sizeof(EfcReportHdr);
      b += sizeof(EfcMiaxSessionCtx);
      total_reports--;

    } else if (((EfcReportHdr*)b)->type == EfcReportType::kSqfSessionCtx) {
      b += sizeof(EfcReportHdr);
      b += sizeof(EfcSqfSessionCtx);
      total_reports--;

    } else on_error ("Unexpected report %02x received",((EfcReportHdr*)b)->type);
  }

  EKA_LOG("MDREPORT: TimeOffset: 0x%jx ", md_report_ts);
  EKA_LOG("Side: %c ", md_report_side);
  EKA_LOG("Quantity: %ju ", md_report_size);
  EKA_LOG("Symbol: 01%c%c%c%c ", (int)((md_report_symbol&0xFF0000000000)>>40),(int)((md_report_symbol&0xFF00000000)>>32),(int)((md_report_symbol&0xFF000000)>>24),(int)((md_report_symbol&0xFF0000)>>16));
  EKA_LOG("Price: %ju ", md_report_price);
  fflush(stdout);
  return;
}


int exc_check_session_for_state_and_data(int global_session_id){ // This obsolete function
  return 0;
}

void  eka_init_round_table (EkaDev* dev) {
  for (uint64_t addr = 0; addr < ROUND_2B_TABLE_DEPTH; addr++) {
    uint64_t data = 0;
    switch (dev->hw.feed_ver) {
    case SN_PHLX:
    case SN_GEMX: 
      data = (addr / 10) * 10;
      break;
    case SN_NASDAQ: 
    case SN_MIAX:
    case SN_CBOE:
      data = addr;
      break;
    default:
      on_error("Unexpected dev->hw.feed_ver = 0x%x",dev->hw.feed_ver);
    }
    eka_write (dev,ROUND_2B_ADDR,addr);
    eka_write (dev,ROUND_2B_DATA,data);
    //    EKA_LOG("%016x (%ju) @ %016x (%ju)",data,data,addr,addr);
  }
  return;
}

EkaOpResult efc_init ( EfcCtx** ppEfcCtx, EkaDev *pEkaDev, const EfcInitCtx* pEfcInitCtx ) {

  EfcCtx* pEfcCtx = (EfcCtx*) calloc(1, sizeof(EfcCtx));
  assert (pEfcCtx != NULL);
  *ppEfcCtx = pEfcCtx;
  pEfcCtx->dev = pEkaDev;
  EkaDev* dev = pEfcCtx->dev;

  EKA_LOG("EfcCtx is created");
  for (int c=0; c<dev->hw.enabled_cores; c++) {
    //    EKA_LOG("initializing %u sessions of core %u: first_session_id = 0xFF",EKA_MAX_UDP_SESSIONS_PER_CORE,c);
    for (int gr=0; gr<EKA_MAX_UDP_SESSIONS_PER_CORE; gr++) dev->core[c].udp_sess[gr].first_session_id = 0xFF;
  }
  //-----------------------------------------------------------
  eka_init_round_table(dev);
  download_subscr_table(dev,1); // clear subscribtions table in FPGA
  //-----------------------------------------------------------

  for (uint i = 0; i < pEfcInitCtx->ekaProps->numProps; i++) {
    EkaProp& ekaProp{ pEfcInitCtx->ekaProps->props[ i ] };
    EKA_LOG("%s -- %s", ekaProp.szKey,ekaProp.szVal);
    //    eka_conf(pEfcCtx->dev,ekaProp.szKey,ekaProp.szVal);
    eka_conf_parse(pEfcCtx->dev,EFC_CONF,ekaProp.szKey,ekaProp.szVal);
  }

  return EKA_OPRESULT__OK;
}

void efc_run (EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx) {
  if ((pEfcCtx->dev->pEfcRunCtx = (EfcRunCtx*)malloc(sizeof(EfcRunCtx))) == NULL) on_error ("EfcRunCtx*)malloc(sizeof(EfcRunCtx))) == NULL");  
  memcpy(pEfcCtx->dev->pEfcRunCtx,pEfcRunCtx,sizeof(EfcRunCtx));

  if ((pEfcCtx->dev->pEfcCtx = (EfcCtx*)malloc(sizeof(EfcCtx))) == NULL) on_error ("EfcCtx*)malloc(sizeof(EfcCtx))) == NULL");  
  memcpy(pEfcCtx->dev->pEfcCtx,pEfcRunCtx,sizeof(EfcCtx));

  EkaDev* dev = pEfcCtx->dev;

  dev->exc_inited = true;  
  (dev->serv_thr).active = true;

  std::thread efc_run_main(efc_run_thread,pEfcCtx,(EfcRunCtx*) pEfcRunCtx);
  efc_run_main.detach();

  std::thread fireReportThread = std::thread(efc_fire_report_thread,pEfcCtx,(EfcRunCtx*) pEfcRunCtx);
  fireReportThread.detach();

  EKA_LOG("Initializing LWIP");
  ekaExcInitLwip(dev);

  while (dev->efc_fire_report_threadIsUp == false || dev->efc_run_threadIsUp == false) {
    EKA_LOG("Waiting for ekaLwipPollThreadIsUp (%d) and efc_run_threadIsUp (%d)",dev->efc_fire_report_threadIsUp,dev->efc_run_threadIsUp);
    usleep(1);
  }
  return;
}


//enum {TCP_WINDOW_STATIC = 16*1024/128}; // 128 = scale factor
//enum {TCP_WINDOW_STATIC = 0xd836};


int eka_set_security_ctx (EkaDev* dev,uint64_t id, struct sec_ctx *ctx, uint8_t ch) {
  assert (ctx != NULL);
  uint32_t ctx_idx = get_subscription_id(dev, id);
  if (ctx_idx == 0xFFFFFFFF) on_error("Security %ju is not subscribed",id);
  uint64_t ctx_wr_addr = P4_CTX_CHANNEL_BASE + ch * EKA_BANKS_PER_CTX_THREAD * EKA_WORDS_PER_CTX_BANK * 8 + dev->thread[ch].bank * EKA_WORDS_PER_CTX_BANK * 8;
  for (int i=0; i<EKA_WORDS_PER_CTX_BANK; i++) eka_write(dev,ctx_wr_addr, ((uint64_t*)ctx)[i]);
  union large_table_desc done_val = {};
  done_val.ltd.src_bank = dev->thread[ch].bank;
  done_val.ltd.src_thread = ch;
  done_val.ltd.target_idx = ctx_idx;
  eka_write(dev, P4_CONFIRM_REG, done_val.lt_desc);
  //  EKA_LOG("configured ctx_wr_addr=0x%jx ctx.lower_bytes_of_sec_id=0x%x src_thread=%d target_idx=%d bank=%d",ctx_wr_addr,ctx->lower_bytes_of_sec_id,done_val.ltd.src_thread,done_val.ltd.target_idx,dev->thread[ch].bank);
  dev->thread[ch].bank = (dev->thread[ch].bank+1) % EKA_BANKS_PER_CTX_THREAD;
  return 0; 
}

int eka_set_security_ctx_with_idx (EkaDev* dev, uint32_t ctx_idx, struct sec_ctx *ctx, uint8_t ch) {
  assert (ctx != NULL);

  //  uint32_t ctx_idx = get_subscription_id(dev, id);
  if (ctx_idx == 0xFFFFFFFF) on_error("Security is not subscribed: ctx_idx = 0x%x",ctx_idx);
  uint64_t ctx_wr_addr = P4_CTX_CHANNEL_BASE + ch * EKA_BANKS_PER_CTX_THREAD * EKA_WORDS_PER_CTX_BANK * 8 + dev->thread[ch].bank * EKA_WORDS_PER_CTX_BANK * 8;
  for (int i=0; i<EKA_WORDS_PER_CTX_BANK; i++) eka_write(dev,ctx_wr_addr, ((uint64_t*)ctx)[i]);
  union large_table_desc done_val = {};
  done_val.ltd.src_bank = dev->thread[ch].bank;
  done_val.ltd.src_thread = ch;
  done_val.ltd.target_idx = ctx_idx;
  eka_write(dev, P4_CONFIRM_REG, done_val.lt_desc);
  //  EKA_LOG("configured ctx_wr_addr=0x%jx ctx.lower_bytes_of_sec_id=0x%x src_thread=%d target_idx=%d bank=%d",ctx_wr_addr,ctx->lower_bytes_of_sec_id,done_val.ltd.src_thread,done_val.ltd.target_idx,dev->thread[ch].bank);
  dev->thread[ch].bank = (dev->thread[ch].bank+1) % EKA_BANKS_PER_CTX_THREAD;
  return 0; 
}

int eka_set_group_session(EkaDev* dev, uint8_t gr, uint16_t sess_id) {
  uint8_t sess = session2sess(sess_id);
  for (int c=0; c<dev->hw.enabled_cores; c++) {
    if (! dev->core[c].connected) continue;
    if (gr >= EKA_MAX_UDP_SESSIONS_PER_CORE) on_error("group_id %d >= EKA_MAX_UDP_SESSIONS_PER_CORE",gr);
    dev->core[c].udp_sess[gr].first_session_id = sess;
    //    EKA_LOG("setting core=%d group=%d first_session=%d",c,gr,sess);
  }
  return 0;
}


int eka_set_session_fire_app_ctx(EkaDev* dev, uint16_t sess_id , void *ctx) {
  assert (ctx != NULL);
  uint8_t core = session2core(sess_id);
  uint8_t sess = session2sess(sess_id);
  memcpy(&(dev->core[core].tcp_sess[sess].app_ctx),ctx,sizeof(session_fire_app_ctx_t));
  size_t session_ctx_size = dev->hw.feed_ver == SN_MIAX ? sizeof(struct miax_session_ctx) : sizeof(struct nasdaq_session_ctx); //TBD change to case

  for (uint i=0; i< (session_ctx_size/8 + !!(session_ctx_size%8)); i++)
    eka_write(dev,P4_SESSION_CTX_BASE + core * P4_SESSION_CTX_CORE_OFFSET + i*8, ((uint64_t*)ctx)[i]);

  union large_table_desc desc = {};
  desc.ltd.src_bank = 0;
  desc.ltd.src_thread = 0;
  desc.ltd.target_idx = sess;
  eka_write(dev, P4_SESSION_CTX_DESC + core*P4_SESSION_CTX_CORE_DESC_OFFSET, desc.lt_desc);
  return 0;
}

void eka_arm_controller(EkaDev* dev, uint8_t arm) {
  EKA_LOG("%s ", arm ? "ON" : "OFF"); 
  eka_write(dev, P4_ARM_DISARM, (uint64_t)arm); 
  //  sleep(1); 
}

int eka_recv_fire_report(EkaDev* dev, uint8_t *dest) {
  return 1;
}


/* void* igmp_sending_thread(EkaDev* dev) { */
/*   EKA_LOG("Launching..."); */
/*   dev->continue_sending_igmps = true; */
/*   while (dev->continue_sending_igmps) { */
/*     for (int s=0; s<EKA_MAX_UDP_SESSIONS_PER_CORE; s++) { */
/*       if (dev->core[0].udp_sess[s].mcast.sin_family != AF_INET) continue; // */
/*       for (int c=0; c<dev->hw.enabled_cores; c++) { */
/*   if (! dev->core[c].connected) continue; */
/*   struct ip_mreq mreq = {}; */
/*   mreq.imr_interface.s_addr = dev->core[c].src_ip; */
/*   mreq.imr_multiaddr.s_addr = dev->core[c].udp_sess[s].mcast.sin_addr.s_addr; */
/*   EKA_LOG("joining IGMP for sock_fd=%d, sess %d on core %d, from local addr %s ",dev->core[c].udp_sess[s].sock_fd,s,c,inet_ntoa(mreq.imr_interface)); */
/*   EKA_LOG("to %s:%d ",inet_ntoa(mreq.imr_multiaddr),be16toh(dev->core[c].udp_sess[s].mcast.sin_port)); */
/*   if (setsockopt(dev->core[c].udp_sess[s].sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) */
/*     on_error("fail to join IGMP for sess %d on core %d, MC addr: %s:%d",s,c,inet_ntoa( mreq.imr_multiaddr),be16toh(dev->core[c].udp_sess[s].mcast.sin_port)); */
/*       } */
/*     } */
/*     sleep(60); */
/*   } */
/*   return NULL; */
/* } */


/* int efc_open_udp_sockets(EkaDev* dev) { */
/*     /\* eka_enable_rx(dev); *\/ */
/*   for (int s=0; s<EKA_MAX_UDP_SESSIONS_PER_CORE; s++) { */
/*     if (dev->core[0].udp_sess[s].mcast.sin_family != AF_INET) continue; // */
/*     for (int c=0; c<dev->hw.enabled_cores; c++) { */
/*       if (! dev->core[c].connected) continue; */
/*       dev->core[c].udp_sessions++; */

/*       EKA_LOG("opening UDP socket for core=%d, sess=%d, %s:%d, core=%d",c,s, */
/*              inet_ntoa(dev->core[c].udp_sess[s].mcast.sin_addr),be16toh(dev->core[c].udp_sess[s].mcast.sin_port),c); */
/*       //---------------------------------------------------------- */
/*       // These sockets are for getting traffic for the Phase4 FPGA logic and sending IGMP */
/*       //---------------------------------------------------------- */
/*       if ((dev->core[c].udp_sess[s].sock_fd = socket(AF_INET,SOCK_DGRAM,0)) < 0) */
/*   on_error("cannot open UDP socket for sess %d on core %d",s,c); */
/*       EKA_LOG("dev->core[%u].udp_sess[%u].sock_fd=%d",c,s,dev->core[c].udp_sess[s].sock_fd); */
/*       int const_one = 1; */
/*       if (setsockopt(dev->core[c].udp_sess[s].sock_fd, SOL_SOCKET, SO_REUSEADDR, &const_one/\*(int){1}*\/, sizeof(int)) < 0)  */
/*   on_error("setsockopt(SO_REUSEADDR) failed"); */
/*       if (setsockopt(dev->core[c].udp_sess[s].sock_fd, SOL_SOCKET, SO_REUSEPORT, &const_one/\*(int){1}*\/, sizeof(int)) < 0)  */
/*   on_error("setsockopt(SO_REUSEPORT) failed"); */
/*       // binding UDP MC session to MC port */
/*       struct sockaddr_in local2bind = {}; */
/*       local2bind.sin_family=AF_INET; */
/*       local2bind.sin_addr.s_addr = INADDR_ANY; */
/*       local2bind.sin_port = dev->core[c].udp_sess[s].mcast.sin_port; */
/*       if (bind(dev->core[c].udp_sess[s].sock_fd,(struct sockaddr*) &local2bind, sizeof(struct sockaddr)) < 0) */
/*   on_error("cannot bind UDP socket for sess %d on core %d, to port %d",s,c,be16toh(dev->core[c].udp_sess[s].mcast.sin_port)); */

/*       //      if (dev->core[c].udp_sess[s].disable_igmp) continue; */
      
/*       struct ip_mreq mreq = {}; */
/*       mreq.imr_interface.s_addr = dev->core[c].src_ip; */
/*       mreq.imr_multiaddr.s_addr = dev->core[c].udp_sess[s].mcast.sin_addr.s_addr; */
      
/*       char local_addr_string[32] = {}; */
/*       sprintf (local_addr_string,"%s",inet_ntoa(mreq.imr_interface)); */
/*       char remote_addr_string[32] = {}; */
/*       sprintf (remote_addr_string,"%s",inet_ntoa(mreq.imr_multiaddr)); */

/*       EKA_LOG("joining IGMP for sock_fd=%d, sess %d on core %d, from local addr %s to %s:%d ", */
/* 	      dev->core[c].udp_sess[s].sock_fd,s,c,local_addr_string,remote_addr_string,be16toh(dev->core[c].udp_sess[s].mcast.sin_port)); */

/*       if (setsockopt(dev->core[c].udp_sess[s].sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) */
/*         on_error("fail to join IGMP for sess %d on core %d, MC addr: %s:%d",s,c,inet_ntoa( mreq.imr_multiaddr),be16toh(dev->core[c].udp_sess[s].mcast.sin_port)); */

/*       //      eka_write(dev,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,1); // disabling FPGA UDP traffic filter */

/*       /\* //---------------------------------------------------------- *\/ */
/*       /\* // For UDP sessions with Recovery info open UDP Channels *\/ */
/*       /\* //---------------------------------------------------------- *\/ */
/*       /\* if (c == 0 && dev->core[0].udp_sess[s].recovery.sin_family == AF_INET) { *\/ */
/*       /\*   if (!SN_IsUdpLane(dev->dev_id, c)) on_error("Lane %u is not configured as an UDP lane!",c); *\/ */

/*       /\*   SN_ChannelOptions udpChannelOptions; *\/ */
/*       /\*   SN_Error errorCode = SN_GetChannelOptions(NULL, &udpChannelOptions); *\/ */
/*       /\*   if (errorCode != SN_ERR_SUCCESS) on_error("SN_GetChannelOptions failed with errorCode = %d",errorCode); *\/ */

/*       /\*   /\\* if (dev->fh->gr[s].total_securities == 0) { *\\/ *\/ */
/*       /\*   /\\*   EKA_LOG("FH GR:%u has no subscribed Securities, setting udpChannelOptions.DoNotUseRxDMA = true",s); *\\/ *\/ */
/*       /\*   /\\*   udpChannelOptions.DoNotUseRxDMA = true; *\\/ *\/ */
/*       /\*   /\\* } else { *\\/ *\/ */
/*       /\*   /\\*   EKA_LOG("FH GR:%u has %u subscribed Securities, setting udpChannelOptions.DoNotUseRxDMA = false",s,dev->fh->gr[s].total_securities); *\\/ *\/ */
/*       /\*   /\\*   udpChannelOptions.DoNotUseRxDMA = false; *\\/ *\/ */
/*       /\*   /\\* } *\\/ *\/ */
/*       /\*   if (dev->fh->gr[s].total_securities > 0) { *\/ */
/*       /\*     eka_write(dev,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,1); // disabling FPGA UDP traffic filter *\/ */

/*       /\*     dev->core[c].udp_sess[s].udpChannelId = SN_AllocateUdpChannel(dev->dev_id, c, -1, &udpChannelOptions); *\/ */
/*       /\*     if (dev->core[c].udp_sess[s].udpChannelId == NULL) on_error("Cannot open UDP channel for core %u, sess %u",c,s); *\/ */
/*       /\*     EKA_LOG("SN_AllocateUdpChannel: dev=%p, dev->core[%u].udp_sess[%u].udpChannelId = 0x%jx",dev,c,s,dev->core[c].udp_sess[s].udpChannelId); *\/ */

/*       /\*     dev->core[c].udp_sess[s].pPreviousUdpPacket = SN_GetNextPacket(dev->core[c].udp_sess[s].udpChannelId, NULL, SN_TIMEOUT_NONE); *\/ */
/*       /\*     if (dev->core[c].udp_sess[s].pPreviousUdpPacket != NULL)  *\/ */
/*       /\*       on_error("pIncomingUdpPacket != NULL: Packet is arriving on UDP channel on core %u before any packet was sent",c); *\/ */

/*       /\*     EKA_LOG("Opening UDP channel for lane %u",c); *\/ */
    
/*       /\*     char ip[20]; *\/ */
/*       /\*     sprintf (ip, "%s",inet_ntoa(dev->core[c].udp_sess[s].mcast.sin_addr)); *\/ */
/*       /\*     errorCode = SN_IgmpJoin( *\/ */
/*       /\*           dev->core[c].udp_sess[s].udpChannelId, *\/ */
/*       /\*           c, *\/ */
/*       /\*           (const char*)ip, *\/ */
/*       /\*           be16toh(dev->core[c].udp_sess[s].mcast.sin_port), *\/ */
/*       /\*           NULL *\/ */
/*       /\*           ); *\/ */
/*       /\*     if (errorCode != SN_ERR_SUCCESS)  *\/ */
/*       /\*       on_error("Failed to join to a multicast group %s:%u, error code %d",ip,be16toh(dev->core[c].udp_sess[s].mcast.sin_port),errorCode); *\/ */
/*       /\*     EKA_LOG("IGMP joined %s:%u for HW UDP Channel on core %u, sesss %u",ip,be16toh(dev->core[c].udp_sess[s].mcast.sin_port),c,s); *\/ */
/*       /\*   } *\/ */
/*       /\* } *\/ */

      
/*     } */
/*   } */
/*   //  std::thread igmp_thread(igmp_sending_thread,dev); */
/*   //  igmp_thread.detach(); */

/*   uint64_t val = eka_read(dev, SW_STATISTICS); */
/*   uint64_t num_of_udp_ch = ((uint64_t)dev->core[0].udp_sessions)<<32; */
/*   val |= num_of_udp_ch; */
/*   eka_write(dev, SW_STATISTICS, val); */
/*   return 0; */
/* } */

void download_conf2hw (EkaDev* dev) { // can be called from exc_recv_start
  // Config for UDP Market Data IP:Port
  for (int s=0; s<EKA_MAX_UDP_SESSIONS_PER_CORE; s++) { // using UDP confs of Core#0 (assuming all cores are same)
    if (! dev->core[0].udp_sess[s].mcast_set) continue;

    EKA_LOG("configuring IP:UDP_PORT for MD for group:%d",s);
    uint32_t ip = htonl(dev->core[0].udp_sess[s].mcast_ip);
    uint16_t port = be16toh(dev->core[0].udp_sess[s].mcast_port);
    uint64_t tmp_ip   = (uint64_t) s << 32 | ip;
    uint64_t tmp_port = (uint64_t) s << 32 | dev->core[0].udp_sess[s].first_session_id << 16 | port;
    //        EKA_LOG("writing 0x%016jx (ip=%s) to addr  0x%016jx",tmp_ip,inet_ntoa(dev->core[0].udp_sess[s].mcast.sin_addr),(uint64_t)FH_GROUP_IP);
    eka_write (dev,FH_GROUP_IP,tmp_ip);
    //        EKA_LOG("writing 0x%016jx to addr  0x%016jx",tmp_port,(uint64_t)FH_GROUP_PORT);
    eka_write (dev,FH_GROUP_PORT,tmp_port);
  }

  // Config MAC_SA, MAC_DA, SRC_IP for HW Fires
  for (int c=0; c<dev->hw.enabled_cores; c++) {
    if (! dev->core[c].connected) continue;
    EKA_LOG("configuring MAC_SA, MAC_DA, SRC_IP for HW Fires: Core=%d",c);

    uint64_t write_value;
    // macda
    memcpy ((char*)&write_value+2, (char*)&dev->core[c].macda, 6);
    write_value = be64toh(write_value);
    eka_write (dev,CORE_CONFIG_BASE+CORE_CONFIG_DELTA*c+CORE_MACDA_OFFSET,write_value);
    //    EKA_LOG("writing macda 0x%016jx to addr  0x%016jx",write_value,(uint64_t)CORE_CONFIG_BASE+CORE_CONFIG_DELTA*c+CORE_MACDA_OFFSET);
    // macsa
    memcpy ((char*)&write_value+2, (char*)&dev->core[c].macsa, 6);
    write_value = be64toh(write_value);
    eka_write (dev,CORE_CONFIG_BASE+CORE_CONFIG_DELTA*c+CORE_MACSA_OFFSET,write_value);
    //    EKA_LOG("writing macsa 0x%016jx to addr  0x%016jx",write_value,(uint64_t)CORE_CONFIG_BASE+CORE_CONFIG_DELTA*c+CORE_MACSA_OFFSET);
    // src ip
    eka_write (dev,CORE_CONFIG_BASE+CORE_CONFIG_DELTA*c+CORE_SRC_IP_OFFSET,htonl(dev->core[c].src_ip));
    //    EKA_LOG("writing source ip 0x%016jx to addr  0x%016jx",(uint64_t)dev->core[c].src_ip,(uint64_t)(CORE_CONFIG_BASE+CORE_CONFIG_DELTA*c+CORE_SRC_IP_OFFSET));
  }

  // Config DST_IP, TCP SRC+DST Ports, IP CSUM per TCP session per Core for HW Fires
  for (int c=0; c<dev->hw.enabled_cores; c++) {
    for (int s=0; s<dev->core[c].tcp_sessions; s++) {
      EKA_LOG("configuring DST_IP, TCP SRC+DST Ports, IP CSUM for HW Fires on TCP session %d for Core=%d",s,c);
      hw_session_nw_header_t hw_nw_header = {};
      uint8_t empty_packet[200] = {};
      uint8_t fire_payload_length=0;
      switch(dev->hw.feed_ver) {
      case SN_GEMX:
	fire_payload_length = 40;
	break;
      case SN_NASDAQ:
	fire_payload_length = 40;
	break;
      case SN_PHLX:
	fire_payload_length = 40;
	break;
      case SN_MIAX:
	fire_payload_length = 36;
	break;
      case SN_CBOE:
	fire_payload_length = sizeof(struct boe_fire);
	break;
      }
      eka_create_packet(dev,empty_packet,c,s,empty_packet,fire_payload_length); //for cs only
      //      hw_nw_header.ip_cs = htons(((uint16_t) (((struct ip_hdr*)(empty_packet+sizeof(struct eth_hdr)))->_chksum)));
      hw_nw_header.ip_cs = EKA_IPH_CHKSUM(empty_packet);
      //      EKA_LOG("EKA_IPH_CHKSUM(empty_packet) = 0x%x",EKA_IPH_CHKSUM(empty_packet));
      hw_nw_header.dst_ip = htonl(dev->core[c].tcp_sess[s].dst_ip);
      hw_nw_header.dst_port = dev->core[c].tcp_sess[s].dst_port;
      hw_nw_header.src_port = dev->core[c].tcp_sess[s].src_port;
      for (int i=0;i<((int)sizeof(hw_session_nw_header_t)/8) + !!(sizeof(hw_session_nw_header_t)%8);i++) {
	uint64_t data = ((uint64_t*)&hw_nw_header)[i];
	uint64_t addr = HW_SESSION_NETWORK_BASE+HW_SESSION_NETWORK_CORE_OFFSET*c+i*8;
	eka_write (dev,addr,data); //data
	//  EKA_LOG("data writing: 0x%016jx to 0x%016jx",data,addr);
      }
      //        eka_write (dev,HW_SESSION_NETWORK_BASE+HW_SESSION_NETWORK_CORE_OFFSET*c+i*8,(uint64_t)((uint64_t*) &hw_nw_header + i)); //data
      
      union large_table_desc hw_nw_desc = {};
      hw_nw_desc.ltd.src_bank = 0;
      hw_nw_desc.ltd.src_thread = 0;
      hw_nw_desc.ltd.target_idx = s;
      eka_write (dev,HW_SESSION_NETWORK_DESC_BASE + HW_SESSION_NETWORK_DESC_OFFSET*c,hw_nw_desc.lt_desc); //desc
      
      usleep (10);
      /* EKA_LOG("dstip writing 0x%016jx for core %d session %d",(uint64_t)hw_nw_header.dst_ip,c,s); */
      /* EKA_LOG("dstport writing 0x%016jx for core %d session %d",(uint64_t)hw_nw_header.dst_port,c,s); */
      /* EKA_LOG("srcport writing 0x%016jx for core %d session %d",(uint64_t)hw_nw_header.src_port,c,s); */
      /* EKA_LOG("ipcs writing 0x%016jx for core %d session %d",(uint64_t)hw_nw_header.ip_cs,c,s); */
    }
  }

  // Config global fire params
  EKA_LOG("enable_strategy=%d, report_only=%d, debug_always_fire=%d, debug_always_fire_on_unsubscribed=%d, max_size=%d, watchdog_timeout_sec=%ju",
    
    dev->fire_params.external_params.enable_strategy, 
    dev->fire_params.external_params.report_only, 
    //    dev->fire_params.external_params.no_report_on_exception, 
    dev->fire_params.external_params.debug_always_fire, 
    dev->fire_params.external_params.debug_always_fire_on_unsubscribed, 
    dev->fire_params.external_params.max_size, 
    dev->fire_params.external_params.watchdog_timeout_sec);

  eka_write(dev,P4_GLOBAL_MAX_SIZE,dev->fire_params.external_params.max_size);

  uint64_t p4_strat_conf = eka_read(dev,P4_STRAT_CONF);
  p4_strat_conf = dev->fire_params.external_params.report_only == 1 ? p4_strat_conf | EKA_P4_REPORT_ONLY_BIT : p4_strat_conf;
  //  p4_strat_conf = dev->fire_params.external_params.no_report_on_exception == 1 ? p4_strat_conf | EKA_P4_NO_REPORT_ON_EXCEPTION : p4_strat_conf;
  p4_strat_conf = dev->fire_params.external_params.debug_always_fire == 1 ? p4_strat_conf | EKA_P4_ALWAYS_FIRE_BIT : p4_strat_conf;
  p4_strat_conf = dev->fire_params.external_params.debug_always_fire_on_unsubscribed == 1 ? p4_strat_conf | EKA_P4_ALWAYS_FIRE_UNSUBS_BIT : p4_strat_conf;

  p4_strat_conf = dev->fire_params.auto_rearm == 1 ? p4_strat_conf | EKA_P4_AUTO_REARM_BIT : p4_strat_conf;
  eka_write(dev,P4_STRAT_CONF,p4_strat_conf);
  p4_strat_conf = dev->fire_params.external_params.watchdog_timeout_sec == 0 ? EKA_WATCHDOG_SEC_VAL : EKA_WATCHDOG_SEC_VAL * dev->fire_params.external_params.watchdog_timeout_sec;
  eka_write(dev,P4_WATCHDOG_CONF,p4_strat_conf);

  // Config HW enables for RX and Fire per core
  eka_enable_cores(dev);
}

int eka_init_strategy_params (EkaDev* dev,const struct global_params *params) {
  memcpy(&(dev->fire_params.external_params),params,sizeof(struct global_params));
  eka_write(dev,P4_ARM_DISARM,0); 
  return 0;
}


uint32_t get_line_idx(EkaDev* dev,uint64_t id) {
  //  if (dev->hw.feed_ver == SN_MIAX) return (uint32_t) id & 0x1FFF; // Low 13 bits
  //  if (dev->hw->feed_ver == NASDAQ) return (uint32_t) if & 0x3FFF; // Low 14 bits
  return (uint32_t) id & 0x7FFF; // Low 15 bits
}

//ascii 48-57 (0..9) 65-90 (A..Z) 97-122(a..z)
bool is_ascii (uint8_t letter) {
  //  EKA_LOG("testing %d", letter);
  if ( (letter>=48 && letter<=57) || (letter>=65 && letter<=90) || (letter>=97 && letter<=122) ) return true;
  return false;
}

bool is_valid_sec_id(EkaDev* dev, uint64_t id) {
  //  EKA_LOG("testing 0x%jx", id);
  switch(dev->hw.feed_ver) {
  case SN_GEMX:
  case SN_NASDAQ:
  case SN_PHLX:
    if ((id & ~0x1FFFFFULL) != 0) return false;
    return true;
  case SN_MIAX:
    if ((id & ~0x3E00FFFFULL) != 0) return false;
    return true;
  case SN_CBOE:
    if ((id & 0xFFFF00000000ULL) != 0x303100000000 /*should be "01XXXX"*/) return false;
    if (!is_ascii( (id & 0xFFULL) >> 0))        return false;
    if (!is_ascii( (id & 0xFF00ULL) >> 8))      return false;
    if (!is_ascii( (id & 0xFF0000ULL) >> 16 ))  return false;
    if (!is_ascii( (id & 0xFF000000ULL) >> 24)) return false;
    return true;
  default:
    on_error ("Unknown dev->hw.feed_ver: %u",dev->hw.feed_ver);
  }

}

static uint16_t get_column_hash_value(EkaDev* dev, uint64_t id) {
  /* enum {NASDAQ_ACTIVE   = 0x1FFFFFULL,MIAX_ACTIVE     = 0x3E00FFFFULL}; */
  /* uint32_t active_bit_mask = (dev->hw.feed_ver == SN_MIAX) ? MIAX_ACTIVE : NASDAQ_ACTIVE; */
  /* if ((id & ~active_bit_mask) != 0) */
  /*   if (dev->hw.feed_ver == SN_MIAX)  */
  /*     EKA_LOG("MIAX PATCH WARNING Security ID 0x%016jx, masked = 0x%016jx active bits dont match HASH assumption", id, (id & ~active_bit_mask)); */
      
  /*   on_error("Security ID 0x%016jx, masked = 0x%016jx active bits dont match HASH assumption, Parser version %d", id, (id & ~active_bit_mask), dev->hw.feed_ver); */
  enum {
    EARLY_GAP  = 9,
    LATE_GAP   = 13,
    LATE_GAP2  = 14,
    LATE_GAP3  = 15,
    LOW16_MASK = 0xFFFF,
    HIGH5_MASK = 0x3E000000,
    HIGH8_MASK = 0x1FE000,
    HIGH7_MASK = 0x1FC000,
    HIGH6_MASK = 0x1F8000,
    HIGH9_MASK = 0xFF8000,
  };
  /* EKA_LOG("id=0x%x, (id & HIGH5_MASK) >> EARLY_GAP = 0x%x, id & LOW16_MASK=0x%x, (((id & HIGH5_MASK) >> EARLY_GAP) | ( id & LOW16_MASK )) & HIGH6_MASK)=0x%x, ((((id & HIGH5_MASK) >> EARLY_GAP) | ( id & LOW16_MASK )) & HIGH6_MASK) >> LATE_GAP3  =0x%x", */
  /*      */
  /*     id, */
  /*     (id & HIGH5_MASK) >> EARLY_GAP, */
  /*     id & LOW16_MASK, */
  /*     ((((id & HIGH5_MASK) >> EARLY_GAP) | ( id & LOW16_MASK )) & HIGH6_MASK), */
  /*     ((((id & HIGH5_MASK) >> EARLY_GAP) | ( id & LOW16_MASK )) & HIGH6_MASK) >> LATE_GAP3 */
  /*     ); */
  switch(dev->hw.feed_ver) {
  case SN_GEMX:
  case SN_NASDAQ:
  case SN_PHLX:
    return (id & HIGH6_MASK) >> LATE_GAP3;
  case SN_MIAX:
    return ((((id & HIGH5_MASK) >> EARLY_GAP) | ( id & LOW16_MASK )) & HIGH6_MASK) >> LATE_GAP3 ;
  case SN_CBOE:
    return (id & HIGH9_MASK) >> LATE_GAP3;
  default:
    on_error ("Unknown dev->hw.feed_ver: %u",dev->hw.feed_ver);
  }
}

uint8_t ascii_normalized(uint8_t letter) {
  if ((letter>=48) && (letter<=57)) return letter-48; // becomes in range 0..9
  if ((letter>=65) && (letter<=90)) return letter-65+10; // becomes in range 10..35
  return letter-97+36; //becomes in range 36..61
}

uint64_t eka_compact_id(EkaDev* dev, uint64_t id) {
  uint64_t compacted_id = 0;
  switch(dev->hw.feed_ver) {
  case SN_GEMX:
  case SN_NASDAQ:
  case SN_PHLX:
  case SN_MIAX:
    return id;
  case SN_CBOE:
    //    EKA_LOG("Trying to copmact 0x%jx", id);
    compacted_id = compacted_id| ascii_normalized((id & 0xFFULL)       >> 0)  <<0;
    compacted_id = compacted_id| ascii_normalized((id & 0xFF00ULL)     >> 8)  <<6;
    compacted_id = compacted_id| ascii_normalized((id & 0xFF0000ULL  ) >> 16) <<12;
    compacted_id = compacted_id| ascii_normalized((id & 0xFF000000ULL) >> 24) <<18;
    //    EKA_LOG("Compacted to 0x%jx", compacted_id);
    return compacted_id;
  default:
    on_error ("Unknown dev->hw.feed_ver: %u",dev->hw.feed_ver);
  }
}

uint64_t eka_subscr_security2fire(EkaDev* dev, uint64_t id) {
  EKA_LOG("subscribing on 0x%jx",id);
  if (! is_valid_sec_id(dev,id) && dev->hw.feed_ver == SN_MIAX) {
    EKA_LOG("MIAX PATCH WARNING Security ID 0x%016jx active bits dont match HASH assumption", id);
    return NO_ROOM_IN_HASH_LINE;
  }

  if (! is_valid_sec_id(dev,id)) on_error("Security ID 0x%016jx active bits dont match HASH assumption, Parser version %d", id, dev->hw.feed_ver);

  if (dev->subscr.sec_cnt >= EKA_MAX_P4_SUBSCR) 
    //    on_error("Max subscribtion amount %d is exceeded",dev->subscr.sec_cnt);
    return REACHED_MAX_TOTAL_SUBSCRIPTIONS;

  id = eka_compact_id(dev,id);
  uint32_t l = get_line_idx(dev, id);
  uint16_t col_hash = get_column_hash_value(dev, id);

  if (dev->subscr.line[l].valid_cntr == EKA_SUBSCR_TABLE_COLUMNS) 
    //    on_error("SecID %lx doesnt fit subscription line %d",id,l);
    return NO_ROOM_IN_HASH_LINE;

  for (int z=0; z<dev->subscr.line[l].valid_cntr; ++z) 
    if (dev->subscr.line[l].col[z] == col_hash) {
      EKA_LOG("EKALINE DEBUG: %s: sec_id = 0x%jx, line=0x%x, col=0x%x, col_hash=0x%x",id,l,z,col_hash);
      return DUPLICATE_SEC_ID;//on_error("SecID %x was already subscribed on",id);
    }
  dev->subscr.line[l].col[dev->subscr.line[l].valid_cntr] = col_hash;
  //  EKA_LOG("line = 0x%x hash = 0x%x ", l,dev->subscr.line[l].col[dev->subscr.line[l].valid_cntr]);
  dev->subscr.line[l].valid_cntr++;
  //  EKA_LOG("valid_cnt = 0x%x",dev->subscr.line[l].valid_cntr);    

  uint64_t val = eka_read(dev, SW_STATISTICS);
  val &= 0xFFFFFFFF00000000;
  val |= (uint64_t)(++dev->subscr.sec_cnt);
  eka_write(dev, SW_STATISTICS, val);
  return id;
}

uint32_t get_subscription_id(EkaDev* dev, uint64_t id) {
  id = eka_compact_id(dev,id);
  uint32_t l = get_line_idx(dev, id);
  uint16_t col_hash = get_column_hash_value(dev, id);
  EKA_LOG("id = 0x%jx line = 0x%x hash = 0x%x ",id, l,col_hash);
  for (int z=0; z<dev->subscr.line[l].valid_cntr; ++z) 
    if (dev->subscr.line[l].col[z] == col_hash) return dev->subscr.line[l].prev_sum + z;
  return 0xFFFFFFFF; // error value
}



void print_subscr_line (EkaDev* dev, int l, uint8_t *dst) {
  for (int col = 0; col < dev->subscr.line[l].valid_cntr; col++) 
    EKA_LOG("0x%x, ",dev->subscr.line[l].col[col]);
  EKA_LOG("");

  EKA_LOG("line=0x%x, valid_cnt=%d, prev_sum = %d : ",l,dev->subscr.line[l].valid_cntr,dev->subscr.line[l].prev_sum);
  for (uint8_t s_col = 0; s_col < EKA_SUBSCR_TABLE_COLUMNS; s_col ++)
    EKA_LOG("%02u:%02x ",s_col,dev->subscr.line[l].col[s_col]);
  
  EKA_LOG("\nto HW:");

  for (uint8_t d_col=0; d_col < (EKA_SUBSCR_TABLE_COLUMNS * 6 / 8) + 1 + 4; d_col ++)
    EKA_LOG("%02u:%02x ",d_col,dst[d_col]);
  EKA_LOG("");

}

void bitset_to_array (char *src_array, uint16_t bit_location) {
  uint16_t byte_offset = (bit_location / 8);
  uint8_t bit_offset = bit_location % 8;

  src_array[byte_offset] = src_array[byte_offset] | (1ULL << bit_offset);
}

void pack_subscr_line4hw_new (EkaDev* dev, int l, uint8_t *dst) {
  *(uint32_t*)dst = dev->subscr.line[l].prev_sum;
  *(dst + 3) = dev->subscr.line[l].valid_cntr;

  uint8_t hash_bits;
  switch (dev->hw.feed_ver) {
  case SN_CBOE:
    hash_bits = 9; 
    break;
  default:
    on_error("Shouldnt be called for this exchange %d",dev->hw.feed_ver);
  }      
  bool flag = false;
  for (uint8_t entry = 0; entry < EKA_SUBSCR_TABLE_COLUMNS; entry++) {
    uint16_t bit_location_offset = 4*8/*first 4 bytes are controls*/+entry*hash_bits;
    for (uint8_t bits = 0; bits < hash_bits; bits++) { //iterate through bits
      if (dev->subscr.line[l].col[entry] & (1ULL<<bits) ) {//if set
  bitset_to_array( (char *)dst, bit_location_offset+bits ); //turn on
  //  flag = true;
      }
    }
  }

  if (flag ) { 
    print_subscr_line(dev,l,dst-4);
    //    hexDump("subscr:", (char *)dst, 64);
    EKA_LOG("line = 0x%x valid_cnt = 0x%x ", l,dev->subscr.line[l].valid_cntr);
  }

}
  
void pack_subscr_line4hw (EkaDev* dev, int l, uint8_t *dst) {
  *(uint32_t*)dst = dev->subscr.line[l].prev_sum;
  *(dst + 3) = dev->subscr.line[l].valid_cntr;
  dst += 4;
  
  uint8_t s_col = 0; // source column
  uint8_t d_col = 0; // desination column
  uint8_t period_groups = (EKA_SUBSCR_TABLE_COLUMNS / 4) + (EKA_SUBSCR_TABLE_COLUMNS % 4 == 0 ? 0 : 1); //
  for (uint8_t gr_per = 0; gr_per < period_groups; gr_per++) { // running 42/4 = 11 group
    uint8_t curr0 = s_col   < EKA_SUBSCR_TABLE_COLUMNS ? (dev->subscr.line[l].col[s_col]       ) & 0x3F : 0;
    uint8_t next0 = s_col+1 < EKA_SUBSCR_TABLE_COLUMNS ? (dev->subscr.line[l].col[s_col+1] << 6) & 0xC0 : 0;
    dst[d_col] = curr0 | next0;

    uint8_t curr1 = s_col+1 < EKA_SUBSCR_TABLE_COLUMNS ? (dev->subscr.line[l].col[s_col+1] >> 2) & 0x0F : 0;
    uint8_t next1 = s_col+2 < EKA_SUBSCR_TABLE_COLUMNS ? (dev->subscr.line[l].col[s_col+2] << 4) & 0xF0 : 0;
    dst[d_col+1] = curr1 | next1;

    uint8_t curr2 = s_col+2 < EKA_SUBSCR_TABLE_COLUMNS ? (dev->subscr.line[l].col[s_col+2] >> 4) & 0x03 : 0;
    uint8_t next2 = s_col+3 < EKA_SUBSCR_TABLE_COLUMNS ? (dev->subscr.line[l].col[s_col+3] << 2) & 0xFC : 0;
    dst[d_col+2] = curr2 | next2;

    s_col += 4;
    d_col += 3;
  }
  //  print_subscr_line(dev,l,dst-4);
}


void download_subscr_table(EkaDev* dev, bool clear_all) {
  struct timespec req = {0, 1000};
  struct timespec rem = {};

  EKA_LOG("clear_all = %d",clear_all);

  dev->subscr.line[0].prev_sum = 0;
  for (int i=1; i<EKA_SUBSCR_TABLE_ROWS; i++)
    dev->subscr.line[i].prev_sum = dev->subscr.line[i-1].prev_sum + dev->subscr.line[i-1].valid_cntr;

  uint8_t hw_vals_size;

  switch (dev->hw.feed_ver) {
  case SN_NASDAQ:
  case SN_GEMX:      
  case SN_MIAX:
  case SN_PHLX:
    hw_vals_size = 5; // (42*6bit = 252bit / 8 = 32B for hash) + 1B valid + 3B counter = 35B in total
    break;
  case SN_CBOE:
    hw_vals_size = 7; // (42*9bit = 378bit / 8 = 48B for hash) + 1B valid + 3B counter = 52B in total
    break;
  default:
    on_error("Unknown feed type");
  }      

  for (int i=0; i<EKA_SUBSCR_TABLE_ROWS; i++) {
    union large_table_desc desc = {};
    desc.ltd.src_bank = 0;
    desc.ltd.src_thread = 0;
    desc.ltd.target_idx = i;


    uint64_t hw_vals[16] = {};
    switch (dev->hw.feed_ver) {
    case SN_NASDAQ:
    case SN_GEMX:      
    case SN_MIAX:
    case SN_PHLX:
      pack_subscr_line4hw(dev, i, (uint8_t *)hw_vals);
      break;
    case SN_CBOE:
      pack_subscr_line4hw_new(dev, i, (uint8_t *)hw_vals);
      break;
    default:
      on_error("Unknown feed type");
    }      
      
    for (int p=0;p<hw_vals_size;p++) { //hw_vals_size * 8Bytes
      //  EKA_LOG("%016lx (%lx)",hw_vals[p],addr);
      uint64_t val2wr = clear_all ? 0 : hw_vals[p];
      eka_write(dev, FH_SUBS_HASH_BASE + 8 * p, val2wr);
    }
    eka_write(dev, FH_SUBS_HASH_DESC, desc.lt_desc);
      
    nanosleep(&req, &rem);  // added due to "too fast" write to card
  }
  if (! clear_all) {
    uint64_t subdone = 1ULL<<63;
    uint64_t val = eka_read(dev, SW_STATISTICS);
    val |= subdone;
    eka_write(dev, SW_STATISTICS, val);
  }
}

int eka_open_udp_sockets(EkaDev* dev) {
#if 1
    /* eka_enable_rx(dev); */
  for (int s=0; s<EKA_MAX_UDP_SESSIONS_PER_CORE; s++) {
    if (! dev->core[0].udp_sess[s].mcast_set) continue; //
    for (int c=0; c<dev->hw.enabled_cores; c++) {
      if (! dev->core[c].connected) continue;
      dev->core[c].udp_sessions++;
      struct sockaddr_in mcast = {};
      mcast.sin_addr.s_addr = dev->core[c].udp_sess[s].mcast_ip;
      mcast.sin_port = dev->core[c].udp_sess[s].mcast_port;
      mcast.sin_family = AF_INET;

      EKA_LOG("opening UDP socket for core=%d, sess=%d, %s:%d, core=%d",c,s,
             inet_ntoa(mcast.sin_addr),be16toh(mcast.sin_port),c);
      //----------------------------------------------------------
      // These sockets are for getting traffic for the Phase4 FPGA logic and sending IGMP
      //----------------------------------------------------------
      if ((dev->core[c].udp_sess[s].sock_fd = socket(AF_INET,SOCK_DGRAM,0)) < 0)
  on_error("cannot open UDP socket for sess %d on core %d",s,c);
      EKA_LOG("dev->core[%u].udp_sess[%u].sock_fd=%d",c,s,dev->core[c].udp_sess[s].sock_fd);
      int const_one = 1;
      if (setsockopt(dev->core[c].udp_sess[s].sock_fd, SOL_SOCKET, SO_REUSEADDR, &const_one/*(int){1}*/, sizeof(int)) < 0) 
  on_error("setsockopt(SO_REUSEADDR) failed");
      if (setsockopt(dev->core[c].udp_sess[s].sock_fd, SOL_SOCKET, SO_REUSEPORT, &const_one/*(int){1}*/, sizeof(int)) < 0) 
  on_error("setsockopt(SO_REUSEPORT) failed");
      // binding UDP MC session to MC port
      struct sockaddr_in local2bind = {};
      local2bind.sin_family=AF_INET;
      local2bind.sin_addr.s_addr = INADDR_ANY;
      local2bind.sin_port = mcast.sin_port;
      if (bind(dev->core[c].udp_sess[s].sock_fd,(struct sockaddr*) &local2bind, sizeof(struct sockaddr)) < 0)
  on_error("cannot bind UDP socket for sess %d on core %d, to port %d",s,c,be16toh(mcast.sin_port));

      //      if (dev->core[c].udp_sess[s].disable_igmp) continue;
      
      struct ip_mreq mreq = {};
      mreq.imr_interface.s_addr = dev->core[c].src_ip;
      mreq.imr_multiaddr.s_addr = mcast.sin_addr.s_addr;
      char local_addr_string[32] = {};
      sprintf (local_addr_string,"%s",inet_ntoa(mreq.imr_interface));
      char remote_addr_string[32] = {};
      sprintf (remote_addr_string,"%s",inet_ntoa(mreq.imr_multiaddr));

      EKA_LOG("joining IGMP for sock_fd=%d, sess %d on core %d, from local addr %s to %s:%d ",
	      dev->core[c].udp_sess[s].sock_fd,s,c,local_addr_string,remote_addr_string,be16toh(mcast.sin_port));

      if (setsockopt(dev->core[c].udp_sess[s].sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
        on_error("fail to join IGMP for sess %d on core %d, MC addr: %s:%d",s,c,inet_ntoa( mreq.imr_multiaddr),be16toh(mcast.sin_port));

      //      eka_write(dev,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,1); // disabling FPGA UDP traffic filter

      /* //---------------------------------------------------------- */
      /* // For UDP sessions with Recovery info open UDP Channels */
      /* //---------------------------------------------------------- */
      /* if (c == 0 && dev->core[0].udp_sess[s].recovery.sin_family == AF_INET) { */
      /*   if (!SN_IsUdpLane(dev->dev_id, c)) on_error("Lane %u is not configured as an UDP lane!",c); */

      /*   SN_ChannelOptions udpChannelOptions; */
      /*   SN_Error errorCode = SN_GetChannelOptions(NULL, &udpChannelOptions); */
      /*   if (errorCode != SN_ERR_SUCCESS) on_error("SN_GetChannelOptions failed with errorCode = %d",errorCode); */

      /*   /\* if (dev->fh->gr[s].total_securities == 0) { *\/ */
      /*   /\*   EKA_LOG("FH GR:%u has no subscribed Securities, setting udpChannelOptions.DoNotUseRxDMA = true",s); *\/ */
      /*   /\*   udpChannelOptions.DoNotUseRxDMA = true; *\/ */
      /*   /\* } else { *\/ */
      /*   /\*   EKA_LOG("FH GR:%u has %u subscribed Securities, setting udpChannelOptions.DoNotUseRxDMA = false",s,dev->fh->gr[s].total_securities); *\/ */
      /*   /\*   udpChannelOptions.DoNotUseRxDMA = false; *\/ */
      /*   /\* } *\/ */
      /*   if (dev->fh->gr[s].total_securities > 0) { */
      /*     eka_write(dev,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,1); // disabling FPGA UDP traffic filter */

      /*     dev->core[c].udp_sess[s].udpChannelId = SN_AllocateUdpChannel(dev->dev_id, c, -1, &udpChannelOptions); */
      /*     if (dev->core[c].udp_sess[s].udpChannelId == NULL) on_error("Cannot open UDP channel for core %u, sess %u",c,s); */
      /*     EKA_LOG("SN_AllocateUdpChannel: dev=%p, dev->core[%u].udp_sess[%u].udpChannelId = 0x%jx",dev,c,s,dev->core[c].udp_sess[s].udpChannelId); */

      /*     dev->core[c].udp_sess[s].pPreviousUdpPacket = SN_GetNextPacket(dev->core[c].udp_sess[s].udpChannelId, NULL, SN_TIMEOUT_NONE); */
      /*     if (dev->core[c].udp_sess[s].pPreviousUdpPacket != NULL)  */
      /*       on_error("pIncomingUdpPacket != NULL: Packet is arriving on UDP channel on core %u before any packet was sent",c); */

      /*     EKA_LOG("Opening UDP channel for lane %u",c); */
    
      /*     char ip[20]; */
      /*     sprintf (ip, "%s",inet_ntoa(dev->core[c].udp_sess[s].mcast.sin_addr)); */
      /*     errorCode = SN_IgmpJoin( */
      /*           dev->core[c].udp_sess[s].udpChannelId, */
      /*           c, */
      /*           (const char*)ip, */
      /*           be16toh(dev->core[c].udp_sess[s].mcast.sin_port), */
      /*           NULL */
      /*           ); */
      /*     if (errorCode != SN_ERR_SUCCESS)  */
      /*       on_error("Failed to join to a multicast group %s:%u, error code %d",ip,be16toh(dev->core[c].udp_sess[s].mcast.sin_port),errorCode); */
      /*     EKA_LOG("IGMP joined %s:%u for HW UDP Channel on core %u, sesss %u",ip,be16toh(dev->core[c].udp_sess[s].mcast.sin_port),c,s); */
      /*   } */
      /* } */

      
    }
  }
  //  std::thread igmp_thread(igmp_sending_thread,dev);
  //  igmp_thread.detach();

  uint64_t val = eka_read(dev, SW_STATISTICS);
  uint64_t num_of_udp_ch = ((uint64_t)dev->core[0].udp_sessions)<<32;
  val |= num_of_udp_ch;
  eka_write(dev, SW_STATISTICS, val);
#endif
  return 0;
}




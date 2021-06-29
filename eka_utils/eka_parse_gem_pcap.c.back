#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>

#include "ekaNW.h"
#include "eka_fh_gem_messages.h"

#define on_error(...) { fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }
#define on_warning(...) { fprintf(stderr, "LOG: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n"); fflush(stdout); fflush(stderr); }

void hexDump (const char* desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    if (desc != NULL) printf("%s:\n", desc);
    if (len == 0) { printf("  ZERO LENGTH\n"); return; }
    if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) printf("  %s\n", buff);
            printf("  %04x ", i);
        }
        printf(" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
        else buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) { printf("   "); i++; }
    printf("  %s\n", buff);
}

struct pcap_file_hdr {
        uint32_t magic_number;   /* magic number */
         uint16_t version_major;  /* major version number */
         uint16_t version_minor;  /* minor version number */
         int32_t  thiszone;       /* GMT to local correction */
         uint32_t sigfigs;        /* accuracy of timestamps */
         uint32_t snaplen;        /* max length of captured packets, in octets */
         uint32_t network;        /* data link type */
 };
 struct pcap_rec_hdr {
         uint32_t ts_sec;         /* timestamp seconds */
         uint32_t ts_usec;        /* timestamp microseconds */
         uint32_t incl_len;       /* number of octets of packet saved in file */
         uint32_t orig_len;       /* actual length of packet */
 };


struct itto_grp_state {
  in_addr_t mc_ip;
  uint16_t  port;
  uint64_t  expected;
  uint64_t  ts_ns;
  uint64_t  drop_cnt;
  uint64_t  back_in_timet_cnt;
};

static struct itto_grp_state gr[] = {
  {inet_addr("233.54.12.164"),18000,0,{},0,0},
  {inet_addr("233.54.12.164"),18001,0,{},0,0},
  {inet_addr("233.54.12.164"),18002,0,{},0,0},
  {inet_addr("233.54.12.164"),18003,0,{},0,0}    
};

int get_gr_id(in_addr_t ip, uint16_t port) {
  for (int i = 0; i < (int)(sizeof(gr)/sizeof(gr[0])); i++) {
    if ((gr[i].mc_ip == ip) || (gr[i].port == port)) return i;
  }
  return -1;
}

static inline uint64_t get_ts(uint8_t* m) {
  uint64_t ts_tmp = 0;
  memcpy((uint8_t*)&ts_tmp+2,m+1,6);
  return be64toh(ts_tmp);
}


static int ts_ns2str(char* dst, uint64_t ts) {
  uint ns = ts % 1000;
  uint64_t res = (ts - ns) / 1000;
  uint us = res % 1000;
  res = (res - us) / 1000;
  uint ms = res % 1000;
  res = (res - ms) / 1000;
  uint s = res % 60;
  res = (res - s) / 60;
  uint m = res % 60;
  res = (res - m) / 60;
  uint h = res % 24;
  sprintf (dst,"%02d:%02d:%02d.%03d.%03d.%03d",h,m,s,ms,us,ns);
  return 0;
}


void parse_gem (uint64_t pkt_num, uint8_t* m, int gr, uint64_t sequence);

int main(int argc, char *argv[]) {
  uint8_t buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(struct pcap_file_hdr),1,pcap_file) != 1) on_error ("Failed to read pcap_file_hdr from the pcap file");

  uint64_t pkt_num = 1;

  while (fread(buf,sizeof(struct pcap_rec_hdr),1,pcap_file) == 1) {

    struct pcap_rec_hdr *pcap_rec_hdr_ptr = (struct pcap_rec_hdr *) buf;

    uint pkt_len = pcap_rec_hdr_ptr->orig_len;
    if (pkt_len > 1536) on_error("Probably wrong PCAP format: pkt_len = %u ",pkt_len);

    if (fread(buf,pkt_len,1,pcap_file) != 1) on_error ("Failed to read %d packet bytes",pcap_rec_hdr_ptr->orig_len);

    if (! EKA_IS_UDP_PKT(buf)) continue;
    int gr_id = get_gr_id((in_addr_t)EKA_IPH_DST(buf),EKA_UDPH_DST(buf));
    if (gr_id < 0) continue;

    uint8_t* pkt = buf + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
    uint16_t message_cnt = EKA_MOLD_MSG_CNT(pkt);
    uint64_t sequence = EKA_MOLD_SEQUENCE(pkt);

    uint8_t* msg = pkt + sizeof(struct mold_hdr);
    //    hexDump("msg",buf,pkt_len);

    for (uint m=0; m < message_cnt; m++) {
      uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)msg);
      msg += sizeof(msg_len);
      parse_gem(pkt_num, msg,gr_id,sequence++);
      msg += msg_len;
    }
    pkt_num++;
  }
  fclose(pcap_file);
}


void parse_gem (uint64_t pkt_num, uint8_t* m, int gr, uint64_t sequence) {
  //  uint64_t ts_ns = get_ts(m);
  uint64_t ts_ns = EKA_GEM_TS((m));

  char curr_ts[32] = {};
  ts_ns2str(curr_ts,ts_ns);

  char enc = (char)m[0];
  printf("%s,%ju,%c,",curr_ts,sequence,enc);

  switch (enc) {
  case 'Q':    // tqf_best_bid_and_ask_update_long
  case 'q':  { // tqf_best_bid_and_ask_update_short 
    tqf_best_bid_and_ask_update_long  *message_long  = (tqf_best_bid_and_ask_update_long *)m;
    tqf_best_bid_and_ask_update_short *message_short = (tqf_best_bid_and_ask_update_short *)m;
    bool long_form = (enc == 'Q');
    printf("%u, %u,%u,%u,%u, %u,%u,%u,%u",
	    long_form ? be32toh(message_long->option_id)          :            be32toh(message_short->option_id),

	    long_form ? be32toh(message_long->bid_price)          : (uint32_t) be16toh(message_short->bid_price) * 100,
	    long_form ? be32toh(message_long->bid_size)           : (uint32_t) be16toh(message_short->bid_size),
	    long_form ? be32toh(message_long->bid_cust_size)      : (uint32_t) be16toh(message_short->bid_cust_size),
	    long_form ? be32toh(message_long->bid_pro_cust_size)  : (uint32_t) be16toh(message_short->bid_pro_cust_size),

	    long_form ? be32toh(message_long->ask_price)          : (uint32_t) be16toh(message_short->ask_price) * 100,
	    long_form ? be32toh(message_long->ask_size)           : (uint32_t) be16toh(message_short->ask_size),
	    long_form ? be32toh(message_long->ask_cust_size)      : (uint32_t) be16toh(message_short->ask_cust_size),
	    long_form ? be32toh(message_long->ask_pro_cust_size)  : (uint32_t) be16toh(message_short->ask_pro_cust_size)
	    );
    break;
  }

  case 'A':
  case 'B':   // tqf_best_bid_or_ask_update_long
  case 'a':
  case 'b': { // tqf_best_bid_or_ask_update_short
    //    EKA_LOG("Im here");

    tqf_best_bid_or_ask_update_long  *message_long  = (tqf_best_bid_or_ask_update_long *)m;
    tqf_best_bid_or_ask_update_short *message_short = (tqf_best_bid_or_ask_update_short *)m;

    bool long_form = (enc == 'A') || (enc == 'B');
    printf("%u, %u,%u,%u,%u",
	    long_form ? be32toh(message_long->option_id)      :            be32toh(message_short->option_id),

	    long_form ? be32toh(message_long->price)          : (uint32_t) be16toh(message_short->price) * 100,
	    long_form ? be32toh(message_long->size)           : (uint32_t) be16toh(message_short->size),
	    long_form ? be32toh(message_long->cust_size)      : (uint32_t) be16toh(message_short->cust_size),
	    long_form ? be32toh(message_long->pro_cust_size)  : (uint32_t) be16toh(message_short->pro_cust_size)
	    );
    break;
  }

  case 'T': { // tqf_ticker
    tqf_ticker  *message = (tqf_ticker *)m;
    printf("%u,%u,%u,%u,%c",
	    be32toh(message->option_id),
	    be32toh(message->last_price),
	    be32toh(message->size),
	    be32toh(message->volume),
	    message->trade_condition
	    );
    break;
  }

  case 'H': { // tqf_trading_action
    tqf_trading_action  *message = (tqf_trading_action *)m;
    printf("%u,%c",
	    be32toh(message->option_id),
	    message->current_trading_state
	    );
    break;
  }

  case 'O': { // tqf_security_open_close
    tqf_security_open_close  *message = (tqf_security_open_close *)m;
    printf("%u,%c",
	    be32toh(message->option_id),
	    message->open_state
	    );
    break;
  }

  default: 
    break;
    
  }
  printf("\n");
  return;
}

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
#include "eka_fh_nom_messages.h"

#define on_error(...) { fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }
#define EFH_PRICE_SCALE 1

/* struct mold_hdr { */
/*   uint8_t       session_id[10]; */
/*   uint64_t      sequence; */
/*   uint16_t      message_cnt; */
/* } __attribute__((packed)); */

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
  {inet_addr("233.54.12.72"),18000,0,{},0,0},
  {inet_addr("233.54.12.73"),18001,0,{},0,0},
  {inet_addr("233.54.12.74"),18002,0,{},0,0},
  {inet_addr("233.54.12.75"),18003,0,{},0,0},    
  {inet_addr("233.54.12.76"),18000,0,{},0,0},
  {inet_addr("233.54.12.77"),18001,0,{},0,0},
  {inet_addr("233.54.12.78"),18002,0,{},0,0},
  {inet_addr("233.54.12.79"),18003,0,{},0,0}    
};

int get_gr_id(in_addr_t ip, uint16_t port) {
  for (int i = 0; i < (int)(sizeof(gr)/sizeof(gr[0])); i++) {
    if ((gr[i].mc_ip == ip) || (gr[i].port == port)) return i;
  }
  return -1;
}

uint64_t get_ts(uint8_t* m) {
  uint64_t ts_tmp = 0;
  memcpy((uint8_t*)&ts_tmp+2,m+3,6);
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

/* static void hexDump (const char* desc, void *addr, int len) { */
/*     int i; */
/*     unsigned char buff[17]; */
/*     unsigned char *pc = (unsigned char*)addr; */
/*     if (desc != NULL) printf ("%s:\n", desc); */
/*     if (len == 0) { printf("  ZERO LENGTH\n"); return; } */
/*     if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; } */
/*     for (i = 0; i < len; i++) { */
/*         if ((i % 16) == 0) { */
/*             if (i != 0) printf ("  %s\n", buff); */
/*             printf ("  %04x ", i); */
/*         } */
/*         printf (" %02x", pc[i]); */
/*         if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.'; */
/*         else buff[i % 16] = pc[i]; */
/*         buff[(i % 16) + 1] = '\0'; */
/*     } */
/*     while ((i % 16) != 0) { printf ("   "); i++; } */
/*     printf ("  %s\n", buff); */
/* } */

void parse_nom (uint64_t pkt_num, uint8_t* m, int gr, uint64_t sequence);

int main(int argc, char *argv[]) {
  uint8_t buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(struct pcap_file_hdr),1,pcap_file) != 1) on_error ("Failed to read pcap_file_hdr from the pcap file");

  uint64_t pkt_num = 1;

  while (fread(buf,sizeof(struct pcap_rec_hdr),1,pcap_file) == 1) {
    struct pcap_rec_hdr *pcap_rec_hdr_ptr = (struct pcap_rec_hdr *) buf;
    uint pkt_len = pcap_rec_hdr_ptr->orig_len;
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
      parse_nom(pkt_num, msg,gr_id,sequence++);
      msg += msg_len;
    }
    pkt_num++;
  }
  fclose(pcap_file);
}

void parse_nom (uint64_t pkt_num, uint8_t* m, int gr, uint64_t sequence) {
  //  uint64_t ts_ns = get_ts(m);
  uint64_t ts_ns = EKA_NOM_TS(m);
  char curr_ts[32] = {};
  ts_ns2str(curr_ts,ts_ns);

  printf ("Pkt:%8ju, GR%d, %s, SN:%16ju,%30s (%c),",pkt_num, gr,curr_ts,sequence,ITTO_NOM_MSG((char)m[0]),(char)m[0]);
  switch ((char)m[0]) {
  case 'R': //ITTO_TYPE_OPTION_DIRECTORY 
    break;
  case 'M': // END OF SNAPSHOT
    break;
  case 'a': { //NOM_ADD_ORDER_SHORT
    struct itto_add_order_short *message = (struct itto_add_order_short *)m;
    printf ("SID:%16u,OID:%16ju,%c,P:%8u,S:%8u",
	    be32toh (message->option_id),
	    be64toh (message->order_reference_delta),
	    (char)             (message->side),
	    (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->size)
	    );
    break;
  }
    //--------------------------------------------------------------
  case 'A' : { //NOM_ADD_ORDER_LONG
    struct itto_add_order_long *message = (struct itto_add_order_long *)m;
    printf ("SID:%16u,OID:%16ju,%c,P:%8u,S:%8u",
	    be32toh (message->option_id),
	    be64toh (message->order_reference_delta),
	    (char)             (message->side),
	    be32toh (message->price) / EFH_PRICE_SCALE,
	    be32toh (message->size)
	    );
    break;
  }
  case 'S': //NOM_SYSTEM_EVENT
    break;
  case 'L': //NOM_BASE_REF -- do nothing        
    break;
  case 'H': { //NOM_TRADING_ACTION 
    struct itto_trading_action *message = (struct itto_trading_action *)m;
    printf ("SID:%16u,TS:%c",be32toh(message->option_id),message->trading_state);
    break;
  }
  case 'O': { //NOM_OPTION_OPEN 
    struct itto_option_open *message = (struct itto_option_open *)m;
    printf ("SID:%16u,OS:%c",be32toh(message->option_id),message->open_state);
    break;
  }
    //--------------------------------------------------------------
  case 'J': {  //NOM_ADD_QUOTE_LONG
    //    if (!quote_mode)  break;
    struct itto_add_quote_long *message = (struct itto_add_quote_long *)m;

    printf ("SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u",
	    be32toh (message->option_id),
	    be64toh (message->bid_reference_delta),
	    be32toh (message->bid_price) / EFH_PRICE_SCALE,
	    be32toh (message->bid_size),
	    be64toh (message->ask_reference_delta),
	    be32toh (message->ask_price) / EFH_PRICE_SCALE,
	    be32toh (message->ask_size)
	    );
    break;
  }
  case 'j': { //NOM_ADD_QUOTE_SHORT
    //    if (!quote_mode)  break;
    struct itto_add_quote_short *message = (struct itto_add_quote_short *)m;
    
    printf ("SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u",
	    be32toh (message->option_id),
	    be64toh (message->bid_reference_delta),
	    (uint32_t) be16toh (message->bid_price) / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->bid_size),
	    be64toh (message->ask_reference_delta),
	    (uint32_t) be16toh (message->ask_price) / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->ask_size)
	    );

    break;
  }

  case 'E':  { //NOM_SINGLE_SIDE_EXEC
    struct itto_executed *message = (struct itto_executed *)m;
    printf ("OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->executed_contracts));

    break;
  }
    //--------------------------------------------------------------
  case 'C': { //NOM_SINGLE_SIDE_EXEC_PRICE
    struct itto_executed_price *message = (struct itto_executed_price *)m;
    printf ("OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->size));

    break;
  }

    //--------------------------------------------------------------
  case 'X': { //NOM_ORDER_CANCEL
    struct itto_order_cancel *message = (struct itto_order_cancel *)m;
    printf ("OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->cancelled_orders));

    break;
  }
    //--------------------------------------------------------------
  case 'u': {  //NOM_SINGLE_SIDE_REPLACE_SHORT
    struct itto_message_replace_short *message = (struct itto_message_replace_short *)m;
    printf ("OOID:%16ju,NOID:%16ju,P:%8u,S:%8u",
	    be64toh (message->original_reference_delta),
	    be64toh (message->new_reference_delta),
	    (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->size)
	    );
    break;
  }
    //--------------------------------------------------------------
  case 'U': { //NOM_SINGLE_SIDE_REPLACE_LONG
    struct itto_message_replace_long *message = (struct itto_message_replace_long *)m;
    printf ("OOID:%16ju,NOID:%16ju,P:%8u,S:%8u",
	    be64toh (message->original_reference_delta),
	    be64toh (message->new_reference_delta),
	    be32toh (message->price) / EFH_PRICE_SCALE,
	    be32toh (message->size)
	    );
    break;
  }
    //--------------------------------------------------------------
  case 'D': { //NOM_SINGLE_SIDE_DELETE 
    struct itto_message_delete *message = (struct itto_message_delete *)m;
    printf ("OID:%16ju",be64toh(message->reference_delta));

    break;
  }
    //--------------------------------------------------------------
  case 'G': { //NOM_SINGLE_SIDE_UPDATE
    struct itto_message_update *message = (struct itto_message_update *)m;
    printf ("OID:%16ju,P:%8u,S:%8u",
	    be64toh (message->reference_delta),
	    be32toh (message->price) / EFH_PRICE_SCALE,
	    be32toh (message->size)
	    );

    break;
  }
    //--------------------------------------------------------------
  case 'k':   {//NOM_QUOTE_REPLACE_SHORT
    struct itto_quote_replace_short *message = (struct itto_quote_replace_short *)m;
    printf ("OBOID:%16ju,NBOID:%16ju,BP:%8u,BS:%8u,OAOID:%16ju,NAOID:%16ju,AP:%8u,AS:%8u",
	    be64toh (message->original_bid_delta),
	    be64toh(message->new_bid_delta),
	    (uint32_t) be16toh (message->bid_price) / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->bid_size),

	    be64toh(message->original_ask_delta),
	    be64toh(message->new_ask_delta),
	    (uint32_t) be16toh(message->ask_price) * 100 / EFH_PRICE_SCALE,
	    (uint32_t) be16toh(message->ask_size)
	    );

    break;
  }
    //--------------------------------------------------------------
  case 'K': { //NOM_QUOTE_REPLACE_LONG
    //    if (!quote_mode) break;
    struct itto_quote_replace_long *message = (struct itto_quote_replace_long *)m;
    printf ("OBOID:%16ju,NBOID:%16ju,BP:%8u,BS:%8u,OAOID:%16ju,NAOID:%16ju,AP:%8u,AS:%8u",
	    be64toh (message->original_bid_delta),
	    be64toh (message->new_bid_delta),
	    be32toh (message->bid_price) * 100 / EFH_PRICE_SCALE,
	    be32toh (message->bid_size),

	    be64toh (message->original_ask_delta),
	    be64toh (message->new_ask_delta),
	    be32toh (message->ask_price) / EFH_PRICE_SCALE,
	    be32toh (message->ask_size)
	    );
    break;
  }
    //--------------------------------------------------------------

  case 'Y': { //NOM_QUOTE_DELETE 
    //    if (!quote_mode)  break;
    struct itto_quote_delete *message = (struct itto_quote_delete *)m;
    printf ("BOID:%16ju,AOID:%16ju",be64toh(message->bid_delta),be64toh(message->ask_delta));

    break;
  }
    //--------------------------------------------------------------

  case 'P': { //NOM_OPTIONS_TRADE
    struct itto_options_trade *message = (struct itto_options_trade *)m;
    printf ("SID:%16u",be32toh(message->option_id));
    break;
  }
    //--------------------------------------------------------------
  case 'Q': { //NOM_CROSS_TRADE
    break;
  }
    //--------------------------------------------------------------
  case 'B': { //NOM_BROKEN_EXEC
    break;
  }
    //--------------------------------------------------------------
  case 'I': { //NOM_NOII
    break;
  }
  default: 
    on_error("UNEXPECTED Message type: enc=\'%c\'",(char)m[0]);
  }
  printf ("\n");
  return;

}

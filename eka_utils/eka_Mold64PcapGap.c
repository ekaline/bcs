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

#define on_error(...) { fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }

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


struct GrpState {
  const char* name;
  in_addr_t mc_ip;
  uint16_t  port;
  uint64_t  expected;
  uint64_t  ts_ns;
  uint64_t  drop_cnt;
  uint64_t  back_in_timet_cnt;
};

static GrpState gr[] = {
  /* {inet_addr("233.54.12.72"),18000,0,{},0,0}, */
  /* {inet_addr("233.54.12.73"),18001,0,{},0,0}, */
  /* {inet_addr("233.54.12.74"),18002,0,{},0,0}, */
  /* {inet_addr("233.54.12.75"),18003,0,{},0,0},     */

  /* {inet_addr("233.54.12.76"),18000,0,{},0,0}, */
  /* {inet_addr("233.54.12.77"),18001,0,{},0,0}, */
  /* {inet_addr("233.54.12.78"),18002,0,{},0,0}, */
  /* {inet_addr("233.54.12.79"),18003,0,{},0,0}, */

  /* {inet_addr("233.49.196.72"),18000,0,{},0,0}, */
  /* {inet_addr("233.49.196.73"),18001,0,{},0,0}, */
  /* {inet_addr("233.49.196.74"),18002,0,{},0,0}, */
  /* {inet_addr("233.49.196.75"),18003,0,{},0,0} */
  {"NOM_ITTO_0",inet_addr("233.54.12.72"),18000},
  {"NOM_ITTO_1",inet_addr("233.54.12.73"),18001},
  {"NOM_ITTO_2",inet_addr("233.54.12.74"),18002},
  {"NOM_ITTO_3",inet_addr("233.54.12.75"),18003},
  {"NOM_ITTO_0",inet_addr("233.49.196.72"),18000},
  {"NOM_ITTO_1",inet_addr("233.49.196.73"),18001},
  {"NOM_ITTO_2",inet_addr("233.49.196.74"),18002},
  {"NOM_ITTO_3",inet_addr("233.49.196.75"),18003},
  {"GEM_TQF_0",inet_addr("233.54.12.148"),18000},
  {"GEM_TQF_1",inet_addr("233.54.12.148"),18001},
  {"GEM_TQF_2",inet_addr("233.54.12.148"),18002},
  {"GEM_TQF_3",inet_addr("233.54.12.148"),18003},
  {"GEM_TQF_0",inet_addr("233.54.12.164"),18000},
  {"GEM_TQF_1",inet_addr("233.54.12.164"),18001},
  {"GEM_TQF_2",inet_addr("233.54.12.164"),18002},
  {"GEM_TQF_3",inet_addr("233.54.12.164"),18003},
  {"ISE_TQF_0",inet_addr("233.54.12.152"),18000},
  {"ISE_TQF_1",inet_addr("233.54.12.152"),18001},
  {"ISE_TQF_2",inet_addr("233.54.12.152"),18002},
  {"ISE_TQF_3",inet_addr("233.54.12.152"),18003},
  {"ISE_TQF_0",inet_addr("233.54.12.168"),18000},
  {"ISE_TQF_1",inet_addr("233.54.12.168"),18001},
  {"ISE_TQF_2",inet_addr("233.54.12.168"),18002},
  {"ISE_TQF_3",inet_addr("233.54.12.168"),18003},
  {"PHLX_TOPO_0",inet_addr("233.47.179.104"),18016},
  {"PHLX_TOPO_1",inet_addr("233.47.179.105"),18017},
  {"PHLX_TOPO_2",inet_addr("233.47.179.106"),18018},
  {"PHLX_TOPO_3",inet_addr("233.47.179.107"),18019},
  {"PHLX_TOPO_4",inet_addr("233.47.179.108"),18020},
  {"PHLX_TOPO_5",inet_addr("233.47.179.109"),18021},
  {"PHLX_TOPO_6",inet_addr("233.47.179.110"),18022},
  {"PHLX_TOPO_7",inet_addr("233.47.179.111"),18023},
  {"PHLX_TOPO_0",inet_addr("233.47.179.168"),18016},
  {"PHLX_TOPO_1",inet_addr("233.47.179.169"),18017},
  {"PHLX_TOPO_2",inet_addr("233.47.179.170"),18018},
  {"PHLX_TOPO_3",inet_addr("233.47.179.171"),18019},
  {"PHLX_TOPO_4",inet_addr("233.47.179.172"),18020},
  {"PHLX_TOPO_5",inet_addr("233.47.179.173"),18021},
  {"PHLX_TOPO_6",inet_addr("233.47.179.174"),18022},
  {"PHLX_TOPO_7",inet_addr("233.47.179.175"),18023},

};

/* void hexDump (const char* desc, void *addr, int len) { */
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

int get_gr_id(in_addr_t ip, uint16_t port) {
  for (int i = 0; i < (int)(sizeof(gr)/sizeof(gr[0])); i++) {
    if ((gr[i].mc_ip == ip) || (gr[i].port == port)) return i;
  }
  return -1;
}

uint64_t get_ts(uint8_t* msg) {
  uint64_t ts_tmp = 0;
  memcpy((uint8_t*)&ts_tmp+2,msg+5,6);
  return be64toh(ts_tmp);
}

static uint64_t ts_ns2str(char* dst, uint64_t ts) {
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

int main(int argc, char *argv[]) {
  uint8_t buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) on_error ("Failed to read pcap_file_hdr from the pcap file");

  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    if (fread(buf,pcap_rec_hdr_ptr->orig_len,1,pcap_file) != 1) on_error ("Failed to read %d packet bytes",pcap_rec_hdr_ptr->orig_len);

    if (! EKA_IS_UDP_PKT(buf)) continue;


    int gr_id = get_gr_id((in_addr_t)EKA_IPH_DST(buf),EKA_UDPH_DST(buf));
    if (gr_id < 0) continue;

    uint8_t* pkt = buf + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
    uint16_t message_cnt = EKA_MOLD_MSG_CNT(pkt);
    uint64_t sequence = EKA_MOLD_SEQUENCE(pkt);

    uint8_t* msg = pkt + sizeof(mold_hdr);
    if (gr[gr_id].expected == 0) {
      gr[gr_id].expected = sequence + message_cnt;
      gr[gr_id].ts_ns = get_ts(msg);
      continue;
    }
    uint64_t ts_ns = get_ts(msg);
    if (sequence != gr[gr_id].expected) {
      char last_ts[32] = {};
      char curr_ts[32] = {};
      char diff_ts[32] = {};

      ts_ns2str(curr_ts,ts_ns);
      ts_ns2str(last_ts,gr[gr_id].ts_ns);
      ts_ns2str(diff_ts,ts_ns - gr[gr_id].ts_ns);

      printf("%20s %s:%u: expected %s %ju received %s %ju -- ",
	     gr[gr_id].name,
	     inet_ntoa(*(in_addr*) &EKA_IPH_DST(buf)),
	     EKA_UDPH_DST(buf),
	     last_ts,
	     gr[gr_id].expected,
	     curr_ts,
	     sequence
	     );
      if (sequence > gr[gr_id].expected)  {
	printf ("lost %8ju, down time: %s\n",
		sequence - gr[gr_id].expected,
		diff_ts
		);
	gr[gr_id].drop_cnt++;
				 
      } else {
	printf ("BACK IN TIME\n");
	gr[gr_id].back_in_timet_cnt++;
      }
    }
    gr[gr_id].expected = sequence + message_cnt;
    gr[gr_id].ts_ns = ts_ns;

  }

  fclose(pcap_file);

  for (int i = 0; i < (int)(sizeof(gr)/sizeof(gr[0])); i++) {
    if (gr[i].drop_cnt == 0) continue;
    printf("%20s %s:%u: Gaps: %8ju, Back-in-time: %8ju\n",
	   gr[i].name,
	   inet_ntoa(*(in_addr*) &gr[i].mc_ip),
	   gr[i].port,
	   gr[i].drop_cnt,
	   gr[i].back_in_timet_cnt
	   );
  }
}

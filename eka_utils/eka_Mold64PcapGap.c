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
#include <time.h>

#include "ekaNW.h"

#define on_error(...) { fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }

/* struct mold_hdr { */
/*   uint8_t       session_id[10]; */
/*   uint64_t      sequence; */
/*   uint16_t      message_cnt; */
/* } __attribute__((packed)); */

int timespec2str(char *buf, int len, struct timespec *ts) {
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec),&t) == NULL) return 1;

    ret = strftime(buf, len, "%F %T", &t);
    if (ret == 0) return 2;
    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= len) return 3;

    return 0;
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


struct GrpState {
  const char* name;
  in_addr_t mc_ip;
  uint16_t  port;
  uint64_t  expected;
  uint64_t  drop_cnt;
};

static GrpState gr[] = {
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

int get_gr_id(in_addr_t ip, uint16_t port) {
  for (int i = 0; i < (int)(sizeof(gr)/sizeof(gr[0])); i++) {
    if ((gr[i].mc_ip == ip) || (gr[i].port == port)) return i;
  }
  return -1;
}

int main(int argc, char *argv[]) {
  uint8_t buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) on_error ("Failed to read pcap_file_hdr from the pcap file");

  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    if (fread(buf,pcap_rec_hdr_ptr->orig_len,1,pcap_file) != 1) 
      on_error ("Failed to read %d packet bytes",pcap_rec_hdr_ptr->orig_len);

    if (! EKA_IS_UDP_PKT(buf)) continue;

    int gr_id = get_gr_id((in_addr_t)EKA_IPH_DST(buf),EKA_UDPH_DST(buf));
    if (gr_id < 0) continue;

    uint8_t* pkt         = buf + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
    uint16_t message_cnt = EKA_MOLD_MSG_CNT(pkt);
    uint64_t sequence    = EKA_MOLD_SEQUENCE(pkt);

    timespec ts = {
      .tv_sec  = pcap_rec_hdr_ptr->ts_sec,
      .tv_nsec = pcap_rec_hdr_ptr->ts_usec * 1000
    };

    if (sequence == 0) gr[gr_id].expected = sequence;

    if (sequence != gr[gr_id].expected) {
      char timestr[40] = {};
      int rc = timespec2str(timestr, sizeof(timestr), &ts);
      if (rc != 0) on_error("timespec2str failed: rc=%d",rc);
      
      printf("%40s: %20s %s:%u: expected %10ju != received %10ju, lost %jd\n",
	     timestr,
	     gr[gr_id].name,
	     inet_ntoa(*(in_addr*) &EKA_IPH_DST(buf)),
	     EKA_UDPH_DST(buf),
	     gr[gr_id].expected,
	     sequence,
	     sequence - gr[gr_id].expected);
      gr[gr_id].drop_cnt++;
    }
    gr[gr_id].expected = sequence + message_cnt;
  }
  fclose(pcap_file);

  for (int i = 0; i < (int)(sizeof(gr)/sizeof(gr[0])); i++) {
    if (gr[i].drop_cnt == 0) continue;
    printf("%20s %s:%u: Gaps: %8ju\n",
	   gr[i].name,
	   inet_ntoa(*(in_addr*) &gr[i].mc_ip),
	   gr[i].port,
	   gr[i].drop_cnt);
  }
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <assert.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "ekaNW.h"
#include "eka_macros.h"

#include "eka_hsvf_box_messages4pcapTest.h"


//#########################################################

inline int skipChar(char* s, char char2skip) {
  int p = 0;
  while (s[p++] != char2skip) {}
  return p;
}

//###################################################

int main(int argc, char *argv[]) {
  char buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) 
    on_error ("Failed to read pcap_file_hdr from the pcap file");

  uint64_t pktNum = 0;
  char timeStamp[16] = {}; 
  //  char sequence[10] = {};
  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    uint pktLen = pcap_rec_hdr_ptr->len;
    if (pktLen > 1536) on_error("Probably wrong PCAP format: pktLen = %u ",pktLen);

    char pkt[1536] = {};
    if (fread(pkt,pktLen,1,pcap_file) != 1) on_error ("Failed to read %d packet bytes",pktLen);
    ++pktNum;
    //    TEST_LOG("pkt = %ju, pktLen = %u",pktNum, pktLen);
    if (! EKA_IS_UDP_PKT(pkt)) continue;

    int pos = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);

    while (pos < (int)pktLen) {
      if (pkt[pos] != HsvfSom) 
	on_error("pkt: %ju: expected SOM (0x%x), received 0x%x",pktNum,HsvfSom,pkt[pos]);
      pos++;
      HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&pkt[pos];
      std::string seqString = std::string(msgHdr->sequence,sizeof(msgHdr->sequence));
      uint64_t seq = std::stoul(seqString,nullptr,10);
      std::string msgId = std::string(msgHdr->MsgType,sizeof(msgHdr->MsgType));
      //      printf("%10ju, %16s,",seq,timeStamp);
      pos += sizeof(HsvfMsgHdr);
      /* -------------------------------- */
      if (msgId == "J ") { // OptionInstrumentKeys
	OptionInstrumentKeys* msg = (OptionInstrumentKeys*)&pkt[pos];
	printf("%10ju, %16s,",seq,timeStamp);
	printf("|%s|,|%s|\n",msgId.c_str(),std::string(msg->InstrumentDescription,sizeof(msg->InstrumentDescription)).c_str());
      } else 
      /* -------------------------------- */
      if (msgId == "N ") { // OptionSummary
	OptionSummary* msg = (OptionSummary*)&pkt[pos];
	printf("%10ju, %16s,",seq,timeStamp);
	printf("|%s|,|%s|\n",msgId.c_str(),std::string(msg->InstrumentDescription,sizeof(msg->InstrumentDescription)).c_str());
      } else 
      /* -------------------------------- */
      if (msgId == "F ") { // OptionQuote
	OptionQuote* msg = (OptionQuote*)&pkt[pos];
	printf("%10ju, %16s,",seq,timeStamp);
	printf("|%s|,|%s|\n",msgId.c_str(),std::string(msg->InstrumentDescription,sizeof(msg->InstrumentDescription)).c_str());
      } else 
      /* -------------------------------- */
      if (msgId == "Z ") { // SystemTimeStamp
	SystemTimeStamp* msg = (SystemTimeStamp*)&pkt[pos];
	sprintf (timeStamp,"%c%c:%c%c:%c%c.%c%c%c",
		 msg->TimeStamp[0],msg->TimeStamp[1],msg->TimeStamp[2],msg->TimeStamp[3],msg->TimeStamp[4],msg->TimeStamp[5],
		 msg->TimeStamp[6],msg->TimeStamp[7],msg->TimeStamp[8]
		 );
	printf("%10ju, %16s,",seq,timeStamp);
	printf("|%s|,|%s|\n",msgId.c_str(),timeStamp);
      } else
      /* -------------------------------- */
	{ /* do nothing */ }

      pos += skipChar(&pkt[pos],HsvfEom);
      pos += skipChar(&pkt[pos],'\0');

    }
  }
  fprintf(stderr,"%ju packets processed\n",pktNum);
  fclose(pcap_file);

  return 0;
}

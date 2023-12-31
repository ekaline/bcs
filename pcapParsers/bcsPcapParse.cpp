#include "eka_macros.h"
#include <iostream>
#include <pcap.h>

#include "EkaFhBcsSbeParser.h"

using namespace BcsSbe;

void packetHandler(unsigned char *userData,
                   const struct pcap_pkthdr *pkthdr,
                   const unsigned char *packet) {
  //  hexDump("Pkt", packet, pkthdr->len);
  auto p = packet + 14 + 20 + 8;
  printPkt(p);
}

int main(int argc, char *argv[]) {
  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *pcap = pcap_open_offline(argv[1], errbuf);

  if (pcap == nullptr) {
    std::cerr << "Error opening pcap file: " << errbuf
              << std::endl;
    return 1;
  }

  if (pcap_loop(pcap, 0, packetHandler, nullptr) == -1) {
    std::cerr << "Error in pcap_loop: " << pcap_geterr(pcap)
              << std::endl;
    return 1;
  }

  pcap_close(pcap);
  return 0;
}

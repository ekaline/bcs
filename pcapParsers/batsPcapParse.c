#include <arpa/inet.h>
#include <assert.h>
#include <endian.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "EkaFhBatsParser.h"
#include "eka_macros.h"

static uint64_t printBatsMsg(uint64_t pktNum, uint8_t *msg,
                             uint64_t sequence,
                             uint64_t seconds);

using namespace Bats;

// ###################################################

int main(int argc, char *argv[]) {
  char buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL)
    on_error("Failed to open dump file %s", argv[1]);
  if (fread(buf, sizeof(pcap_file_hdr), 1, pcap_file) != 1)
    on_error(
        "Failed to read pcap_file_hdr from the pcap file");

  uint64_t pktNum = 0;

  uint64_t seconds = 0;

  while (fread(buf, sizeof(pcap_rec_hdr), 1, pcap_file) ==
         1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *)buf;
    uint pktLen = pcap_rec_hdr_ptr->len;
    if (pktLen > 1536)
      on_error("Probably wrong PCAP format: pktLen = %u ",
               pktLen);

    uint8_t pkt[1536] = {};
    if (fread(pkt, pktLen, 1, pcap_file) != 1)
      on_error("Failed reading %d bytes", pktLen);
    ++pktNum;
    //    TEST_LOG("pkt = %ju, pktLen = %u",pktNum, pktLen);
    if (!EKA_IS_UDP_PKT(pkt))
      continue;

    int pos = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
              sizeof(EkaUdpHdr);

    auto msgInPkt = EKA_BATS_MSG_CNT((&pkt[pos]));
    auto sequence = EKA_BATS_SEQUENCE((&pkt[pos]));
    pos += sizeof(sequenced_unit_header);
    printf("-----------\n");

    for (uint msg = 0; msg < msgInPkt; msg++) {
      uint8_t msgLen = pkt[pos];

      auto sec = printBatsMsg(pktNum, &pkt[pos], sequence++,
                              seconds);
      if (sec)
        seconds = sec;

      pos += msgLen;
    }
  }
  fprintf(stderr, "%ju packets processed\n", pktNum);
  fclose(pcap_file);

  return 0;
}

static uint64_t printBatsMsg(uint64_t pktNum, uint8_t *msg,
                             uint64_t sequence,
                             uint64_t seconds) {
  MsgId enc = (MsgId)msg[1];

  uint64_t ts =
      seconds + ((const GenericHeader *)msg)->time;

  uint64_t sec = 0;

  printf("%10ju,", pktNum);
  printf("%s", ts_ns2str(ts).c_str());
  printf("%10ju,", sequence);
  printf("%s(0x%x),", EKA_BATS_PITCH_MSG_DECODE(enc),
         (uint8_t)enc);

  switch (enc) {
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_LONG: {
    auto m{reinterpret_cast<const add_order_long *>(msg)};

    printf(
        "SID:\'%s\'(0x%016jx),OID:%ju,\'%c\',P:%8ju,S:%8u",
        EKA_PRINT_BATS_SYMBOL(m->symbol),
        symbol2secId(m->symbol), m->order_id, m->side,
        m->price, m->size);
  } break;
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_SHORT: {
    auto m{reinterpret_cast<const add_order_short *>(msg)};

    printf(
        "SID:\'%s\'(0x%016jx),OID:%ju,\'%c\',P:%8d,S:%8d",
        EKA_PRINT_BATS_SYMBOL(m->symbol),
        symbol2secId(m->symbol), m->order_id, m->side,
        m->price * 100, m->size);
  } break;
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_EXPANDED: {
    auto m{
        reinterpret_cast<const add_order_expanded *>(msg)};

    printf(
        "SID:\'%s\'(0x%016jx),OID:%ju,\'%c\',P:%8ju,S:%8u",
        EKA_PRINT_BATS_SYMBOL_EXP(m->exp_symbol),
        expSymbol2secId(m->exp_symbol), m->order_id,
        m->side, m->price, m->size);
  } break;
    //--------------------------------------------------------------
  case MsgId::TRADE_LONG: {
    auto m{reinterpret_cast<const TradeLong *>(msg)};
    printf("SID:\'%s\'(0x%016jx),OID:%ju,P:%8ju,S:%8d,TC:"
           "\'%c\'",
           EKA_PRINT_BATS_SYMBOL(m->symbol),
           symbol2secId(m->symbol), m->order_id, m->price,
           m->size, m->trade_condition);
  } break;

    //--------------------------------------------------------------
  case MsgId::TRADE_SHORT: {
    auto m{reinterpret_cast<const TradeShort *>(msg)};
    printf("SID:\'%s\'(0x%016jx),OID:%ju,P:%8u,S:%8d,TC:\'%"
           "c\'",
           EKA_PRINT_BATS_SYMBOL(m->symbol),
           symbol2secId(m->symbol), m->order_id,
           m->price * 100, m->size, m->trade_condition);
  } break;
    //--------------------------------------------------------------
  case MsgId::TRADE_EXPANDED: {
    auto m{reinterpret_cast<const TradeExpanded *>(msg)};
    printf("SID:\'%s\'(0x%016jx),OID:%ju,P:%8ju,S:%8d,TC:"
           "\'%c\'",
           EKA_PRINT_BATS_SYMBOL_EXP(m->symbol),
           expSymbol2secId(m->symbol), m->order_id,
           m->price, m->size, m->trade_condition);
  } break;
    //--------------------------------------------------------------

  case MsgId::TIME: {
    auto m{reinterpret_cast<const GenericHeader *>(msg)};
    sec = static_cast<uint64_t>(m->time * SEC_TO_NANO);
    printf("sec = %ju", sec);
  } break;

    //--------------------------------------------------------------
  case MsgId::ORDER_MODIFY_SHORT: {
    auto m{
        reinterpret_cast<const order_modify_short *>(msg)};
    printf("OID:%ju,P:%8u,S:%8d,flags:%x", m->order_id,
           m->price * 100, m->size, m->flags);
  } break;
    //--------------------------------------------------------------
  case MsgId::ORDER_MODIFY_LONG: {
    auto m{
        reinterpret_cast<const order_modify_long *>(msg)};
    printf("OID:%ju,P:%8ju,S:%8d,flags:%x", m->order_id,
           m->price, m->size, m->flags);
  } break;
    //--------------------------------------------------------------
  case MsgId::REDUCED_SIZE_SHORT: {
    auto m{
        reinterpret_cast<const reduced_size_short *>(msg)};
    printf("OID:%ju,CanceledSize:%u", m->order_id,
           m->canceled_size);
  } break;
    //--------------------------------------------------------------
  case MsgId::REDUCED_SIZE_LONG: {
    auto m{
        reinterpret_cast<const reduced_size_long *>(msg)};
    printf("OID:%ju,CanceledSize:%u", m->order_id,
           m->canceled_size);
  } break;
    //--------------------------------------------------------------
  case MsgId::ORDER_EXECUTED: {
    auto m{reinterpret_cast<const order_executed *>(msg)};
    printf("OID:%ju,ExecID:%ju,ExecutedSize:%u,TC:%c",
           m->order_id, m->execution_id, m->executed_size,
           m->trade_condition);
  } break;
    //--------------------------------------------------------------
  case MsgId::ORDER_DELETE: {
    auto m{reinterpret_cast<const order_delete *>(msg)};
    printf("OID:%ju", m->order_id);
  } break;
    //--------------------------------------------------------------
  case MsgId::ORDER_EXECUTED_AT_PRICE_SIZE: {
    auto m{reinterpret_cast<
        const order_executed_at_price_size *>(msg)};
    printf("OID:%ju,ExecID:%ju,"
           "ExecutedSize:%u,RemainingSize:%u,"
           "P:%ju"
           "TC:%c",
           m->order_id, m->execution_id, m->executed_size,
           m->remaining_size, m->price, m->trade_condition);
  } break;
    //--------------------------------------------------------------
  case MsgId::SYMBOL_MAPPING: {
    auto m{reinterpret_cast<const SymbolMapping *>(msg)};
    printf(
        "%s,%s,%s",
        std::string(m->symbol, sizeof(m->symbol)).c_str(),
        std::string(m->osi_symbol, sizeof(m->osi_symbol))
            .c_str(),
        std::string(m->underlying, sizeof(m->underlying))
            .c_str());
  } break;

  default:
    printf("UNEXPECTED");
    break;
  }
  printf("\n");
  return sec;
}

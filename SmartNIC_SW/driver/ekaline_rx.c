//oob_chan.c: in fbProcessOobChannel before: fbNicReceive(nif, pCurrentPacket); // Pass packet upwards into Linux network stack

uint8_t* eka_frameHeader = sc_get_raw_payload(pCurrentPacket);
int eka_sizeOfEtherFrameHeader = GetSizeOfEthernetFrameHeader((FbEtherFrameHeader*)eka_frameHeader);
struct iphdr *eka_ip = (struct iphdr*)&eka_frameHeader[eka_sizeOfEtherFrameHeader];

if (((FbEtherFrameHeader *)eka_frameHeader)->etherType==0x0081) eka_ip = (struct iphdr *)(((char*) eka_ip) + 4); // VLAN

unsigned char *eka_saddr = (unsigned char *) &(eka_ip->saddr);
unsigned char *eka_daddr = (unsigned char *) &(eka_ip->daddr);

#include <linux/delay.h>

struct udphdr {
  __u16   source;
  __u16   dest;
  __u16   len;
  __u16   check;
};
bool eka_drop_me = false;
if(pDevExt->eka_debug)
  PRINTK("EKALINE DEBUG: RX: eka_frameHeader=%p, eka_ip=%p,  etherType=0x%04x, eka_ip->protocol=0x%0x\n",
       eka_frameHeader,
       eka_ip,
       ((FbEtherFrameHeader *)eka_frameHeader)->etherType,
       eka_ip->protocol
       );

// Dropping all UDP RX
//if (nif->eka_private_data->drop_all_rx_udp == 1 &&
if (pDevExt->eka_drop_all_rx_udp == 1 &&
    (((FbEtherFrameHeader *)eka_frameHeader)->etherType==0x0080 || ((FbEtherFrameHeader *)eka_frameHeader)->etherType==0x0081) &&
    eka_ip->protocol == 0x11) {
  
  struct udphdr* udph = (struct udphdr *)((char*)eka_ip + eka_ip->ihl * sizeof(uint32_t));
  if(pDevExt->eka_debug)
    PRINTK("EKALINE DEBUG: RX: DROPPING: udph=%p,  %d.%d.%d.%d -> %d.%d.%d.%d : %hu->%hu\n", 
	 udph,
	 eka_saddr[0],eka_saddr[1],eka_saddr[2],eka_saddr[3],
	 eka_daddr[0],eka_daddr[1],eka_daddr[2],eka_daddr[3],
	 ntohs(udph->source), ntohs(udph->dest)
	 );

	 eka_drop_me = true;  // UDP
 }

if (eka_ip->protocol == 0x6) { // TCP
  struct tcphdr* tcph = (struct tcphdr *)((char*)eka_ip + eka_ip->ihl * sizeof(uint32_t));

  int eks;
  for (eks=0; eks<EKA_SESSIONS_PER_NIF;eks++) {
    if (nif->eka_private_data->eka_session[eks].active == 0) continue;
    if (nif->eka_private_data->eka_session[eks].ip_saddr != eka_ip->daddr) continue;
    if (nif->eka_private_data->eka_session[eks].ip_daddr != eka_ip->saddr) continue;
    if (nif->eka_private_data->eka_session[eks].tcp_sport != ntohs(tcph->dest)) continue;
    if (nif->eka_private_data->eka_session[eks].tcp_dport != ntohs(tcph->source)) continue;
    eka_drop_me = true;  // PATCH for User space TCP
#if 0



    uint16_t payload_len = ntohs(eka_ip->tot_len) - eka_ip->ihl * sizeof(uint32_t) - 4 * tcph->doff;

    nif->eka_private_data->eka_session[eks].tcp_remote_seq_num = ntohl(tcph->seq) + payload_len;
    nif->eka_private_data->eka_session[eks].pkt_rx_cntr++;
    if(pDevExt->eka_debug)
      PRINTK("EKALINE DEBUG: RX: %s: %d.%d.%d.%d -> %d.%d.%d.%d : %hu->%hu, tcp_local_seq_num: %u, ack: %u, doff: %hu, urg: %hu, FLAGS=%c%c%c%c%c%c, payload_len=%d\n", 
	     __func__,
	     eka_saddr[0],eka_saddr[1],eka_saddr[2],eka_saddr[3],eka_daddr[0],eka_daddr[1],eka_daddr[2],eka_daddr[3],
	     ntohs(tcph->source), ntohs(tcph->dest),
	     nif->eka_private_data->eka_session[eks].tcp_local_seq_num, ntohl(tcph->ack_seq),
	     tcph->doff, ntohs(tcph->urg_ptr),
	     tcph->urg ? 'U' : '-',
	     tcph->ack ? 'A' : '-',
	     tcph->psh ? 'P' : '-',
	     tcph->rst ? 'R' : '-',
	     tcph->syn ? 'S' : '-',
	     tcph->fin ? 'F' : '-',
	     payload_len);
	
    if ((ntohl(tcph->ack_seq) > nif->eka_private_data->eka_session[eks].tcp_local_seq_num) && !tcph->syn && !tcph->fin) {
      if(pDevExt->eka_debug)
	PRINTK("EKALINE DEBUG: RX: %s: expected ACK= %u, received ACK=%u -- DELAYING START\n",__func__,nif->eka_private_data->eka_session[eks].tcp_local_seq_num,ntohl(tcph->ack_seq));
      int cnt = 0;
      uint d = 5;
      while ((ntohl(tcph->ack_seq) > nif->eka_private_data->eka_session[eks].tcp_local_seq_num) && !tcph->syn && !tcph->fin && (cnt++ < 10000)) {
	udelay (d);
      }
      if(pDevExt->eka_debug)
	PRINTK("EKALINE DEBUG: RX: %s: expected ACK= %u, received ACK=%u, delayed %d us -- DELAYING END\n",__func__,nif->eka_private_data->eka_session[eks].tcp_local_seq_num,ntohl(tcph->ack_seq),cnt*d);
    }
#endif // 0
    break;
  }
 }

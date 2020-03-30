uint8_t* eka_frameHeader = sn_get_raw_payload(oobPacket);
int eka_sizeOfEtherFrameHeader = GetSizeOfEthernetFrameHeader((FbEtherFrameHeader*)eka_frameHeader);
//struct iphdr *eka_ip = (struct iphdr*)ipHeader;
struct iphdr *eka_ip = (struct iphdr*)&eka_frameHeader[eka_sizeOfEtherFrameHeader];

unsigned char *eka_saddr = (unsigned char *) &(eka_ip->saddr);
unsigned char *eka_daddr = (unsigned char *) &(eka_ip->daddr);

#include <linux/delay.h>
//#include <linux/ip.h>
//#include <linux/tcp.h>

bool eka_drop_me = false;
if (eka_ip->protocol == 0x6) {
    struct tcphdr* tcph = (struct tcphdr *)((char*)eka_ip + eka_ip->ihl * sizeof(uint32_t));

    int eks;
    for (eks=0; eks<EKA_SESSIONS_PER_NIF;eks++) {
        if (nif->eka_private_data->eka_session[eks].active == 0) continue;
        if (nif->eka_private_data->eka_session[eks].ip_saddr != eka_ip->daddr) continue;
        if (nif->eka_private_data->eka_session[eks].ip_daddr != eka_ip->saddr) continue;
        if (nif->eka_private_data->eka_session[eks].tcp_sport != tcph->dest) continue;
        if (nif->eka_private_data->eka_session[eks].tcp_dport != tcph->source) continue;
	uint16_t len = sn_get_raw_payload_length(oobPacket) - eka_sizeOfEtherFrameHeader - eka_ip->ihl * sizeof(uint32_t) - 4 * tcph->doff;

        nif->eka_private_data->eka_session[eks].tcp_remote_seq_num = ntohl(tcph->seq) + len - eka_sizeOfEtherFrameHeader - eka_ip->ihl * sizeof(uint32_t) - 4 * tcph->doff;
        nif->eka_private_data->eka_session[eks].pkt_rx_cntr++;
	PRINTK("EKALINE DEBUG: %s: %d.%d.%d.%d -> %d.%d.%d.%d : %hu->%hu, tcp_local_seq_num: %u, ack: %u, doff: %hu, urg: %hu, FLAGS=%c%c%c%c%c%c, len=%d\n", 
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
	       len);
	
	if (ntohl(tcph->ack_seq) > nif->eka_private_data->eka_session[eks].tcp_local_seq_num) {
	  PRINTK("EKALINE DEBUG: %s: expected ACK= %u, received ACK=%u -- DROPPING\n",__func__,nif->eka_private_data->eka_session[eks].tcp_local_seq_num,ntohl(tcph->ack_seq));
	  eka_drop_me = true;
	}


	//	udelay(10);
        //uint8_t core = (uint8_t)(nif->lane);
        //volatile uint64_t* eka_reg_ack = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + (0x60000 + 0x1000*core)* 8);
        //*eka_reg_ack = tcph->seq;
        //volatile uint64_t* eka_reg_seq_desc = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + (0x6f000 + 0x100*core)* 8);
        //table_desc_t desc = {
        //    .td.source_bank = 0,
        //    .td.source_thread = 0,
        //    .td.target_idx = eks,
        //};
        //*eka_reg_seq_desc = desc.desc;

        break;
    }
}

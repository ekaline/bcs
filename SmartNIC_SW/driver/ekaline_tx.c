/* This file should be included from function
 * fb_start_xmit(struct sk_buff *skb, struct net_device
 * *dev) at file ndev.c */

char *eka_p = pChannel->bucket[bucket]->data;
bool eka_drop_me = false;
const FbEtherFrameHeader *frameHeader =
    (FbEtherFrameHeader *)eka_p;
int sizeOfEtherFrameHeader = GetSizeOfEthernetFrameHeader(
    frameHeader); // includes the VLAN tags if exists !!
struct iphdr *eka_ip =
    (struct iphdr *)&eka_p[sizeOfEtherFrameHeader];

// if (frameHeader->etherType == htons(ETHER_TYPE_ARP) &&
// ndev->nif->eka_private_data->drop_arp) eka_drop_me =
// true;

/* unsigned char *eka_saddr =
    (unsigned char *)&(eka_ip->saddr);
unsigned char *eka_daddr =
    (unsigned char *)&(eka_ip->daddr);
 */
// uint8_t core = (uint8_t)(ndev->nif->lane);

/* PRINTK("EKALINE DEBUG: TX %s: eka_debug=%u, core=%d,
 * etherType=0x%04x (ETHER_TYPE_VLAN=0x%04x),
 * eka_ip->protocol=0x%02x,
 * %02x:%02x:%02x:%02x:%02x:%02x--->%02x:%02x:%02x:%02x:%02x:%02x
 * %d.%d.%d.%d -> %d.%d.%d.%d\n", */
/*        __func__, */
/*        ndev->nif->eka_private_data->eka_debug, */
/*        core, */
/*        frameHeader->etherType, */
/*        ETHER_TYPE_VLAN, */
/*        eka_ip->protocol, */
/*        (uint8_t)eka_p[6],(uint8_t)eka_p[7],(uint8_t)eka_p[8],(uint8_t)eka_p[9],(uint8_t)eka_p[10],(uint8_t)eka_p[11],
 */
/*        (uint8_t)eka_p[0],(uint8_t)eka_p[1],(uint8_t)eka_p[2],(uint8_t)eka_p[3],(uint8_t)eka_p[4],
 * (uint8_t)eka_p[5], */
/*        eka_saddr[0],eka_saddr[1],eka_saddr[2],eka_saddr[3],eka_daddr[0],eka_daddr[1],eka_daddr[2],eka_daddr[3]
 */
/*        ); */

// if (eka_ip->protocol == 0x2 &&
// ndev->nif->eka_private_data->drop_igmp) eka_drop_me =
// true; /* IGMP */

// else

// uint8_t core = (uint8_t)(ndev->nif->lane);
// uint8_t core = (uint8_t)(ndev->nif->network_interface);

if (eka_ip->protocol == 0x6) { // TCP
  struct tcphdr *tcph =
      (struct tcphdr *)((char *)eka_ip +
                        eka_ip->ihl * sizeof(uint32_t));
  int eks;
  for (eks = 0; eks < EKA_SESSIONS_PER_NIF; eks++) {
    if (ndev->nif->eka_private_data->eka_session[eks]
            .active == 0)
      continue;

    /* PRINTK("EKALINE DEBUG: TX: 0x%08x:%u --> 0x%08x:%u vs
     * TX: 0x%08x:%u --> 0x%08x:%u\n", */
    /* 	    ndev->nif->eka_private_data->eka_session[eks].ip_saddr,
     */
    /* 	    ndev->nif->eka_private_data->eka_session[eks].tcp_sport,
     */
    /* 	    ndev->nif->eka_private_data->eka_session[eks].ip_daddr,
     */
    /* 	    ndev->nif->eka_private_data->eka_session[eks].tcp_dport,
     */
    /* 	    eka_ip->saddr, */
    /* 	    ntohs(tcph->source), */
    /* 	    eka_ip->daddr, */
    /* 	    ntohs(tcph->dest) */
    /* 	    ); */
    if (ndev->nif->eka_private_data->eka_session[eks]
            .ip_saddr != eka_ip->saddr)
      continue;
    if (ndev->nif->eka_private_data->eka_session[eks]
            .ip_daddr != eka_ip->daddr)
      continue;
    if (ndev->nif->eka_private_data->eka_session[eks]
            .tcp_sport != ntohs(tcph->source))
      continue;
    if (ndev->nif->eka_private_data->eka_session[eks]
            .tcp_dport != ntohs(tcph->dest))
      continue;
    eka_drop_me = true;
#if 0
     int mac_byte;
     for (mac_byte=0; mac_byte<6; mac_byte++) ndev->nif->eka_private_data->eka_session[eks].macda[mac_byte] = (uint8_t)eka_p[mac_byte]; // copying MAC_DA
     for (mac_byte=0; mac_byte<6; mac_byte++) ndev->nif->eka_private_data->eka_session[eks].macsa[mac_byte] = (uint8_t)eka_p[6+mac_byte]; // copying MAC_SA
     //----------------------------------------------------------------
     uint32_t* vlanheader = (uint8_t*)eka_p + 12; // won't be used if no VLAN
     ndev->nif->eka_private_data->eka_session[eks].vlan_tag = *vlanheader;
     //----------------------------------------------------------------

     //      ndev->nif->eka_private_data->eka_session[eks].tcp_window = ntohs(tcph->window);
     // Fix for firing at win==0
     ndev->nif->eka_private_data->eka_session[eks].tcp_window = ntohs(tcph->window) > 1 ? ntohs(tcph->window) - 2 : ntohs(tcph->window);
     //-----------------------------------------------------------------
     // Writing Initial TCP Window size number (temporarily only one global)
     volatile uint64_t* eka_win_size_cont = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + 0xe0318 + core*0x1000);
     //	if(ndev->nif->eka_private_data->eka_debug) PRINTK("EKALINE DEBUG: %s: Initializing TCP WINDOW to 0x%016x (=%u) (Big Endian = %u)\n",__func__,eka_win,eka_win,tcph->window);
     *eka_win_size_cont = (uint64_t) ndev->nif->eka_private_data->eka_session[eks].tcp_window;
     //-----------------------------------------------------------------


     //	    int payload_len = len - sizeOfEtherFrameHeader - 4 * eka_ip->ihl - 4 * tcph->doff;
     int payload_len = ntohs(eka_ip->tot_len) - 4 * eka_ip->ihl - 4 * tcph->doff;
     if (payload_len == 0 || ntohl(tcph->seq) < ndev->nif->eka_private_data->eka_session[eks].tcp_local_seq_num || ndev->nif->eka_private_data->eka_session[eks].send_seq2hw) {
       ndev->nif->eka_private_data->eka_session[eks].pkt_tx_cntr++;
     } else {
       eka_drop_me = true;
     }

     //	    ndev->nif->eka_private_data->eka_session[eks].tcp_local_seq_num = ntohl(tcph->seq) + len - sizeOfEtherFrameHeader - 4 * eka_ip->ihl - 4 * tcph->doff;
     ndev->nif->eka_private_data->eka_session[eks].tcp_local_seq_num = ntohl(tcph->seq) + payload_len;
     if(ndev->nif->eka_private_data->eka_debug)
       PRINTK("EKALINE DEBUG: TX %s: core=%d, %02x:%02x:%02x:%02x:%02x:%02x--->%02x:%02x:%02x:%02x:%02x:%02x %d.%d.%d.%d -> %d.%d.%d.%d : %hu->%hu, seq: %u, ack: %u, doff: %hu, urg: %hu, FLAGS=%c%c%c%c%c%c : payload_len = %u ,data[0]=0x%02x) : %s\n",
	      __func__,
	      core,
	      (uint8_t)eka_p[6],(uint8_t)eka_p[7],(uint8_t)eka_p[8],(uint8_t)eka_p[9],(uint8_t)eka_p[10],(uint8_t)eka_p[11],
	      (uint8_t)eka_p[0],(uint8_t)eka_p[1],(uint8_t)eka_p[2],(uint8_t)eka_p[3],(uint8_t)eka_p[4], (uint8_t)eka_p[5],
	      eka_saddr[0],eka_saddr[1],eka_saddr[2],eka_saddr[3],eka_daddr[0],eka_daddr[1],eka_daddr[2],eka_daddr[3],
	      ntohs(tcph->source), ntohs(tcph->dest),ntohl(tcph->seq), ntohl(tcph->ack_seq),tcph->doff, ntohs(tcph->urg_ptr),
	      tcph->urg ? 'U' : '-',
	      tcph->ack ? 'A' : '-',
	      tcph->psh ? 'P' : '-',
	      tcph->rst ? 'R' : '-',
	      tcph->syn ? 'S' : '-',
	      tcph->fin ? 'F' : '-',
	      payload_len,
	      *((uint8_t *)tcph + 4 * tcph->doff),
	      eka_drop_me ? "DROPPED" : "  ");
     /* volatile uint64_t* eka_reg_addr0 = (uint64_t*)(pDevExt->bar0_va + BAR0_REGS_OFFSET + 0x200 * 8); */
     volatile uint64_t* eka_reg_seq_desc;
     table_desc_t desc;
     if ((ndev->nif->eka_private_data->eka_session[eks].send_seq2hw == 1) && tcph->ack) {
       // Writing Initial TCP Sequence number
       //	      ndev->nif->eka_private_data->eka_session[eks].tcp_local_seq_num ++; // +1 for SYN
       volatile uint64_t* eka_reg_seq = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + (0x30000 + 0x1000*core));
       *eka_reg_seq =  ndev->nif->eka_private_data->eka_session[eks].tcp_local_seq_num;
       eka_reg_seq_desc = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + (0x3f000 + 0x100*core));
       desc.td.source_bank = 0;
       desc.td.source_thread = 0;
       desc.td.target_idx = eks;
       *eka_reg_seq_desc = desc.desc;

       // Writing Initial TCP Window size number (temporarily only one global)
       volatile uint64_t* eka_win_size = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + 0xe0318);
       uint8_t eka_win = ntohs(tcph->window);
       if(ndev->nif->eka_private_data->eka_debug) PRINTK("EKALINE DEBUG: TX %s: Initializing TCP WINDOW to 0x%016x (=%u) (Big Endian = %u)\n",__func__,eka_win,eka_win,tcph->window);
       *eka_win_size = (uint64_t)eka_win;

       ndev->nif->eka_private_data->eka_session[eks].send_seq2hw = 0;
     }
     volatile uint64_t* eka_reg_ack = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + (0x60000 + 0x1000*core));
     *eka_reg_ack = htonl(tcph->ack_seq);
     eka_reg_seq_desc = (uint64_t*)(pDevExt->bar2_va + BAR2_REGS_OFFSET + (0x6f000 + 0x100*core));
     desc.td.source_bank = 0;
     desc.td.source_thread = 0;
     desc.td.target_idx = eks;

     *eka_reg_seq_desc = desc.desc;
     //            PRINTK("EKALINE DEBUG: %s: sending ACK=%u\n",__func__,ntohl(tcph->ack_seq));
#endif // 0
    break;
  }
}

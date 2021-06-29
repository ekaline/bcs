#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <string.h>

#include "eka.h"
#include "smartnic.h"
#include "ctls.h"
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stdout); fflush(stderr); exit(1); }

int main(int argc, char **argv){
    SN_DeviceId DeviceId = SN_OpenDevice(NULL, NULL);
    if (DeviceId == NULL) on_error ("Cannot open Silicom SMARTNIC device. Is driver loaded?\n" );

    int fd = SN_GetFileDescriptor(DeviceId);
    for (int nif=0; nif<SC_NO_NETWORK_INTERFACES;nif++) {
      SN_StatusInfo statusInfo;
      int rc = SN_GetStatusInfo(DeviceId, &statusInfo);
      if (rc != SN_ERR_SUCCESS) on_error("%s: Failed to get status info, error code %d\n", __func__,rc);
      SN_LinkStatus *s = (SN_LinkStatus *) &statusInfo.LinkStatus[nif];
      printf ("NIF%u: SFP_Present=%d, link=%d",nif,s->SFP_Present,s->Link);
      if (s->SFP_Present && s->Link) {
	eka_ioctl_t state = {};
	state.nif_num = nif;
	state.cmd = EKA_GET_NIF_STATE;
	if (ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) < 0) on_error ("IOCTL EKA_GET_NIF_STATE failed\n");
	printf(", eka_debug=%u, drop_igmp=%u, drop_arp=%u, drop_all_rx_udp=%u\n",
	       state.eka_debug,
	       state.drop_igmp,
	       state.drop_arp,
	       state.drop_all_rx_udp);
      } else {
	printf("\n");
	continue;
      }

      for (int eks=0; eks<EKA_SESSIONS_PER_NIF;eks++) {
	eka_ioctl_t state = {};
	memset(&state,0,sizeof(eka_ioctl_t));
	state.cmd = EKA_DUMP;
	state.nif_num = nif;
	state.session_num = eks;
	if (ioctl(fd,SC_IOCTL_EKALINE_DATA,&state) < 0) on_error ("IOCTL EKA_DUMP for nif=%d eks=%d failed\n",nif,eks);;
	if (state.eka_session.active != 1) continue;
	printf ("NIF: %2d, session: %2d, active:%d, mac: ", nif,eks,state.eka_session.active);
	unsigned char *sa = (unsigned char *) &state.eka_session.ip_saddr;
	unsigned char *da = (unsigned char *) &state.eka_session.ip_daddr;
	printf ("%02x:%02x:%02x:%02x:%02x:%02x--->%02x:%02x:%02x:%02x:%02x:%02x, ", 
		state.eka_session.macsa[0],
		state.eka_session.macsa[1],
		state.eka_session.macsa[2],
		state.eka_session.macsa[3],
		state.eka_session.macsa[4],
		state.eka_session.macsa[5],
		state.eka_session.macda[0],
		state.eka_session.macda[1],
		state.eka_session.macda[2],
		state.eka_session.macda[3],
		state.eka_session.macda[4],
		state.eka_session.macda[5]
		);
	printf ("%d.%d.%d.%d->%d.%d.%d.%d, ", sa[0],sa[1],sa[2],sa[3],da[0],da[1],da[2],da[3]);
	printf ("%u->%u, ", state.eka_session.tcp_sport,state.eka_session.tcp_dport);
	printf ("local_seq=%u, remote_seq=%u, ",
		state.eka_session.tcp_local_seq_num  ,state.eka_session.tcp_remote_seq_num );
	printf ("tx=%d, rx=%d",state.eka_session.pkt_tx_cntr,state.eka_session.pkt_rx_cntr);
	printf ("\n");
      }
    }
    SN_CloseDevice(DeviceId);
    return 0;
}

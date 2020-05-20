#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <sched.h>
#include <time.h>

#include "eka_fh_book.h"
#include "eka_fh_group.h"
#include "eka_dev.h"
#include "Efh.h"
#include "Eka.h"
#include "eka_hsvf_box_messages.h"
#include "eka_fh.h"

int ekaTcpConnect(int* sock, uint32_t ip, uint16_t port);
int ekaUdpConnect(EkaDev* dev, int* sock, uint32_t ip, uint16_t port);
/* ----------------------------- */

static EkaOpResult sendLogin (FhBoxGr* gr) {
  EkaDev* dev = gr->dev;

  HsvfLogin msg = {};
  time_t rawtime;
  time (&rawtime);
  struct tm * ct = localtime (&rawtime);

  /* msg.SoM = HsvfSom; */
  /* sprintf(msg.hdr.sequence,"%9ju",gr->txSeqNum ++); */
  /* memcpy(&msg.hdr.MsgType,"LI",sizeof(msg.hdr.MsgType)); */
  /* memcpy(&msg.User,gr->auth_user,sizeof(std::min(sizeof(msg.User),sizeof(gr->auth_user)))); */
  /* memcpy(&msg.Pwd,gr->auth_passwd,sizeof(std::min(sizeof(msg.Pwd),sizeof(gr->auth_passwd)))); */
  /* sprintf(msg.TimeStamp,"%02u%02u%02u",ct->tm_hour,ct->tm_min,ct->tm_sec); */
  /* memcpy(msg.ProtocolVersion,"C7",sizeof(msg.ProtocolVersion)); */
  /* msg.EoM = HsvfEom; */

	
#ifndef FH_LAB
  if(send(gr->snapshot_sock,&msg,sizeof(msg), 0) < 0) {
    EKA_WARN("BOX Login send failed");
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
#endif
  msg.EoM = '\0';
  EKA_LOG("%s:%u Box Login sent: %s",EKA_EXCH_DECODE(gr->exch),gr->id,(char*)&msg);
  return EKA_OPRESULT__OK;
}
/* ----------------------------- */


void eka_hsvf_get_definitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhBoxGr* gr,EkaFhMode op) {
  assert(gr != NULL);
  EkaDev* dev = gr->dev;
  assert(dev != NULL);
  EkaOpResult ret = EKA_OPRESULT__OK;

  EKA_LOG("Definitions for %s:%u - BOX to %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id,EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));
  //-----------------------------------------------------------------
  ekaTcpConnect(&gr->snapshot_sock,gr->snapshot_ip,gr->snapshot_port);
  //-----------------------------------------------------------------
  if ((ret = sendLogin(gr))        != EKA_OPRESULT__OK) return;// ret;
  //-----------------------------------------------------------------
  /* if ((ret = getLoginResponse(gr)) != EKA_OPRESULT__OK) return ret; */
  /* //----------------------------------------------------------------- */
  /* if ((ret = sendRequest(gr))      != EKA_OPRESULT__OK) return ret; */
  //-----------------------------------------------------------------
  
}

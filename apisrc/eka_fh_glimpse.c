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
#include "EkaDev.h"
#include "Efh.h"
#include "Eka.h"
#include "eka_fh_phlx_messages.h"
#include "eka_fh.h"

int ekaTcpConnect(int* sock, uint32_t ip, uint16_t port);
int ekaUdpConnect(EkaDev* dev, int* sock, uint32_t ip, uint16_t port);

struct soupbin_header {
  uint16_t    length;
  char        type;
} __attribute__((packed));

struct soupbin_login_req {
  struct soupbin_header	header;
  char		       	username[6];
  char		       	password[10];
  char			session[10];
  char			sequence[20];
} __attribute__((packed));


void* soupbin_heartbeat_thread(void* attr) {
  pthread_detach(pthread_self());
  FhGroup* gr = (FhGroup*) attr;
  EkaDev* dev = gr->dev;
  gr->heartbeatThreadDone = false;
  struct soupbin_header heartbeat = {};
  heartbeat.length		= htons(1);
  heartbeat.type		= 'R';
  EKA_TRACE("%s:%u: sending heartbeats to sock = %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
  while(gr->heartbeat_active) {
    /* EKA_TRACE("%s:%u sending Glimpse hearbeat on sock %d, recovery_sequence = %ju, currUdpSequence=%ju", */
    /* 	      EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock,gr->recovery_sequence,gr->q == NULL ? 0 : gr->q->currUdpSequence); */
    if(send(gr->snapshot_sock,&heartbeat,sizeof(struct soupbin_header), 0) < 0) 
      on_error("%s:%u: heartbeat send failed, gr->snapshot_sock=%d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
    usleep(900000);
  }
  gr->heartbeatThreadDone = true;

  EKA_LOG("%s:%u: Glimpse hearbeat thread terminated on sock %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
  return NULL;
}

static void sendLogin (FhGroup* gr, uint64_t start_sequence) {
  EkaDev* dev = gr->dev;

  struct soupbin_header header = {};
  struct soupbin_login_req login_message = {};
  login_message.header.length = htons(sizeof(struct soupbin_login_req)-sizeof(header.length));
  login_message.header.type		= 'L';
  memset(login_message.session,' ',sizeof(login_message.session));
  //  memcpy(login_message.sequence, "                   1", sizeof(login_message.sequence));

  char tmp_start_sequence_str[21];
  sprintf(tmp_start_sequence_str,"%20ju",start_sequence);
  memcpy(login_message.sequence,tmp_start_sequence_str,20);
  memcpy(login_message.username, gr->auth_user,   sizeof(login_message.username));
  memcpy(login_message.password, gr->auth_passwd, sizeof(login_message.password));
#if 1
  EKA_LOG("%s:%u: sending login message: user[6]=|%s|, passwd[10]=|%s|, session[10]=|%s|, sequence[20]=|%s|",
	  EKA_EXCH_DECODE(gr->exch),gr->id,
	  login_message.username+'\0',
	  login_message.password+'\0',
	  login_message.session+'\0',
	  login_message.sequence+'\0'
	  );
#endif	
  if(send(gr->snapshot_sock,&login_message,sizeof(login_message), 0) < 0) 
    on_error("%s:%u Login send failed, gr->snapshot_sock = %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
  return;
}

static void sendLogout (FhGroup* gr) {
  EkaDev* dev = gr->dev;
  struct soupbin_header logout_request = {};
  logout_request.length		= htons(1);
  logout_request.type		= 'O';
  if(send(gr->snapshot_sock,&logout_request,sizeof(logout_request) , 0) < 0) 
    on_error("%s:%u Logout send failed, gr->snapshot_sock = %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
  EKA_LOG("%s:%u Logout sent",EKA_EXCH_DECODE(gr->exch),gr->id);

  return;
}
/* ##################################################################### */

static bool getLoginResponse(FhGroup* gr) {
  EkaDev* dev = gr->dev;

  soupbin_header soupbin_hdr ={};
  if (recv(gr->snapshot_sock,&soupbin_hdr,sizeof(struct soupbin),MSG_WAITALL) <= 0) 
    on_error("%s:%u Glimpse connection reset by peer after Login (failed to receive SoupbinHdr), gr->snapshot_sock = %d",
	     EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
  char soupbin_buf[100] = {};
  if (recv(gr->snapshot_sock,soupbin_buf,be16toh(soupbin_hdr.length) - sizeof(soupbin_hdr.type),MSG_WAITALL) <= 0) 
    on_error("%s:%u failed to receive Soupbin message from Glimpse, gr->snapshot_sock = %d",
	     EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);

  if (soupbin_hdr.type == 'J') on_error("Glimpse rejected login (\'J\') message with code: |%c|",soupbin_buf[0]);
  if (soupbin_hdr.type == 'H') on_error("Glimpse Heartbeat arrived before login");
  if (soupbin_hdr.type != 'A') on_error("Unknown Soupbin message type \'%c\' arrived after Login request",soupbin_hdr.type);
  char* session_id = soupbin_buf;
  char first_seq[20] = {};
  memcpy(first_seq,session_id+10,sizeof(first_seq));
  gr->recovery_sequence = strtoul(first_seq, NULL, 10);
  
  EKA_LOG("%s:%u Login accepted for session_id=%s, first_seq=%ju",EKA_EXCH_DECODE(gr->exch),gr->id,session_id,gr->recovery_sequence);
  return true;  
}
/* ##################################################################### */

void* eka_get_glimpse_data(void* attr) {

  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  FhGroup*   gr             = ((EkaFhThreadAttr*)attr)->gr;
  uint64_t   start_sequence = ((EkaFhThreadAttr*)attr)->startSeq;
  uint64_t   end_sequence   = ((EkaFhThreadAttr*)attr)->endSeq;
  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;

  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();


  if (op != EkaFhMode::DEFINITIONS) pthread_detach(pthread_self());

  eka_dev_t* dev = pEfhCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  if (dev->fh[pEfhCtx->fhId] == NULL) on_error("dev->fh[pEfhCtx->fhId] == NULL for pEfhCtx->fhId = %u",pEfhCtx->fhId);
  if (gr == NULL) on_error("gr == NULL");

  EKA_LOG("%s:%u %s : start_sequence=%ju, end_sequence=%ju",EKA_EXCH_DECODE(gr->exch),gr->id,
	    op==EkaFhMode::SNAPSHOT ? "GAP_SNAPSHOT" : "DEFINITIONS",
	    start_sequence,end_sequence
	    );
  //-----------------------------------------------------------------
  EkaCredentialLease* lease;
  const struct timespec leaseTime = {.tv_sec = 180, .tv_nsec = 0};
  const struct timespec timeout   = {.tv_sec = 60,  .tv_nsec = 0};
  char credName[7] = {};
  memset (credName,'\0',sizeof(credName));
  memcpy (credName,gr->auth_user,sizeof(credName) - 1);
  const EkaGroup group{gr->exch, (EkaLSI)gr->id};
  int rc = dev->credAcquire(EkaCredentialType::kSnapshot, group, (const char*)credName, &leaseTime,&timeout,dev->credContext,&lease);
  if (rc != 0) on_error("%s:%u Failed to credAcquire for %s",EKA_EXCH_DECODE(gr->exch),gr->id,credName);
  EKA_LOG("%s:%u Glimpse Credentials Accquired",EKA_EXCH_DECODE(gr->exch),gr->id);
  //-----------------------------------------------------------------
  if (gr->snapshot_sock != -1) on_error("%s:%u gr->snapshot_sock != -1",EKA_EXCH_DECODE(gr->exch),gr->id);
  ekaTcpConnect(&gr->snapshot_sock,gr->snapshot_ip,gr->snapshot_port);
  if (gr->snapshot_sock == -1) on_error("%s:%u gr->snapshot_sock = -1",EKA_EXCH_DECODE(gr->exch),gr->id);
  EKA_LOG("%s:%u TCP connected to Glimpse, gr->snapshot_sock = %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);

  //-----------------------------------------------------------------
  sendLogin (gr, start_sequence);
  //-----------------------------------------------------------------
  getLoginResponse(gr);
  //-----------------------------------------------------------------
  gr->heartbeat_active = true;
  gr->hearbeat_ctr = 0;
  gr->snapshot_active = true;
  pthread_t heartbeat_thread;
  dev->createThread((std::string("HB_") + std::string(EKA_EXCH_DECODE(gr->exch)) + '_' + std::to_string(gr->id)).c_str(),EkaThreadType::kHeartbeat,soupbin_heartbeat_thread,(void*)gr,dev->createThreadContext,(uintptr_t*)&heartbeat_thread);

  //-----------------------------------------------------------------

  while (gr->snapshot_active) { // Accepted Login
    struct soupbin soupbin_hdr ={};
    if (recv(gr->snapshot_sock,&soupbin_hdr,sizeof(struct soupbin),MSG_WAITALL) <= 0) 
      on_error("%s:%u: Glimpse Server connection reset by peer (failed to receive SoupbinHdr), gr->snapshot_sock=%d",
	       EKA_EXCH_DECODE(gr->exch),gr->id,gr->snapshot_sock);
    char soupbin_buf[1000] = {};
    int rc_size = recv(gr->snapshot_sock,soupbin_buf,be16toh(soupbin_hdr.length) - sizeof(soupbin_hdr.type),MSG_WAITALL);
    if (rc_size - sizeof(soupbin_hdr.type) <= 0) 	
      on_error("%s:%u failed to receive SoupbinBuf: received %u, expected %ju, gr->snapshot_sock=%d",
	       EKA_EXCH_DECODE(gr->exch),gr->id,rc_size,sizeof(soupbin_hdr.type), gr->snapshot_sock);

    if (soupbin_hdr.type == 'H') {
      if (gr->feed_ver == EfhFeedVer::kPHLX && (op == EkaFhMode::DEFINITIONS  || end_sequence == 1) && ++gr->hearbeat_ctr == 5) { 
	gr->snapshotThreadDone = true;
	break; //
      }
      continue; // Heartbeat
    }
    if (soupbin_hdr.type == 'Z') on_error("%s:%u Glimpse closed the session with Z (End of Session Packet)",EKA_EXCH_DECODE(gr->exch),gr->id);
    if (soupbin_hdr.type == '+') EKA_TRACE("%s:%u Glimpse debug message: %s",EKA_EXCH_DECODE(gr->exch),gr->id,soupbin_buf);
    unsigned char* m = (unsigned char*)&(soupbin_buf[0]);
  //-----------------------------------------------------------------
    if (gr->parseMsg(pEfhRunCtx,m,gr->recovery_sequence,op)) {
      EKA_TRACE("%s:%u After \'M\' message: gr->seq_after_snapshot = %ju, gr->recovery_sequence = %ju",
		EKA_EXCH_DECODE(gr->exch),gr->id,soupbin_buf,gr->seq_after_snapshot, gr->recovery_sequence);
      break;
    }
  //-----------------------------------------------------------------
    if (soupbin_hdr.type == 'S') gr->recovery_sequence++;
  }
  gr->gapClosed = true;
  gr->heartbeat_active = false;
  //-----------------------------------------------------------------
  sendLogout(gr);
  //-----------------------------------------------------------------
  //  heartbeat_thread.join();
  close(gr->snapshot_sock);
  gr->snapshot_sock = -1;
  EKA_TRACE("%s:%u End Of %s, gr->recovery_sequence++ = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,
	    op==EkaFhMode::SNAPSHOT ? "GAP_RECOVERY" : "DEFINITIONS",gr->recovery_sequence);

  rc = dev->credRelease(lease, dev->credContext);
  if (rc != 0) on_error("%s:%u Failed to credRelease for %s",EKA_EXCH_DECODE(gr->exch),gr->id,credName);
  EKA_LOG("%s:%u Glimpse Credentials Released",EKA_EXCH_DECODE(gr->exch),gr->id);

  return NULL;
}

void* eka_get_mold_retransmit_data(void* attr) {
  pthread_detach(pthread_self());

  //  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  FhGroup*   gr             = ((EkaFhThreadAttr*)attr)->gr;
  uint64_t   start          = ((EkaFhThreadAttr*)attr)->startSeq;
  uint64_t   end            = ((EkaFhThreadAttr*)attr)->endSeq;
  //  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;
  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();


  EkaDev* dev = gr->dev;
  assert (dev != NULL);
  if (gr->recovery_sock != -1) on_error("%s:%u gr->recovery_sock != -1",EKA_EXCH_DECODE(gr->exch),gr->id);
  if ((gr->recovery_sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == -1) on_error("Failed to open socket for %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id);

  struct mold_hdr mold_request = {};
  memcpy(&mold_request.session_id,(uint8_t*)gr->session_id,10);
  uint64_t cnt2ask = end - start + 1;
  uint64_t seq2ask = start;

  socklen_t addrlen = sizeof(struct sockaddr);

  struct sockaddr_in mold_recovery_addr = {};
  mold_recovery_addr.sin_addr.s_addr = gr->recovery_ip;
  mold_recovery_addr.sin_port = gr->recovery_port; 

  gr->recovery_active = true;
  uint64_t sequence = 0;
  while (gr->recovery_active && cnt2ask > 0) {
    char buf[1500] = {};
    int read_size = -1;

    mold_request.sequence = be64toh(seq2ask);
    uint16_t cnt2ask4mold = cnt2ask > 200 ? 200 : cnt2ask & 0xFFFF; // 200 is just a number: a Mold pkt always contains less than 200 messages
    mold_request.message_cnt = be16toh(cnt2ask4mold);
    /* EKA_TRACE("%s:%u: Sending Mold request to: %s:%u, session_id = %s, seq=%ju, cnt=%u", */
    /* 	      EKA_EXCH_DECODE(gr->exch),gr->id, */
    /* 	      EKA_IP2STR(*(uint32_t*)&mold_recovery_addr.sin_addr),be16toh(mold_recovery_addr.sin_port), */
    /* 	      mold_request.session_id + '\0', */
    /* 	      be64toh(mold_request.sequence), */
    /* 	      be16toh(mold_request.message_cnt) */
    /* 	      ); */
    if (sendto(gr->recovery_sock,&mold_request,sizeof(struct mold_hdr),0,(const struct sockaddr*) &mold_recovery_addr,sizeof(struct sockaddr)) < 0) 
      on_error("sendto failed for %s:%u on sending Mold request to: %s:%u, gr->recovery_sock=%d",
	       EKA_EXCH_DECODE(gr->exch),gr->id,
	       EKA_IP2STR(*(uint32_t*)&mold_recovery_addr.sin_addr),be16toh(mold_recovery_addr.sin_port),
	       gr->recovery_sock
	       );

    if ((read_size = recvfrom(gr->recovery_sock, buf, sizeof(buf), 0, (struct sockaddr*) &mold_recovery_addr, &addrlen)) < 0) 
      on_error("%s:%u read %d bytes when tried to recv Mold Pkt of > %ju bytes",EKA_EXCH_DECODE(gr->exch),gr->id,read_size,sizeof(struct mold_hdr));

    //-----------------------------------------------
    uint indx = sizeof(struct mold_hdr); // pointer to the start of 1st message in the packet
    uint16_t message_cnt = EKA_MOLD_MSG_CNT(buf);
    sequence = EKA_MOLD_SEQUENCE(buf);
    for (uint msg=0; msg < message_cnt; msg++) {
      uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)&(buf[indx]));
      //-----------------------------------------------------------------
      if (sequence >= start) gr->parseMsg(pEfhRunCtx,(unsigned char*)&buf[indx+2],sequence++,EkaFhMode::MCAST);
      //-----------------------------------------------------------------
      indx += msg_len + sizeof(msg_len); 
    }
    //-----------------------------------------------
    cnt2ask -= message_cnt;
    seq2ask += message_cnt;
  } // while loop
  gr->seq_after_snapshot = sequence;
  EKA_LOG("%s:%u: Mold recovery finished: next expected_sequence = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
  gr->gapClosed = true;

  close(gr->recovery_sock);
  gr->recovery_sock = -1;
  return NULL;
}

void* eka_get_phlx_mold_retransmit_data(void* attr) {
  pthread_detach(pthread_self());

  //  EfhCtx*    pEfhCtx        = ((EkaFhThreadAttr*)attr)->pEfhCtx;
  EfhRunCtx* pEfhRunCtx     = ((EkaFhThreadAttr*)attr)->pEfhRunCtx;
  FhGroup*   gr             = ((EkaFhThreadAttr*)attr)->gr;
  uint64_t   start          = ((EkaFhThreadAttr*)attr)->startSeq;
  uint64_t   end            = ((EkaFhThreadAttr*)attr)->endSeq;
  //  EkaFhMode  op             = ((EkaFhThreadAttr*)attr)->op;
  ((EkaFhThreadAttr*)attr)->~EkaFhThreadAttr();


  EkaDev* dev = gr->dev;
  assert (dev != NULL);
  if (gr->recovery_sock != -1) on_error("%s:%u gr->recovery_sock != -1",EKA_EXCH_DECODE(gr->exch),gr->id);
  if ((gr->recovery_sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == -1) on_error("Failed to open socket for %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id);

  PhlxMoldHdr mold_request = {};
  memcpy(&mold_request.session_id,(uint8_t*)gr->session_id,10);
  uint64_t cnt2ask = end - start + 1;
  uint64_t seq2ask = start;

  socklen_t addrlen = sizeof(struct sockaddr);

  struct sockaddr_in mold_recovery_addr = {};
  mold_recovery_addr.sin_addr.s_addr = gr->recovery_ip;
  mold_recovery_addr.sin_port = gr->recovery_port; 

  gr->recovery_active = true;
  uint64_t sequence = 0;
  while (gr->recovery_active && cnt2ask > 0) {
    char buf[1500] = {};
    int read_size = -1;

    mold_request.sequence = seq2ask;
    uint16_t cnt2ask4mold = cnt2ask > 200 ? 200 : cnt2ask & 0xFFFF; // 200 is just a number: a Mold pkt always contains less than 200 messages
    mold_request.message_cnt = cnt2ask4mold;
    /* EKA_TRACE("%s:%u: Sending Mold request to: %s:%u, session_id = %s, seq=%ju, cnt=%u", */
    /* 	      EKA_EXCH_DECODE(gr->exch),gr->id, */
    /* 	      EKA_IP2STR(*(uint32_t*)&mold_recovery_addr.sin_addr),be16toh(mold_recovery_addr.sin_port), */
    /* 	      mold_request.session_id + '\0', */
    /* 	      mold_request.sequence, */
    /* 	      mold_request.message_cnt */
    /* 	      ); */
    if (sendto(gr->recovery_sock,&mold_request,sizeof(mold_request),0,(const struct sockaddr*) &mold_recovery_addr,sizeof(struct sockaddr)) < 0) 
      on_error("sendto failed for %s:%u on sending Mold request to: %s:%u, gr->recovery_sock=%d",
	       EKA_EXCH_DECODE(gr->exch),gr->id,
	       EKA_IP2STR(*(uint32_t*)&mold_recovery_addr.sin_addr),be16toh(mold_recovery_addr.sin_port),
	       gr->recovery_sock
	       );

    if ((read_size = recvfrom(gr->recovery_sock, buf, sizeof(buf), 0, (struct sockaddr*) &mold_recovery_addr, &addrlen)) < 0) 
      on_error("%s:%u read %d bytes when tried to recv Mold Pkt of > %ju bytes",EKA_EXCH_DECODE(gr->exch),gr->id,read_size,sizeof(struct mold_hdr));

    //-----------------------------------------------
    uint indx = sizeof(struct mold_hdr); // pointer to the start of 1st message in the packet
    uint16_t message_cnt = EKA_PHLX_MOLD_MSG_CNT(buf);
    sequence = EKA_PHLX_MOLD_SEQUENCE(buf);
    for (uint msg=0; msg < message_cnt; msg++) {
      uint16_t msg_len = (uint16_t) *(uint16_t*)&(buf[indx]);
      //-----------------------------------------------------------------
      if (sequence >= start) gr->parseMsg(pEfhRunCtx,(unsigned char*)&buf[indx+2],sequence++,EkaFhMode::MCAST);
      //-----------------------------------------------------------------
      indx += msg_len + sizeof(msg_len); 
    }
    //-----------------------------------------------
    cnt2ask -= message_cnt;
    seq2ask += message_cnt;
  } // while loop
  gr->seq_after_snapshot = sequence;
  EKA_LOG("%s:%u: Mold recovery finished: next expected_sequence = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
  gr->gapClosed = true;

  close(gr->recovery_sock);
  gr->recovery_sock = -1;
  return NULL;
}

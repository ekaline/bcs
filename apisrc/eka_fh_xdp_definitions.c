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

#include "eka_fh_book.h"
//#include "eka_data_structs.h"
#include "eka_fh_group.h"
#include "eka_dev.h"

#include "eka_fh_xdp_messages.h"

int ekaTcpConnect(int* sock, uint32_t ip, uint16_t port);

/* ##################################################################### */

static EkaOpResult sendLogin (FhXdpGr* gr) {
  EkaDev* dev = gr->dev;

  //--------------- XDP Login Request -------------------

  XdpLoginRequest loginRequest = {};
  loginRequest.hdr.MsgSize = sizeof(loginRequest);
  loginRequest.hdr.MsgType = EKA_XDP_MSG_TYPE::LOGIN_REQUEST;
  memcpy(loginRequest.SourceID,gr->auth_user,sizeof(loginRequest.SourceID));
  memcpy(loginRequest.Password,gr->auth_passwd,sizeof(loginRequest.Password));
	
#ifndef FH_LAB
  if(send(gr->snapshot_sock,&loginRequest,sizeof(loginRequest), 0) < 0) {
    EKA_WARN("XDP Login send failed");
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
#endif
  EKA_LOG("%s:%u Xdp Login sent: SourceID = |%s|, Password = |%s|",EKA_EXCH_DECODE(gr->exch),gr->id,loginRequest.SourceID,loginRequest.Password);
  return EKA_OPRESULT__OK;
}
/* ##################################################################### */

static EkaOpResult getLoginResponse(FhXdpGr* gr) {
  EkaDev* dev = gr->dev;

  XdpLoginResponse loginResponse = {};

  if (recv(gr->snapshot_sock,&loginResponse,sizeof(loginResponse),MSG_WAITALL) <= 0) {
    EKA_WARN("Request Server connection reset by peer (failed to receive XdpLoginResponse)");
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }

  if (loginResponse.hdr.MsgType != EKA_XDP_MSG_TYPE::LOGIN_RESPONSE) 
    on_error("loginResponse.hdr.MsgType %u != %u ",(uint)loginResponse.hdr.MsgType,(uint)EKA_XDP_MSG_TYPE::LOGIN_RESPONSE);

  if (loginResponse.ResponseCode != 'A' || loginResponse.RejectCode != ' ') {
    if (loginResponse.ResponseCode != 'B') EKA_WARN("Unexpected loginResponse.ResponseCode %c",loginResponse.ResponseCode);
    EKA_WARN("%s:%u: XDP Login Rejected with reason: |%c| meaning %s",EKA_EXCH_DECODE(gr->exch),gr->id,loginResponse.RejectCode,
	     loginResponse.RejectCode == ' ' ? "Accepted" :
	     loginResponse.RejectCode == 'A' ? "Not authorized" :
	     loginResponse.RejectCode == 'M' ? "Maximum server connections reached" :
	     loginResponse.RejectCode == 'T' ? "Timeout" :
	     "Unknown");
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
  EKA_LOG("%s:%u: XDP Login accepted",EKA_EXCH_DECODE(gr->exch),gr->id);
  return EKA_OPRESULT__OK;
}

/* ##################################################################### */
static EkaOpResult sendRequest(FhXdpGr* gr) {
  EkaDev* dev = gr->dev;

  XdpSeriesIndexMappingRequest defRequest  = {};
  defRequest.hdr.MsgSize = sizeof(defRequest);
  defRequest.hdr.MsgType = EKA_XDP_MSG_TYPE::SERIES_INDEX_MAPPING_REQUEST;

  defRequest.SeriesIndex = 0; // ALL
  memcpy(defRequest.SourceID,gr->auth_user,sizeof(defRequest.SourceID));
  defRequest.ChannelID = XDP_TOP_FEED_BASE + gr->id;   // 

  //  hexDump("XDP definitions request",(char*) &defRequest,sizeof(defRequest));
#ifndef FH_LAB
  if(send(gr->snapshot_sock,&defRequest,sizeof(defRequest), 0) < 0) {
    EKA_WARN("%s:%u: XDP Request send failed",EKA_EXCH_DECODE(gr->exch),gr->id);
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
#endif
  EKA_LOG("%s:%u: SERIES_INDEX_MAPPING_REQUEST sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  return EKA_OPRESULT__OK;
}

/* ##################################################################### */
static EkaOpResult sendLogOut(FhXdpGr* gr) {
  EkaDev* dev = gr->dev;
  XdpLogoutRequest logOut = {};
  logOut.hdr.MsgType = EKA_XDP_MSG_TYPE::LOGOUT_REQUEST;
  logOut.hdr.MsgSize = sizeof(logOut);

  if(send(gr->snapshot_sock,&logOut,sizeof(logOut), 0) < 0) {
    EKA_WARN("%s:%u: XDP Logout send failed",EKA_EXCH_DECODE(gr->exch),gr->id);
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
  EKA_LOG("%s:%u XDP Logout sent",EKA_EXCH_DECODE(gr->exch),gr->id);
  return EKA_OPRESULT__OK;
}


/* ##################################################################### */
static EkaOpResult sendHeartbeat(FhXdpGr* gr) {
  EkaDev* dev = gr->dev;

  XdpHeartbeat hearbeat = {};
  hearbeat.hdr.MsgType = EKA_XDP_MSG_TYPE::HEARTBEAT;
  hearbeat.hdr.MsgSize = sizeof(hearbeat);
  memcpy(&hearbeat.SourceID,gr->auth_user,sizeof(hearbeat.SourceID));
  if(send(gr->snapshot_sock,&hearbeat,sizeof(hearbeat), 0) < (int) sizeof(hearbeat)) {
    EKA_WARN("%s:%u: XDP HEARTBEAT send failed",EKA_EXCH_DECODE(gr->exch),gr->id);
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  }
  EKA_TRACE("%s:%u: XDP HEARTBEAT sent",EKA_EXCH_DECODE(gr->exch),gr->id);
    
  return EKA_OPRESULT__OK;
}

/* ##################################################################### */

EkaOpResult eka_get_xdp_definitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhXdpGr* gr,EkaFhMode op) {
  assert(gr != NULL);
  EkaDev* dev = gr->dev;
  assert(dev != NULL);
  EkaOpResult ret = EKA_OPRESULT__OK;

  EKA_LOG("Definitions for %s:%u - XDP to %s:%u",EKA_EXCH_DECODE(gr->exch),gr->id,EKA_IP2STR(gr->snapshot_ip),be16toh(gr->snapshot_port));
  //-----------------------------------------------------------------
  ekaTcpConnect(&gr->snapshot_sock,gr->snapshot_ip,gr->snapshot_port);
  //-----------------------------------------------------------------
  if ((ret = sendLogin(gr))        != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  if ((ret = getLoginResponse(gr)) != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  if ((ret = sendRequest(gr))      != EKA_OPRESULT__OK) return ret;
  //-----------------------------------------------------------------
  
  while (1) { 
    uint8_t buf[1536] = {};
    uint8_t* hdr = buf;
    if (recv(gr->snapshot_sock,hdr,sizeof(XdpMsgHdr),MSG_WAITALL) < (int)sizeof(XdpMsgHdr)) {
      EKA_WARN("%s:%u: Request Server connection reset by peer (failed to read XdpMsgHdr)",EKA_EXCH_DECODE(gr->exch),gr->id);
      return EKA_OPRESULT__ERR_SYSTEM_ERROR;
    }
    if (recv(gr->snapshot_sock,hdr + sizeof(XdpMsgHdr),((XdpMsgHdr*)hdr)->MsgSize - sizeof(XdpMsgHdr),MSG_WAITALL) < (int)(((XdpMsgHdr*)hdr)->MsgSize - sizeof(XdpMsgHdr))) {
      EKA_WARN("%s:%u: Request Server connection reset by peer (failed to read Msg Body for MsgType = %u)",EKA_EXCH_DECODE(gr->exch),gr->id,((XdpMsgHdr*)hdr)->MsgType);
      return EKA_OPRESULT__ERR_SYSTEM_ERROR;
    }
    //-----------------------------------------------------------------
    switch (((XdpMsgHdr*)hdr)->MsgType) {
      /* ***************************************************** */
    case EKA_XDP_MSG_TYPE::REQUEST_RESPONSE : 
      EKA_LOG("%s:%u: REQUEST_RESPONSE: SourceID = |%s|, ChannelID = %u, Status = %u",
	      EKA_EXCH_DECODE(gr->exch),gr->id,
	      ((XdpRequestResponse*)hdr)->SourceID,
	      ((XdpRequestResponse*)hdr)->ChannelID,
	      ((XdpRequestResponse*)hdr)->Status
	      );

      switch (((XdpRequestResponse*)hdr)->Status) {
      case 0 : 
	EKA_LOG("%s:%u: Request was accepted",EKA_EXCH_DECODE(gr->exch),gr->id);
	break;
      case 1 :
	EKA_WARN("%s:%u: Rejected due to an Invalid Source ID",EKA_EXCH_DECODE(gr->exch),gr->id);
	return EKA_OPRESULT__ERR_SYSTEM_ERROR;
      case 4 :
	EKA_WARN("%s:%u: Rejected due to ",EKA_EXCH_DECODE(gr->exch),gr->id);
	return EKA_OPRESULT__ERR_SYSTEM_ERROR;
      case 6 :
	EKA_WARN("%s:%u: Rejected due to an Invalid Channel ID",EKA_EXCH_DECODE(gr->exch),gr->id);
	return EKA_OPRESULT__ERR_SYSTEM_ERROR;
      case 8 :
	EKA_LOG("%s:%u: Underlying download complete",EKA_EXCH_DECODE(gr->exch),gr->id);
	break;
      case 9 :
	EKA_LOG("%s:%u: Series download complete",EKA_EXCH_DECODE(gr->exch),gr->id);
	//-------------------------------------------------------
	if ((ret = sendLogOut(gr))        != EKA_OPRESULT__OK) return ret;
	close(gr->snapshot_sock);
	//-------------------------------------------------------
	return EKA_OPRESULT__OK;
      case 10 :
	EKA_LOG("%s:%u: Complex download complete",EKA_EXCH_DECODE(gr->exch),gr->id);
	break;
      default:
	EKA_WARN("%s:%u: Unexpected (XdpRequestResponse*)hdr)->Status = %u",EKA_EXCH_DECODE(gr->exch),gr->id,((XdpRequestResponse*)hdr)->Status);
	return EKA_OPRESULT__ERR_SYSTEM_ERROR;
      }
      break;
      /* ***************************************************** */
    case EKA_XDP_MSG_TYPE::HEARTBEAT : 
      if ((ret = sendHeartbeat(gr))        != EKA_OPRESULT__OK) return ret;
      break;
      /* ***************************************************** */
    case EKA_XDP_MSG_TYPE::SERIES_INDEX_MAPPING : {
      XdpSeriesMapping* m = (XdpSeriesMapping*) hdr;
      if (m->ChannelID < XDP_TOP_FEED_BASE) break;

      uint8_t AbcGroupID = m->OptionSymbolRoot[0] - 'A';

      if ((int)AbcGroupID != (int)(m->ChannelID - XDP_TOP_FEED_BASE))
	on_error("m->ChannelID (%u) != AbcGroupID (%u) for OptionSymbolRoot = %s",
		 m->ChannelID,
		 AbcGroupID,
		 std::string(m->OptionSymbolRoot,sizeof(m->OptionSymbolRoot)).c_str()
		 );

      if (AbcGroupID != gr->id) break;

      EfhDefinitionMsg msg = {};
      msg.header.msgType        = EfhMsgType::kDefinition;
      msg.header.group.source   = gr->exch;
      msg.header.group.localId  = gr->id;
      msg.header.underlyingId   = m->UnderlyingIndex;
      msg.header.securityId     = m->SeriesIndex;
      msg.header.sequenceNumber = 0;
      msg.header.timeStamp      = 0;
      msg.header.gapNum         = 0;

      //    msg.secondaryGroup        = 0;
      msg.securityType          = EfhSecurityType::kOpt;
      msg.expiryDate            = 
	(2000 + (m->MaturityDate[0] - '0') * 10 + (m->MaturityDate[1] - '0')) * 10000 + 
	(       (m->MaturityDate[2] - '0') * 10 +  m->MaturityDate[3] - '0')  * 100   +
	(        m->MaturityDate[4] - '0') * 10 +  m->MaturityDate[5] - '0';

      msg.contractSize          = m->ContractMultiplier;
      
      msg.strikePrice           = (uint64_t) (strtof(m->StrikePrice,NULL) * 10000); //  / EFH_XDP_STRIKE_PRICE_SCALE;
      msg.exchange              = EKA_GRP_SRC2EXCH(gr->exch);
      msg.optionType            = m->PutOrCall ?  EfhOptionType::kCall : EfhOptionType::kPut;

      memcpy (&msg.underlying, m->OptionSymbolRoot,std::min(sizeof(msg.underlying), sizeof(m->OptionSymbolRoot)));
      memcpy (&msg.classSymbol,m->UnderlyingSymbol,std::min(sizeof(msg.classSymbol),sizeof(m->UnderlyingSymbol)));

      XdpAuxAttrA attrA = {};
      attrA.attr.StreamID       = m->StreamID;
      attrA.attr.ChannelID      = m->ChannelID;
      attrA.attr.PriceScaleCode = m->PriceScaleCode;
      attrA.attr.GroupID        = m->GroupID;

      XdpAuxAttrB attrB = {};
      attrB.attr.UnderlIdx  = m->UnderlyingIndex;
      attrB.attr.AbcGroupID = AbcGroupID;

      msg.opaqueAttrA = attrA.opaqueField;
      msg.opaqueAttrB = attrB.opaqueField;

      pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    }
      break;
      /* ***************************************************** */

    default:
      EKA_TRACE("%s:%u: ignored EKA_XDP_MSG_TYPE %u",EKA_EXCH_DECODE(gr->exch),gr->id,((XdpMsgHdr*)hdr)->MsgType);
    }
  }
  return EKA_OPRESULT__OK;

}

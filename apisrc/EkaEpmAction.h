#ifndef _EKA_EPM_ACTION_H_
#define _EKA_EPM_ACTION_H_

#include "EkaEpm.h"
#include "EkaWc.h"

class EkaTcpSess;

class EkaEpmAction {
public:
  EkaEpmAction(EkaDev*                 _dev,
	       EkaEpm::ActionType      _type,
	       uint                    _idx,
	       uint                    _localIdx,
	       uint                    _region,
	       uint8_t                 _coreId,
	       uint8_t                 _sessId,
	       uint                    _auxIdx,
	       uint		       _heapOffs,
	       uint64_t		       _actionAddr);

  /* ----------------------------------------------------- */
  int updateAttrs (uint8_t _coreId, uint8_t _sessId, const EpmAction *epmAction);
  /* ----------------------------------------------------- */
  void updateHwAction();
  /* ----------------------------------------------------- */
  void printHwAction();
  /* ----------------------------------------------------- */
  void printHeap();
  /* ----------------------------------------------------- */
  void print(const char* msg);
  /* ----------------------------------------------------- */
  int setUdpMcNwHdrs(uint8_t* macSa, 
		     uint32_t srcIp, 
		     uint32_t dstIp, 
		     uint16_t srcPort, 
		     uint16_t dstPort,
		     uint     payloadSize);
  /* ----------------------------------------------------- */
  void setTcpSess(EkaTcpSess* tcpSess);
  /* ----------------------------------------------------- */

  int setNwHdrs(uint8_t* macDa, 
		uint8_t* macSa, 
		uint32_t srcIp, 
		uint32_t dstIp, 
		uint16_t srcPort, 
		uint16_t dstPort);
  /* ----------------------------------------------------- */
  int sendFullPkt(const void* buf, uint len);
  /* ----------------------------------------------------- */
  int preloadFullPkt(const void* buf, uint len);
  /* ----------------------------------------------------- */
  int setPktPayloadAndSendWC(const void* buf, uint len);
  /* ----------------------------------------------------- */
  int setUdpPktPayload(const void* buf, uint len);
  /* ----------------------------------------------------- */
  int send(uint32_t _tcpCSum);
  /* ----------------------------------------------------- */
  int send();
  /* ----------------------------------------------------- */
  int fastSend(const void* buf, uint len);
  /* ----------------------------------------------------- */
  bool isTcp();
  bool isUdp();

private:
  int setActionBitmap();
  int setTemplate();
  int setName();
  int initEpmActionLocalCopy();
  int setHwAction();
  void setIpTtl ();
  EkaEpm::FrameType getFrameType();
  size_t getPayloadOffset();
  uint16_t getL3L4len();

public:
  inline
  void copyHeap2Fpga() {
    dev->ekaWc->epmCopyWcBuf(heapAddr,
			     &epm->heap[heapOffs],
			     pktSize,
			     localIdx,
			     region
			     );
  }

  inline
  void copyHeap2FpgaAndSend() {
    dev->ekaWc->epmCopyAndSendWcBuf(&epm->heap[heapOffs],
				    pktSize,
				    localIdx,
				    region,
				    tcpCSum
				    );
  }
  /* ----------------------------------------------------- */

public:
  char actionName[30] = {};

  EkaDev*  dev = NULL;
  EkaEpm*  epm = NULL;
  EkaEpm::ActionType type = EkaEpm::ActionType::INVALID;
  uint     idx          = -1;
  uint     localIdx     = -1;
  uint     region       = -1;
  uint8_t  coreId       = -1;;
  uint8_t  sessId       = -1;
  uint     productIdx   = -1;
  uint64_t heapAddr     = -1;
  uint     heapOffs     = -1;
  uint64_t actionAddr   = -1;
  /* uint64_t templateAddr = -1; */
  /* uint     templateId   = -1; */
  EpmTemplate*  epmTemplate = NULL;


  bool     initialized  = false;
  
  uint     thrId        = -1;

  epm_actionid_t nextIdx  = EPM_LAST_ACTION;

  epm_action_t hwAction   = {};
  EpmAction epmActionLocalCopy = {};


private:
  EpmActionBitmap actionBitParams = {};

  epm_strategyid_t strategyId      = -1;
  uint             payloadLen      = 0;
  uint             pktSize         = 0;
  uint32_t         tcpCSum         = 0;

  EkaEthHdr*       ethHdr          = NULL;
  EkaIpHdr*        ipHdr           = NULL;
  EkaTcpHdr*       tcpHdr          = NULL;
  EkaUdpHdr*       udpHdr          = NULL;
  uint8_t*         payload         = NULL;

  volatile uint64_t* snDevWCPtr = NULL;

  EkaTcpSess*      mySess          = NULL;
};



#endif

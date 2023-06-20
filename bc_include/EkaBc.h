#ifndef _EKA_BC_H_
#define _EKA_BC_H_

extern "C" {
struct EkaDev;

#define EFC_BC_MAX_CORES 4
#define EPM_BC_LAST_ACTION 0xFFFF
  
EkaDev *ekaBcOpenDev();
int ekaBcCloseDev(EkaDev *pEkaDev);

int ekaBcTcpConnect(EkaDev *pEkaDev, int8_t lane,
                    const char *ip, uint16_t port);

ssize_t ekaBcSend(EkaDev *pEkaDev, int sock,
                  const void *buf, size_t size);

ssize_t ekaBcRecv(EkaDev *pEkaDev, int sock, void *buf,
                  size_t size);

int ekaBcCloseSock(EkaDev *pEkaDev, int sock);

int ekaBcFcInit(EkaDev *pEkaDev);

void ekaBcSwKeepAliveSend(EkaDev *pEkaDev);

struct EkaBcTriggerParams {
  int8_t lane;      ///< 10G port to receive UDP MC trigger
  const char *mcIp; ///< MC IP address
  uint16_t mcUdpPort; ///< MC UDP Port
};

struct EkaBcFcMdParams {
  ///< list of triggers
  ///< belonging to this strategy
  const EkaBcTriggerParams *triggerParams;
  size_t numTriggers;
};

int ekaBcCmeFcMdInit(EkaDev *pEkaDev,
                     const struct EkaBcFcMdParams *params);

struct EkaBcCmeFastCanceGlobalParams {
  uint8_t report_only;
  uint64_t watchdog_timeout_sec;
};

int ekaBcCmeFcGlobalInit(
    EkaDev *pEkaDev,
    const struct EkaBcCmeFastCanceGlobalParams *params);

struct EkaBcCmeFcAlgoParams {
  int fireActionId; ///< 1st Action fired
  uint64_t minTimeDiff;
  uint8_t minNoMDEntries;
};

int ekaBcCmeFcAlgoInit(
    EkaDev *pEkaDev,
    const struct EkaBcCmeFcAlgoParams *params);

typedef void (*onEkaBcFcReportCb)(const void *report,
                                  size_t len, void *ctx);

struct EkaBcFcRunCtx {
  onEkaBcFcReportCb onReportCb;
  void *cbCtx; ///< optional opaque field looped back to CB
};

void ekaBcFcRun(EkaDev *pEkaDev,
                struct EkaBcFcRunCtx *pEkaBcFcRunCtx);

void ekaBcEnableController(EkaDev *pEkaDev, bool enable,
                           uint32_t ver = 0);

ssize_t ekaBcCmeSendHB(EkaDev *pEkaDev, int sock,
                       const void *buffer, size_t size);

int ekaBcCmeSetILinkAppseq(EkaDev *ekaDev, int sock,
                           int32_t appSequence);

void efcBCPrintFireReport(const void* p, size_t len, void* ctx);
  
struct EkaBcAction {
  int sock;       ///< TCP connection
  int nextAction; ///< Next action in sequence
};

struct EkaBcPortAttrs {
  uint32_t host_ip;
  uint32_t netmask;
  uint32_t gateway;
  char nexthop_mac[6];
  char src_mac_addr[6];
};

void ekaBcConfigurePort(
    EkaDev *pEkaDev, int8_t lane,
    const struct EkaBcPortAttrs *pPortAttrs);

int ekaBcAllocateFcAction(EkaDev *pEkaDev);

struct EkaBcActionParams {
  int tcpSock;
  int nextAction;
};

int ekaBcSetActionParams(
    EkaDev *pEkaDev, int actionIdx,
    const struct EkaBcActionParams *params);

int ekaBcSetActionPayload(EkaDev *pEkaDev, int actionIdx,
                          const void *payload, size_t len);




  ///////////////////////  
  // Reports
  ///////////////////////
  
  enum EkaBCEventType {
    EkaBCFireEvent=1,       
      EkaBCEpmEvent=2,              
      EkaBCExceptionEvent=3,      
      EkaBCFastCancelEvent=4
      };

#define EkaBCEventType2STR(x)					\
  x == EkaBCFireEvent         ? "FireEvent" :		\
    x == EkaBCEpmEvent        ? "EpmEvent" :		\
    x == EkaBCExceptionEvent  ? "ExceptionEvent" :	\
    x == EkaBCFastCancelEvent ? "FastCancelEvent" :	\
    "UnknownReport"

  struct EkaBCContainerGlobalHdr {
    EkaBCEventType type;
    uint32_t num_of_reports;
  };
  

  enum EfcBCReportType {
    ControllerState=1000,       
      MdReport=2000,              
      ExceptionReport=4000,      
      FirePkt=5000,
      EpmReport=6000,
      FastCancelReport=7000
      };
  
  // every report is pre-pended by this header
  struct EfcBCReportHdr {
    EfcBCReportType type;
    uint8_t idx;
    size_t size;
  };
  


  struct BCExceptionReport {
    uint32_t globalVector;
    uint32_t portVector[EFC_BC_MAX_CORES];
  };

  struct BCArmStatusReport {
    uint8_t armFlag;
    uint32_t expectedVersion;
  };

  struct EfcBCExceptionsReport {
    BCArmStatusReport armStatus;
    BCExceptionReport exceptionStatus;
  };


  enum EpmBCTriggerAction {
    EkaBCUnknown,             ///< Zero initialization yields an invalid value
      EkaBCSent,                ///< All action payloads sent successfully
      EkaBCInvalidToken,        ///< Trigger security token did not match action token
      EkaBCInvalidStrategy,     ///< Strategy id was not valid
      EkaBCInvalidAction,       ///< Action id was not valid
      EkaBCDisabledAction,      ///< Did not fire because an enable bit was not set
      EkaBCSendError            ///< Send error occured (e.g., TCP session closed)
      };

  enum EkaBCOpResult {
    EKABC_OPRESULT__OK    = 0,        /** General success message */
    EKABC_OPRESULT__ALREADY_INITIALIZED = 1,
    EKABC_OPRESULT__END_OF_SESSION = 2,
    EKABC_OPRESULT__ERR_A = -100,     /** Temporary filler error message.  Replace 'A' with actual error code. */
    EKABC_OPRESULT__ERR_DOUBLE_SUBSCRIPTION = -101,     // returned by efhSubscribeStatic() if trying to subscribe to same security again
    EKABC_OPRESULT__ERR_BAD_ADDRESS = -102,     // returned if you pass NULL for something that can't be NULL, similar to EFAULT
    EKABC_OPRESULT__ERR_SYSTEM_ERROR = -103,     // returned when a system call fails and errno is set
    EKABC_OPRESULT__ERR_NOT_IMPLEMENTED = -104,     // returned when an API call is not implemented
    EKABC_OPRESULT__ERR_GROUP_NOT_AVAILABLE = -105, // returned by test feed handler when group not present in capture
    EKABC_OPRESULT__ERR_EXCHANGE_RETRANSMIT_CONNECTION = -106, // returned if exchange retransmit connection failed
    EKABC_OPRESULT__ERR_EFC_SET_CTX_ON_UNSUBSCRIBED_SECURITY = -107,
    EKABC_OPRESULT__ERR_STRIKE_PRICE_OVERFLOW = -108,
    EKABC_OPRESULT__ERR_INVALID_CONFIG = -109,

    // EPM specific
    EKABC_OPRESULT__ERR_EPM_DISABLED = -201,
    EKABC_OPRESULT__ERR_INVALID_CORE = -202,
    EKABC_OPRESULT__ERR_EPM_UNINITALIZED = -203,
    EKABC_OPRESULT__ERR_INVALID_STRATEGY = -204,
    EKABC_OPRESULT__ERR_INVALID_ACTION = -205,
    EKABC_OPRESULT__ERR_NOT_CONNECTED = -206,
    EKABC_OPRESULT__ERR_INVALID_OFFSET = -207,
    EKABC_OPRESULT__ERR_INVALID_ALIGN = -208,
    EKABC_OPRESULT__ERR_INVALID_LENGTH = -209,
    EKABC_OPRESULT__ERR_UNKNOWN_FLAG = -210,
    EKABC_OPRESULT__ERR_MAX_STRATEGIES = -211,

    // EFC specific
    EKABC_OPRESULT__ERR_EFC_DISABLED = -301,
    EKABC_OPRESULT__ERR_EFC_UNINITALIZED = -302,

    // EFH recovery specific
    EKABC_OPRESULT__RECOVERY_IN_PROGRESS = -400,
    EKABC_OPRESULT__ERR_RECOVERY_FAILED  = -401,

    // EFH TCP/Protocol Handshake specific
    EKABC_OPRESULT__ERR_TCP_SOCKET  = -501,
    EKABC_OPRESULT__ERR_UDP_SOCKET  = -502,
    EKABC_OPRESULT__ERR_PROTOCOL  = -503,
  };
  
  struct EpmBCFireReport {
    int32_t strategyId;        ///< Strategy ID the report corresponds to
    int32_t   actionId;          ///< Action ID the report corresponds to
    int32_t   triggerActionId;   ///< Action ID of the Trigger
    uint64_t      triggerToken;      ///< Security token of the Trigger
    EpmBCTriggerAction action;            ///< What device did in response to trigger
    EkaBCOpResult error;                  ///< Error code for SendError
    uint64_t preLocalEnable;    ///< Action-level enable bits before fire
    uint64_t postLocalEnable;   ///< Action-level enable bits after firing
    uint64_t preStratEnable;    ///< Strategy-level enable bits before fire
    uint64_t postStratEnable;   ///< Strategy-level enable bits after fire
    uintptr_t user;                     ///< Opaque value copied from EpmAction
    bool local;                         ///< True -> called from epmRaiseTrigger
  };

  struct EpmBCFastCancelReport {
    uint8_t         eventIsZero;       ///< Field from trigger MD
    uint8_t         numInGroup;        ///< Field from trigger MD
    uint64_t        transactTime;      ///< Field from trigger MD
    uint64_t        headerTime;        ///< Field from trigger MD
    uint32_t        sequenceNumber;    ///< Field from trigger MD
  };

  
  inline int ekaBCDecodeExceptions(char* dst,const EfcBCExceptionsReport* excpt) {
    auto d = dst;
    bool exceptionRaised = false;
    if (excpt->exceptionStatus.globalVector)
      exceptionRaised = true;;
    for(auto i = 0; i < 4; i++) { // 4 Cores
      if (excpt->exceptionStatus.portVector[i])
	exceptionRaised = true;;
    }
    if (exceptionRaised)
      d += sprintf(d,"\n\nFPGA internal exceptions:\n");
    else
      goto END;
    if ((excpt->exceptionStatus.globalVector>>0 )&0x1) d += sprintf(d,"Bit 0: SW->HW Configuration Write Corruption\n" );
    if ((excpt->exceptionStatus.globalVector>>1 )&0x1) d += sprintf(d,"Bit 1: HW->SW Dummy Packet DMA data full\n" );
    if ((excpt->exceptionStatus.globalVector>>2 )&0x1) d += sprintf(d,"Bit 2: HW->SW Dummy Packet DMA control full\n" );
    if ((excpt->exceptionStatus.globalVector>>3 )&0x1) d += sprintf(d,"Bit 3: HW->SW Report feedback DMA data full\n" );
    if ((excpt->exceptionStatus.globalVector>>4 )&0x1) d += sprintf(d,"Bit 4: HW->SW Report feedback DMA control full\n" );
    if ((excpt->exceptionStatus.globalVector>>5 )&0x1) d += sprintf(d,"Bit 5: Controller Watchdog Expired\n" );
    if ((excpt->exceptionStatus.globalVector>>6 )&0x1) d += sprintf(d,"Bit 6: SW->HW EPM Trigger control full\n" );
    if ((excpt->exceptionStatus.globalVector>>7 )&0x1) d += sprintf(d,"Bit 7: EPM Wrong Action Type\n" );
    if ((excpt->exceptionStatus.globalVector>>8 )&0x1) d += sprintf(d,"Bit 8: EPM Wrong Action Source\n" );
    if ((excpt->exceptionStatus.globalVector>>9 )&0x1) d += sprintf(d,"Bit 9: Strategy Double Fire Protection Triggered\n" );
    if ((excpt->exceptionStatus.globalVector>>10)&0x1) d += sprintf(d,"Bit 10: Strategy Context FIFO overrun\n" );
    if ((excpt->exceptionStatus.globalVector>>11)&0x1) d += sprintf(d,"Bit 11: Strategy MD vs Context Out of Sync\n" );
    if ((excpt->exceptionStatus.globalVector>>12)&0x1) d += sprintf(d,"Bit 12: Strategy Security ID mismatch\n" );
    if ((excpt->exceptionStatus.globalVector>>13)&0x1) d += sprintf(d,"Bit 13: SW->HW Strategy Update overrun\n" );
    if ((excpt->exceptionStatus.globalVector>>14)&0x1) d += sprintf(d,"Bit 14: Write Combining: WC to EPM fifo overrun\n" );
    if ((excpt->exceptionStatus.globalVector>>15)&0x1) d += sprintf(d,"Bit 15: Write Combining: CTRL 16B unaligned\n" );
    if ((excpt->exceptionStatus.globalVector>>16)&0x1) d += sprintf(d,"Bit 16: Write Combining: CTRL fragmented\n" );
    if ((excpt->exceptionStatus.globalVector>>17)&0x1) d += sprintf(d,"Bit 17: Write Combining: Missing TLP\n" );
    if ((excpt->exceptionStatus.globalVector>>18)&0x1) d += sprintf(d,"Bit 18: Write Combining: Unknown CTRL Opcode\n" );
    if ((excpt->exceptionStatus.globalVector>>19)&0x1) d += sprintf(d,"Bit 19: Write Combining: Unaligned data in TLP\n" );
    if ((excpt->exceptionStatus.globalVector>>20)&0x1) d += sprintf(d,"Bit 20: Write Combining: Unaligned data in non-TLP\n" );
    if ((excpt->exceptionStatus.globalVector>>21)&0x1) d += sprintf(d,"Bit 21: Write Combining: CTRL bitmap ff override\n" );
    if ((excpt->exceptionStatus.globalVector>>22)&0x1) d += sprintf(d,"Bit 22: Write Combining: CTRL LSB override\n" );
    if ((excpt->exceptionStatus.globalVector>>23)&0x1) d += sprintf(d,"Bit 23: Write Combining: CTRL MSB override\n" );
    if ((excpt->exceptionStatus.globalVector>>24)&0x1) d += sprintf(d,"Bit 24: Write Combining: Partial CTRL info\n" );
    if ((excpt->exceptionStatus.globalVector>>25)&0x1) d += sprintf(d,"Bit 25: Write Combining: pre-WC fifo overrun\n" );
    if ((excpt->exceptionStatus.globalVector>>26)&0x1) d += sprintf(d,"Bit 26: Write Combining: Same heap bank violation\n" );

    for(auto i = 0; i < 4; i++) { // 4 Cores
      if (excpt->exceptionStatus.portVector[i]) {
	d += sprintf(d,"\nResolving exception for Core%d\n",i);
	if ((excpt->exceptionStatus.portVector[i]>>0)&0x1)  d += sprintf(d,"Bit 0: MD Parser Error\n");
	if ((excpt->exceptionStatus.portVector[i]>>1)&0x1)  d += sprintf(d,"Bit 1: TCP Sequence Management Corruption\n");
	if ((excpt->exceptionStatus.portVector[i]>>2)&0x1)  d += sprintf(d,"Bit 2: TCP Ack Management Corruption\n");
	if ((excpt->exceptionStatus.portVector[i]>>3)&0x1)  d += sprintf(d,"Bit 3: HW->SW LWIP data drop\n");
	if ((excpt->exceptionStatus.portVector[i]>>4)&0x1)  d += sprintf(d,"Bit 4: HW->SW LWIP control drop\n");
	if ((excpt->exceptionStatus.portVector[i]>>5)&0x1)  d += sprintf(d,"Bit 5: HW->SW Sniffer data drop\n");
	if ((excpt->exceptionStatus.portVector[i]>>6)&0x1)  d += sprintf(d,"Bit 6: HW->SW Sniffer control drop\n");
      }
    }
  END:
    //  d += sprintf(d,"Arm=%d, Ver=%d\n",excpt->armStatus.armFlag,excpt->armStatus.expectedVersion);
    return d - dst;
  }


} // End of extern "C"
#endif

#ifndef _EFH_MACROS_H
#define _EFH_MACROS_H

#include <string>
#include <regex>

#include "compat/EfhMsgs.h"
#include "compat/Efh.h"

#define EFH_GET_SRC(x) (						\
			(std::regex_search(std::string(x),std::regex("NOM_ITTO"))   == true) ? EkaSource::kNOM_ITTO   : \
			(std::regex_search(std::string(x),std::regex("BX_DPTH"))    == true) ? EkaSource::kBX_DPTH    : \
			(std::regex_search(std::string(x),std::regex("PHLX_TOPO"))  == true) ? EkaSource::kPHLX_TOPO  : \
			(std::regex_search(std::string(x),std::regex("PHLX_ORD" ))  == true) ? EkaSource::kPHLX_ORD   : \
			(std::regex_search(std::string(x),std::regex("GEM_TQF"))    == true) ? EkaSource::kGEM_TQF    : \
			(std::regex_search(std::string(x),std::regex("ISE_TQF"))    == true) ? EkaSource::kISE_TQF    : \
			(std::regex_search(std::string(x),std::regex("MRX_TQF"))    == true) ? EkaSource::kMRX_TQF    : \
			(std::regex_search(std::string(x),std::regex("MRX2_TOP"))   == true) ? EkaSource::kMRX2_TOP   : \
			(std::regex_search(std::string(x),std::regex("MIAX_TOM"))   == true) ? EkaSource::kMIAX_TOM   : \
			(std::regex_search(std::string(x),std::regex("EMLD_TOM"))   == true) ? EkaSource::kEMLD_TOM   : \
			(std::regex_search(std::string(x),std::regex("PEARL_TOM"))  == true) ? EkaSource::kPEARL_TOM  : \
			(std::regex_search(std::string(x),std::regex("C1_PITCH"))   == true) ? EkaSource::kC1_PITCH    : \
			(std::regex_search(std::string(x),std::regex("C2_PITCH"))   == true) ? EkaSource::kC2_PITCH    : \
			(std::regex_search(std::string(x),std::regex("BZX_PITCH"))  == true) ? EkaSource::kBZX_PITCH   : \
			(std::regex_search(std::string(x),std::regex("EDGX_PITCH")) == true) ? EkaSource::kEDGX_PITCH  : \
			(std::regex_search(std::string(x),std::regex("ARCA_XDP"))   == true) ? EkaSource::kARCA_XDP  : \
			(std::regex_search(std::string(x),std::regex("AMEX_XDP"))   == true) ? EkaSource::kAMEX_XDP  : \
			(std::regex_search(std::string(x),std::regex("ARCA_PLR"))   == true) ? EkaSource::kARCA_PLR  : \
			(std::regex_search(std::string(x),std::regex("AMEX_PLR"))   == true) ? EkaSource::kAMEX_PLR  : \
			(std::regex_search(std::string(x),std::regex("BOX_HSVF"))   == true) ? EkaSource::kBOX_HSVF  : \
			(std::regex_search(std::string(x),std::regex("CME_SBE"))    == true) ? EkaSource::kCME_SBE  : \
			EkaSource::kInvalid)

#define EFH_EXCH2FEED(x)			(	\
  (x == EkaSource::kNOM_ITTO)   ? EfhFeedVer::kNASDAQ :	\
  (x == EkaSource::kBX_DPTH)    ? EfhFeedVer::kBX :	\
  (x == EkaSource::kGEM_TQF)    ? EfhFeedVer::kGEMX :	\
  (x == EkaSource::kISE_TQF)    ? EfhFeedVer::kGEMX :	\
  (x == EkaSource::kMRX_TQF)    ? EfhFeedVer::kGEMX :	\
  (x == EkaSource::kMRX2_TOP)   ? EfhFeedVer::kMRX2_TOP :	\
  (x == EkaSource::kARCA_XDP)   ? EfhFeedVer::kXDP :	\
  (x == EkaSource::kAMEX_XDP)   ? EfhFeedVer::kXDP :	\
  (x == EkaSource::kARCA_PLR)   ? EfhFeedVer::kPLR :	\
  (x == EkaSource::kAMEX_PLR)   ? EfhFeedVer::kPLR :	\
  (x == EkaSource::kPHLX_TOPO)  ? EfhFeedVer::kPHLX :	\
  (x == EkaSource::kPHLX_ORD)   ? EfhFeedVer::kPHLX :	\
  (x == EkaSource::kMIAX_TOM)   ? EfhFeedVer::kMIAX :   \
  (x == EkaSource::kEMLD_TOM)   ? EfhFeedVer::kMIAX :   \
  (x == EkaSource::kPEARL_TOM)  ? EfhFeedVer::kMIAX :   \
  (x == EkaSource::kC1_PITCH)   ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kC2_PITCH)   ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kBZX_PITCH)  ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kEDGX_PITCH) ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kBOX_HSVF)   ? EfhFeedVer::kBOX :	\
  (x == EkaSource::kCME_SBE)    ? EfhFeedVer::kCME :	\
  EfhFeedVer::kInvalid)

#define EFH_EXCH2FULL_BOOK(x) ((x == EkaSource::kNOM_ITTO) || (x == EkaSource::kBX_DPTH) || (x == EkaSource::kC2_PITCH) || (x == EkaSource::kBZX_PITCH) || (x == EkaSource::kEDGX_PITCH))

#define EKA_EXCH_DECODE(x) (                     \
  (x == EkaSource::kNOM_ITTO)   ? "NOM_ITTO"   : \
  (x == EkaSource::kBX_DPTH)    ? "BX_DPTH"    : \
  (x == EkaSource::kGEM_TQF)    ? "GEM_TQF"    : \
  (x == EkaSource::kISE_TQF)    ? "ISE_TQF"    : \
  (x == EkaSource::kMRX_TQF)    ? "MRX_TQF"    : \
  (x == EkaSource::kMRX2_TOP)   ? "MRX2_TOP"   : \
  (x == EkaSource::kPHLX_TOPO)  ? "PHLX_TOPO"  : \
  (x == EkaSource::kPHLX_ORD)   ? "PHLX_ORD"   : \
  (x == EkaSource::kARCA_XDP)   ? "ARCA_XDP"   : \
  (x == EkaSource::kAMEX_XDP)   ? "AMEX_XDP"   : \
  (x == EkaSource::kARCA_PLR)   ? "ARCA_PLR"   : \
  (x == EkaSource::kAMEX_PLR)   ? "AMEX_PLR"   : \
  (x == EkaSource::kC1_PITCH)   ? "C1_PITCH"   : \
  (x == EkaSource::kC2_PITCH)   ? "C2_PITCH"   : \
  (x == EkaSource::kBZX_PITCH)  ? "BZX_PITCH"  : \
  (x == EkaSource::kEDGX_PITCH) ? "EDGX_PITCH" : \
  (x == EkaSource::kMIAX_TOM)   ? "MIAX_TOM"   : \
  (x == EkaSource::kEMLD_TOM)   ? "EMLD_TOM"   : \
  (x == EkaSource::kPEARL_TOM)  ? "PEARL_TOM"  : \
  (x == EkaSource::kBOX_HSVF)   ? "BOX_HSVF"   : \
  (x == EkaSource::kCME_SBE)    ? "CME_SBE"    : \
  "UNKNOWN")

#define EKA_EXCH_SOURCE_DECODE(x) (            \
  (x == EkaSource::kNOM_ITTO)   ? "NOM"      : \
  (x == EkaSource::kBX_DPTH)    ? "BX"       : \
  (x == EkaSource::kGEM_TQF)    ? "GEM"      : \
  (x == EkaSource::kISE_TQF)    ? "ISE"      : \
  (x == EkaSource::kMRX_TQF)    ? "MRX"      : \
  (x == EkaSource::kMRX2_TOP)   ? "MRX2_TOP" : \
  (x == EkaSource::kPHLX_TOPO)  ? "PHLX"     : \
  (x == EkaSource::kPHLX_ORD)   ? "PHLX_ORD" : \
  (x == EkaSource::kARCA_XDP)   ? "ARCA"     : \
  (x == EkaSource::kAMEX_XDP)   ? "AMEX"     : \
  (x == EkaSource::kARCA_PLR)   ? "ARCA"     : \
  (x == EkaSource::kAMEX_PLR)   ? "AMEX"     : \
  (x == EkaSource::kC1_PITCH)   ? "C1"       : \
  (x == EkaSource::kC2_PITCH)   ? "C2"       : \
  (x == EkaSource::kBZX_PITCH)  ? "BZX"      : \
  (x == EkaSource::kEDGX_PITCH) ? "EDGX"     : \
  (x == EkaSource::kMIAX_TOM)   ? "MIAX"     : \
  (x == EkaSource::kEMLD_TOM)   ? "EMLD"     : \
  (x == EkaSource::kPEARL_TOM)  ? "PEARL"    : \
  (x == EkaSource::kBOX_HSVF)   ? "BOX"      : \
  (x == EkaSource::kCME_SBE)    ? "CME"      : \
  "UNKNOWN")


#define EKA_GRP_SRC2EXCH(x) (				   \
  (x == EkaSource::kNOM_ITTO)   ? EfhExchange::kNOM   :	   \
  (x == EkaSource::kBX_DPTH)    ? EfhExchange::kBX    :	   \
  (x == EkaSource::kGEM_TQF)    ? EfhExchange::kGEM   :	   \
  (x == EkaSource::kISE_TQF)    ? EfhExchange::kISE   :	   \
  (x == EkaSource::kMRX_TQF)    ? EfhExchange::kMCRY  :	   \
  (x == EkaSource::kMRX2_TOP)   ? EfhExchange::kMCRY  :	   \
  (x == EkaSource::kPHLX_TOPO)  ? EfhExchange::kPHLX  :	   \
  (x == EkaSource::kPHLX_ORD)   ? EfhExchange::kPHLX  :	   \
  (x == EkaSource::kMIAX_TOM)   ? EfhExchange::kMIAX  :	   \
  (x == EkaSource::kEMLD_TOM)   ? EfhExchange::kEMLD  :	   \
  (x == EkaSource::kPEARL_TOM)  ? EfhExchange::kPEARL :	   \
  (x == EkaSource::kC1_PITCH)   ? EfhExchange::kCboe  :	   \
  (x == EkaSource::kC2_PITCH)   ? EfhExchange::kCboe2 :	   \
  (x == EkaSource::kBZX_PITCH)  ? EfhExchange::kBATS  :	   \
  (x == EkaSource::kEDGX_PITCH) ? EfhExchange::kEDGX  :	   \
  (x == EkaSource::kAMEX_XDP)   ? EfhExchange::kAOE   :	   \
  (x == EkaSource::kARCA_XDP)   ? EfhExchange::kPCX   :	   \
  (x == EkaSource::kAMEX_PLR)   ? EfhExchange::kAOE   :	   \
  (x == EkaSource::kARCA_PLR)   ? EfhExchange::kPCX   :	   \
  (x == EkaSource::kBOX_HSVF)   ? EfhExchange::kBOX   :	   \
  (x == EkaSource::kCME_SBE)    ? EfhExchange::kCME   :	   \
   EfhExchange::kUnknown)

#define EKA_CTS_SOURCE(x)						\
  x == EkaSource::kNOM_ITTO ? "OptionMarketDataNASDAQ4" :		\
    x == EkaSource::kPHLX_TOPO ? "OptionMarketDataPHLX4" :		\
    x == EkaSource::kGEM_TQF ? "OptionMarketDataGEM4" :			\
    x == EkaSource::kISE_TQF ? "OptionMarketDataISE6" :			\
    x == EkaSource::kC1_PITCH ? "OptionMarketDataC1OX3" :		\
    x == EkaSource::kC2_PITCH ? "OptionMarketDataC2OX3" :		\
    "Unknown"

#define EKA_PRINT_GRP(x) ((std::string(EKA_EXCH_DECODE((EkaSource)(((EkaGroup*)x)->source))) + '_' + std::to_string((uint)((EkaGroup*)x)->localId)).c_str())

#define EKA_FEED_VER_DECODE(x) \
  x == EfhFeedVer::kNASDAQ   ? "NASDAQ"      : \
  x == EfhFeedVer::kBX       ? "BX"          : \
  x == EfhFeedVer::kGEMX     ? "GEMX"        : \
  x == EfhFeedVer::kPHLX     ? "PHLX"        : \
  x == EfhFeedVer::kMIAX     ? "MIAX"        : \
  x == EfhFeedVer::kBATS     ? "BATS"        : \
  x == EfhFeedVer::kCBOE     ? "CBOE"        : \
  x == EfhFeedVer::kXDP      ? "XDP"         : \
  x == EfhFeedVer::kPLR      ? "PLR"         : \
  x == EfhFeedVer::kBOX      ? "BOX"         : \
  x == EfhFeedVer::kCME      ? "CME"         : \
  x == EfhFeedVer::kNEWS     ? "NEWS"         : \
                               "UNKNOWN"

#define EKA_TS_DECODE(x) \
  x == EfhTradeStatus::kUninit     ? '_' : \
  x == EfhTradeStatus::kGapRecover ? 'G' : \
  x == EfhTradeStatus::kHalted     ? 'H' : \
  x == EfhTradeStatus::kPreopen    ? 'P' : \
  x == EfhTradeStatus::kNormal     ? 'N' : \
  x == EfhTradeStatus::kClosed     ? 'C' : \
  'X'

inline int strikePriceScaleFactor (EkaSource exch) {
  switch (EFH_EXCH2FEED(exch)) {
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kBATS:
  case EfhFeedVer::kBOX  : return 10;
  case EfhFeedVer::kGEMX : return 10000;
  default:                 return 1;    
  }
}

inline void eka_create_avt_definition (char* dst, const EfhOptionDefinitionMsg* msg) {
  if (msg->header.group.source  == EkaSource::kCME_SBE && msg->commonDef.securityType == EfhSecurityType::kOption) {
    std::string classSymbol    = std::string(msg->commonDef.classSymbol,sizeof(msg->commonDef.classSymbol));
    sprintf(dst,"%s_%c%04jd",
	    classSymbol.c_str(),
	    msg->optionType == EfhOptionType::kCall ? 'C' : 'P',
	    msg->strikePrice);
  } else {
    uint8_t y,m,d;

    d = msg->commonDef.expiryDate % 100;
    m = ((msg->commonDef.expiryDate - d) / 100) % 100;
    y = msg->commonDef.expiryDate / 10000 - 2000;

    memcpy(dst,msg->commonDef.underlying,6);
    for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_';
    char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P';
    sprintf(dst+6,"%02u%02u%02u%c%08jd",y,m,d,call_put,msg->strikePrice / strikePriceScaleFactor(msg->header.group.source));
  }
  return;
}

inline char printEfhSecurityType(EfhSecurityType t) {
  switch (t) {
  case EfhSecurityType::kInvalid : return ' ';
  case EfhSecurityType::kIndex   : return 'I';
  case EfhSecurityType::kStock   : return 'S';
  case EfhSecurityType::kOption  : return 'O';
  case EfhSecurityType::kFuture  : return 'F';
  case EfhSecurityType::kRfq     : return 'R';
  case EfhSecurityType::kComplex : return 'C';
  default:
    return '-';
  }
}

inline char printEfhOrderSide(EfhOrderSide s) {
  switch (s) {
  case EfhOrderSide::kOther : return 'O';
  case EfhOrderSide::kCross : return 'C';
  case EfhOrderSide::kErr   : return 'E';
  case EfhOrderSide::kBid   : return 'B';
  case EfhOrderSide::kAsk   : return 'A';
  default :
    return '-';
  }
}

#endif

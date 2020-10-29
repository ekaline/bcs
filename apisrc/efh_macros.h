#ifndef _EFH_MACROS_H
#define _EFH_MACROS_H

#include <string>
#include <regex>

#define EFH_GET_SRC(x) (						\
			(std::regex_search(std::string(x),std::regex("NOM_ITTO"))   == true) ? EkaSource::kNOM_ITTO   : \
			(std::regex_search(std::string(x),std::regex("PHLX_TOPO"))  == true) ? EkaSource::kPHLX_TOPO  : \
			(std::regex_search(std::string(x),std::regex("PHLX_ORD" ))  == true) ? EkaSource::kPHLX_ORD   : \
			(std::regex_search(std::string(x),std::regex("GEM_TQF"))    == true) ? EkaSource::kGEM_TQF    : \
			(std::regex_search(std::string(x),std::regex("ISE_TQF"))    == true) ? EkaSource::kISE_TQF    : \
			(std::regex_search(std::string(x),std::regex("MRX_TQF"))    == true) ? EkaSource::kMRX_TQF    : \
			(std::regex_search(std::string(x),std::regex("MIAX_TOM"))   == true) ? EkaSource::kMIAX_TOM   : \
			(std::regex_search(std::string(x),std::regex("PEARL_TOM"))  == true) ? EkaSource::kPEARL_TOM  : \
			(std::regex_search(std::string(x),std::regex("C1_PITCH"))   == true) ? EkaSource::kC1_PITCH    : \
			(std::regex_search(std::string(x),std::regex("C2_PITCH"))   == true) ? EkaSource::kC2_PITCH    : \
			(std::regex_search(std::string(x),std::regex("BZX_PITCH"))  == true) ? EkaSource::kBZX_PITCH   : \
			(std::regex_search(std::string(x),std::regex("EDGX_PITCH")) == true) ? EkaSource::kEDGX_PITCH  : \
			(std::regex_search(std::string(x),std::regex("ARCA_XDP"))   == true) ? EkaSource::kARCA_XDP  : \
			(std::regex_search(std::string(x),std::regex("AMEX_XDP"))   == true) ? EkaSource::kAMEX_XDP  : \
			(std::regex_search(std::string(x),std::regex("BOX_HSVF"))   == true) ? EkaSource::kBOX_HSVF  : \
			EkaSource::kInvalid)

#define EFH_EXCH2FEED(x)			(	\
  (x == EkaSource::kNOM_ITTO)   ? EfhFeedVer::kNASDAQ :	\
  (x == EkaSource::kGEM_TQF)    ? EfhFeedVer::kGEMX :	\
  (x == EkaSource::kISE_TQF)    ? EfhFeedVer::kGEMX :	\
  (x == EkaSource::kMRX_TQF)    ? EfhFeedVer::kGEMX :	\
  (x == EkaSource::kARCA_XDP)   ? EfhFeedVer::kXDP :	\
  (x == EkaSource::kAMEX_XDP)   ? EfhFeedVer::kXDP :	\
  (x == EkaSource::kPHLX_TOPO)  ? EfhFeedVer::kPHLX :	\
  (x == EkaSource::kPHLX_ORD)   ? EfhFeedVer::kPHLX :	\
  (x == EkaSource::kMIAX_TOM)   ? EfhFeedVer::kMIAX :	\
  (x == EkaSource::kPEARL_TOM)  ? EfhFeedVer::kMIAX :	\
  (x == EkaSource::kC1_PITCH)   ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kC2_PITCH)   ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kBZX_PITCH)  ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kEDGX_PITCH) ? EfhFeedVer::kBATS :	\
  (x == EkaSource::kBOX_HSVF)   ? EfhFeedVer::kBOX :	\
  EfhFeedVer::kInvalid)

#define EFH_EXCH2FULL_BOOK(x) ((x == EkaSource::kNOM_ITTO) || (x == EkaSource::kC2_PITCH) || (x == EkaSource::kBZX_PITCH) || (x == EkaSource::kEDGX_PITCH))

#define EKA_EXCH_DECODE(x) (				   \
  (x == EkaSource::kNOM_ITTO)   ? "NOM_ITTO"   :	   \
  (x == EkaSource::kGEM_TQF)    ? "GEM_TQF"    :	   \
  (x == EkaSource::kISE_TQF)    ? "ISE_TQF"    :	   \
  (x == EkaSource::kMRX_TQF)    ? "MRX_TQF"    :	   \
  (x == EkaSource::kPHLX_TOPO)  ? "PHLX_TOPO"  :	   \
  (x == EkaSource::kPHLX_ORD)   ? "PHLX_ORD"   :	   \
  (x == EkaSource::kARCA_XDP)   ? "ARCA_XDP"   :	   \
  (x == EkaSource::kAMEX_XDP)   ? "AMEX_XDP"   :	   \
  (x == EkaSource::kC1_PITCH)   ? "C1_PITCH"   :	   \
  (x == EkaSource::kC2_PITCH)   ? "C2_PITCH"   :	   \
  (x == EkaSource::kBZX_PITCH)  ? "BZX_PITCH"  :	   \
  (x == EkaSource::kEDGX_PITCH) ? "EDGX_PITCH" :	   \
  (x == EkaSource::kMIAX_TOM)   ? "MIAX_TOM"   :	   \
  (x == EkaSource::kPEARL_TOM)  ? "PEARL_TOM"  :	   \
  (x == EkaSource::kBOX_HSVF)   ? "BOX_HSVF"  :	   \
  "UNKNOWN")

#define EKA_EXCH_SOURCE_DECODE(x) (					\
				   (x == EkaSource::kNOM_ITTO)   ? "NOM"   : \
				   (x == EkaSource::kGEM_TQF)    ? "GEM"    : \
				   (x == EkaSource::kISE_TQF)    ? "ISE"    : \
				   (x == EkaSource::kMRX_TQF)    ? "MRX"    : \
				   (x == EkaSource::kPHLX_TOPO)  ? "PHLX"  : \
				   (x == EkaSource::kPHLX_ORD)   ? "PHLX_ORD"  : \
				   (x == EkaSource::kARCA_XDP)   ? "ARCA"   : \
				   (x == EkaSource::kAMEX_XDP)   ? "AMEX"   : \
				   (x == EkaSource::kC1_PITCH)   ? "C1"   : \
				   (x == EkaSource::kC2_PITCH)   ? "C2"   : \
				   (x == EkaSource::kBZX_PITCH)  ? "BZX"  : \
				   (x == EkaSource::kEDGX_PITCH) ? "EDGX" : \
				   (x == EkaSource::kMIAX_TOM)   ? "MIAX"   : \
				   (x == EkaSource::kPEARL_TOM)  ? "PEARL"  : \
				   (x == EkaSource::kBOX_HSVF)   ? "BOX"  : \
				   "UNKNOWN")


#define EKA_GRP_SRC2EXCH(x) (				   \
  (x == EkaSource::kNOM_ITTO)   ? EfhExchange::kNOM   :	   \
  (x == EkaSource::kGEM_TQF)    ? EfhExchange::kGEM   :	   \
  (x == EkaSource::kISE_TQF)    ? EfhExchange::kISE   :	   \
  (x == EkaSource::kMRX_TQF)    ? EfhExchange::kMCRY  :	   \
  (x == EkaSource::kPHLX_TOPO)  ? EfhExchange::kPHLX  :	   \
  (x == EkaSource::kPHLX_ORD)   ? EfhExchange::kPHLX  :	   \
  (x == EkaSource::kMIAX_TOM)   ? EfhExchange::kMIAX  :	   \
  (x == EkaSource::kPEARL_TOM)  ? EfhExchange::kPEARL :	   \
  (x == EkaSource::kC1_PITCH)   ? EfhExchange::kCboe  :	   \
  (x == EkaSource::kC2_PITCH)   ? EfhExchange::kCboe2 :	   \
  (x == EkaSource::kBZX_PITCH)  ? EfhExchange::kBATS  :	   \
  (x == EkaSource::kEDGX_PITCH) ? EfhExchange::kEDGX  :	   \
  (x == EkaSource::kAMEX_XDP)   ? EfhExchange::kAOE   :	   \
  (x == EkaSource::kARCA_XDP)   ? EfhExchange::kPCX   :	   \
  (x == EkaSource::kBOX_HSVF)   ? EfhExchange::kBOX   :	   \
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
              x == EfhFeedVer::kGEMX     ? "GEMX"        : \
              x == EfhFeedVer::kPHLX     ? "PHLX"        : \
              x == EfhFeedVer::kMIAX     ? "MIAX"        : \
              x == EfhFeedVer::kBATS     ? "BATS"        : \
              x == EfhFeedVer::kXDP      ? "XDP"        : \
              x == EfhFeedVer::kBOX      ? "BOX"        : \
                                           "UNKNOWN"
#define EKA_TS_DECODE(x) \
  x == EfhTradeStatus::kUninit       ? '_' :	\
    x == EfhTradeStatus::kGapRecover ? 'G' :	\
    x == EfhTradeStatus::kHalted     ? 'H' :	\
    x == EfhTradeStatus::kPreopen    ? 'P' :	\
    x == EfhTradeStatus::kNormal     ? 'N' :	\
    x == EfhTradeStatus::kClosed     ? 'C' :	\
    'X'

#define EKA_OPRA_TC_DECODE(x)	\
  x == ' ' ? EfhTradeCond::kReg :		\
    x == 'A' ? EfhTradeCond::kCanc :		\
    x == 'B' ? EfhTradeCond::kOseq :		\
    x == 'C' ? EfhTradeCond::kCncl :		\
    x == 'D' ? EfhTradeCond::kLate :		\
    x == 'F' ? EfhTradeCond::kOpen :		\
    x == 'G' ? EfhTradeCond::kCnol :		\
    x == 'H' ? EfhTradeCond::kOpnl :		\
    x == 'I' ? EfhTradeCond::kAuto :		\
    x == 'J' ? EfhTradeCond::kReop :		\
    x == 'K' ? EfhTradeCond::kAjst :		\
    x == 'L' ? EfhTradeCond::kSprd :		\
    x == 'M' ? EfhTradeCond::kStdl :		\
    x == 'N' ? EfhTradeCond::kStdp :		\
    x == 'O' ? EfhTradeCond::kCstp :		\
    x == 'Q' ? EfhTradeCond::kCmbo :		\
    x == 'R' ? EfhTradeCond::kSpim :		\
    x == 'S' ? EfhTradeCond::kIsoi :		\
    x == 'T' ? EfhTradeCond::kBnmt :		\
    x == 'X' ? EfhTradeCond::kXmpt :		\
    EfhTradeCond::kUnmapped

#endif

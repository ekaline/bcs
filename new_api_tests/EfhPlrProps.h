#ifndef _EFH_PLR_PROPS_H
#define _EFH_PLR_PROPS_H

// CERT
EkaProp efhArcaPlrInitCtxEntries_A[] = {
    {"efh.ARCA_PLR.group.0.products","vanilla_book"},
    {"efh.ARCA_PLR.group.0.mcast.addr",      "224.0.60.9:41021"},
    {"efh.ARCA_PLR.group.0.refresh.tcpAddr", "162.68.19.122:35050"},
    {"efh.ARCA_PLR.group.0.refresh.udpAddr", "224.0.60.15:41027"},
    {"efh.ARCA_PLR.group.0.retrans.tcpAddr", "162.68.19.122:35050"},
    {"efh.ARCA_PLR.group.0.retrans.udpAddr", "224.0.60.12:41024"},
    {"efh.ARCA_PLR.group.0.ChannelId","51"},
    {"efh.ARCA_PLR.group.0.SourceId","ARGTSXD01"},

    {"efh.ARCA_PLR.group.1.products","vanilla_trades"},
    {"efh.ARCA_PLR.group.1.mcast.addr",      "224.0.60.18:41041"},
    {"efh.ARCA_PLR.group.1.refresh.tcpAddr", "162.68.19.122:35050"},
    {"efh.ARCA_PLR.group.1.refresh.udpAddr", "224.0.60.20:41043"},
    {"efh.ARCA_PLR.group.1.retrans.tcpAddr", "162.68.19.122:35050"},
    {"efh.ARCA_PLR.group.1.retrans.udpAddr", "224.0.60.19:41042"},
    {"efh.ARCA_PLR.group.1.ChannelId","1"},
    {"efh.ARCA_PLR.group.1.SourceId","ARGTSXD01"},
 
    {"efh.ARCA_PLR.group.2.products","complex_book"},
    {"efh.ARCA_PLR.group.2.mcast.addr",      "224.0.60.27:41071"},
    {"efh.ARCA_PLR.group.2.refresh.tcpAddr", "162.68.19.122:35050"},
    {"efh.ARCA_PLR.group.2.refresh.udpAddr", "224.0.60.33:41077"},
    {"efh.ARCA_PLR.group.2.retrans.tcpAddr", "162.68.19.122:35050"},
    {"efh.ARCA_PLR.group.2.retrans.udpAddr", "224.0.60.30:41074"},
    {"efh.ARCA_PLR.group.2.ChannelId","51"},
    {"efh.ARCA_PLR.group.2.SourceId","ARGTSXD01"},
               
};

EkaProp efhArcaPlrInitCtxEntries_B[] = {
	{"efh.ARCA_PLR.group.0.mcast.addr",      "224.0.60.9:41021"},
	{"efh.ARCA_PLR.group.0.refresh.tcpAddr", "162.68.19.122:35050"},
	{"efh.ARCA_PLR.group.0.refresh.udpAddr", "224.0.60.15:41027"},
	{"efh.ARCA_PLR.group.0.retrans.tcpAddr", "162.68.19.122:35050"},
	{"efh.ARCA_PLR.group.0.retrans.udpAddr", "224.0.60.12:41024"},
	{"efh.ARCA_PLR.group.0.ChannelId","51"},
	{"efh.ARCA_PLR.group.0.SourceId","ARGTSXD01"},

};

EkaProp efhAmexPlrInitCtxEntries_A[] = {
	{"efh.AMEX_PLR.group.0.mcast.addr",      "224.0.60.9:41021"},
	{"efh.AMEX_PLR.group.0.refresh.tcpAddr", "162.68.19.122:35050"},
	{"efh.AMEX_PLR.group.0.refresh.udpAddr", "224.0.60.15:41027"},
	{"efh.AMEX_PLR.group.0.retrans.tcpAddr", "162.68.19.122:35050"},
	{"efh.AMEX_PLR.group.0.retrans.udpAddr", "224.0.60.12:41024"},
	{"efh.AMEX_PLR.group.0.ChannelId","51"},
	{"efh.AMEX_PLR.group.0.SourceId","ARGTSXD01"},

};

EkaProp efhAmexPlrInitCtxEntries_B[] = {
	{"efh.AMEX_PLR.group.0.mcast.addr",      "224.0.60.9:41021"},
	{"efh.AMEX_PLR.group.0.refresh.tcpAddr", "162.68.19.122:35050"},
	{"efh.AMEX_PLR.group.0.refresh.udpAddr", "224.0.60.15:41027"},
	{"efh.AMEX_PLR.group.0.retrans.tcpAddr", "162.68.19.122:35050"},
	{"efh.AMEX_PLR.group.0.retrans.udpAddr", "224.0.60.12:41024"},
	{"efh.AMEX_PLR.group.0.ChannelId","51"},
	{"efh.AMEX_PLR.group.0.SourceId","ARGTSXD01"},

};

const EkaGroup arcaPlrGroups[] = {
	{EkaSource::kARCA_PLR, (EkaLSI)0},
	{EkaSource::kARCA_PLR, (EkaLSI)1},
	{EkaSource::kARCA_PLR, (EkaLSI)2},
};
const EkaGroup amexPlrGroups[] = {
	{EkaSource::kAMEX_PLR, (EkaLSI)0},
	{EkaSource::kAMEX_PLR, (EkaLSI)1},
	{EkaSource::kAMEX_PLR, (EkaLSI)2},
};
#endif

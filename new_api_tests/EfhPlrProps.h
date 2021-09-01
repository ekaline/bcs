#ifndef _EFH_PLR_PROPS_H
#define _EFH_PLR_PROPS_H

EkaProp efhArcaPlrInitCtxEntries_A[] = {
	{"efh.ARCA_PLR.group.0.mcast.addr",      "224.0.60.9:41021"},
	{"efh.ARCA_PLR.group.0.refresh.tcpAddr", "162.68.19.122:35050"},
	{"efh.ARCA_PLR.group.0.refresh.udpAddr", "224.0.60.15:41027"},
	{"efh.ARCA_PLR.group.0.retrans.tcpAddr", "162.68.19.122:35050"},
	{"efh.ARCA_PLR.group.0.retrans.udpAddr", "224.0.60.12:41024"},
	{"efh.ARCA_PLR.group.0.ChannelId","51"},
	{"efh.ARCA_PLR.group.0.SourceId","ARGTSXD01"},

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
};
const EkaGroup amexPlrGroups[] = {
	{EkaSource::kAMEX_PLR, (EkaLSI)0},
};
#endif

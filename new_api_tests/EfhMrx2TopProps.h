#ifndef _EFH_MRX2TOP_DPTH__PROPS_H
#define _EFH_MRX2TOP_DPTH__PROPS_H

EkaProp efhMrx2TopInitCtxEntries_A[] = {
// Top Feed used for Quotes and Trade Statuses only
	{"efh.MRX2_TOP.group.0.products","vanilla_book"},
	{"efh.MRX2_TOP.group.0.mcast.addr"   ,"233.104.73.0:18000"},
	{"efh.MRX2_TOP.group.0.recovery.addr","206.200.182.64:18100"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.0.snapshot.addr","206.200.182.72:18300"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.0.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.1.products","vanilla_book"},
	{"efh.MRX2_TOP.group.1.mcast.addr"   ,"233.104.73.1:18001"},
	{"efh.MRX2_TOP.group.1.recovery.addr","206.200.182.66:18101"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.1.snapshot.addr","206.200.182.73:18301"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.1.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.2.products","vanilla_book"},
	{"efh.MRX2_TOP.group.2.mcast.addr"   ,"233.104.73.2:18002"},
	{"efh.MRX2_TOP.group.2.recovery.addr","206.200.182.68:18102"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.2.snapshot.addr","206.200.182.74:18302"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.2.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.3.products","vanilla_book"},
	{"efh.MRX2_TOP.group.3.mcast.addr"   ,"233.104.73.3:18003"},
	{"efh.MRX2_TOP.group.3.recovery.addr","206.200.182.70:18103"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.3.snapshot.addr","206.200.182.75:18303"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.3.snapshot.auth","MGGT04:UWYYFCYI"},

// Order Feed used for SQFs only
	{"efh.MRX2_TOP.group.4.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.4.mcast.addr"   ,"233.104.73.8:18008"},
	{"efh.MRX2_TOP.group.4.recovery.addr","206.200.182.88:18108"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.4.snapshot.addr","206.200.182.72:18300"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.4.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.5.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.5.mcast.addr"   ,"233.104.73.9:18009"},
	{"efh.MRX2_TOP.group.5.recovery.addr","206.200.182.91:18109"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.5.snapshot.addr","206.200.182.73:18301"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.5.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.6.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.6.mcast.addr"   ,"233.104.73.10:18010"},
	{"efh.MRX2_TOP.group.6.recovery.addr","206.200.182.94:18110"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.6.snapshot.addr","206.200.182.74:18302"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.6.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.7.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.7.mcast.addr"   ,"233.104.73.11:18011"},
	{"efh.MRX2_TOP.group.7.recovery.addr","206.200.182.97:18111"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.7.snapshot.addr","206.200.182.75:18303"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.7.snapshot.auth","MGGT04:UWYYFCYI"},

// Trade Feed used for Trades only
	{"efh.MRX2_TOP.group.8.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.8.mcast.addr"   ,"233.104.73.12:18012"},
	{"efh.MRX2_TOP.group.8.recovery.addr","206.200.182.100:18112"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.8.snapshot.addr","206.200.182.72:18300"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.8.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.9.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.9.mcast.addr"   ,"233.104.73.13:18013"},
	{"efh.MRX2_TOP.group.9.recovery.addr","206.200.182.103:18113"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.9.snapshot.addr","206.200.182.73:18301"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.9.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.10.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.10.mcast.addr"   ,"233.104.73.14:18014"},
	{"efh.MRX2_TOP.group.10.recovery.addr","206.200.182.105:18114"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.10.snapshot.addr","206.200.182.74:18302"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.10.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.11.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.11.mcast.addr"   ,"233.104.73.15:18015"},
	{"efh.MRX2_TOP.group.11.recovery.addr","206.200.182.107:18115"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.11.snapshot.addr","206.200.182.75:18303"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.11.snapshot.auth","MGGT04:UWYYFCYI"},

};

EkaProp efhMrx2TopInitCtxEntries_B[] = {
// Top Feed used for Quotes and Trade Statuses only
	{"efh.MRX2_TOP.group.0.products","vanilla_book"},
	{"efh.MRX2_TOP.group.0.mcast.addr"   ,"233.127.56.0:18000"},
	{"efh.MRX2_TOP.group.0.recovery.addr","206.200.182.192:18100"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.0.snapshot.addr","206.200.182.72:18300"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.0.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.1.products","vanilla_book"},
	{"efh.MRX2_TOP.group.1.mcast.addr"   ,"233.127.56.1:18001"},
	{"efh.MRX2_TOP.group.1.recovery.addr","206.200.182.194:18101"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.1.snapshot.addr","206.200.182.73:18301"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.1.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.2.products","vanilla_book"},
	{"efh.MRX2_TOP.group.2.mcast.addr"   ,"233.127.56.2:18002"},
	{"efh.MRX2_TOP.group.2.recovery.addr","206.200.182.196:18102"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.2.snapshot.addr","206.200.182.74:18302"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.2.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.3.products","vanilla_book"},
	{"efh.MRX2_TOP.group.3.mcast.addr"   ,"233.127.56.3:18003"},
	{"efh.MRX2_TOP.group.3.recovery.addr","206.200.182.198:18103"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.3.snapshot.addr","206.200.182.75:18303"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.3.snapshot.auth","MGGT04:UWYYFCYI"},

// Order Feed used for SQFs only
	{"efh.MRX2_TOP.group.4.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.4.mcast.addr"   ,"233.127.56.8:18008"},
	{"efh.MRX2_TOP.group.4.recovery.addr","206.200.182.216:18108"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.4.snapshot.addr","206.200.182.217:18108"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.4.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.5.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.5.mcast.addr"   ,"233.127.56.9:18009"},
	{"efh.MRX2_TOP.group.5.recovery.addr","206.200.182.219:18109"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.5.snapshot.addr","206.200.182.220:18109"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.5.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.6.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.6.mcast.addr"   ,"233.127.56.10:18010"},
	{"efh.MRX2_TOP.group.6.recovery.addr","206.200.182.222:18110"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.6.snapshot.addr","206.200.182.223:18110"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.6.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.7.products","vanilla_auction"},
	{"efh.MRX2_TOP.group.7.mcast.addr"   ,"233.127.56.11:18011"},
	{"efh.MRX2_TOP.group.7.recovery.addr","206.200.182.225:18111"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.7.snapshot.addr","206.200.182.226:18111"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.7.snapshot.auth","MGGT04:UWYYFCYI"},

// Trade Feed used for Trades only
	{"efh.MRX2_TOP.group.8.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.8.mcast.addr"   ,"233.127.56.12:18012"},
	{"efh.MRX2_TOP.group.8.recovery.addr","206.200.182.228:18112"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.8.snapshot.addr","206.200.182.229:18112"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.8.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.9.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.9.mcast.addr"   ,"233.127.56.13:18013"},
	{"efh.MRX2_TOP.group.9.recovery.addr","206.200.182.230:18113"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.9.snapshot.addr","206.200.182.231:18113"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.9.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.10.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.10.mcast.addr"   ,"233.127.56.14:18014"},
	{"efh.MRX2_TOP.group.10.recovery.addr","206.200.182.232:18114"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.10.snapshot.addr","206.200.182.233:18114"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.10.snapshot.auth","MGGT04:UWYYFCYI"},

	{"efh.MRX2_TOP.group.11.products","vanilla_trades"},
	{"efh.MRX2_TOP.group.11.mcast.addr"   ,"233.127.56.15:18015"},
	{"efh.MRX2_TOP.group.11.recovery.addr","206.200.182.234:18115"}, 	// MOLD RECOVERY
	{"efh.MRX2_TOP.group.11.snapshot.addr","206.200.182.235:18115"}, 	// TCP GLIMPSE/SOUPBIN
	{"efh.MRX2_TOP.group.11.snapshot.auth","MGGT04:UWYYFCYI"},

};

const EkaGroup mrx2TopGroups[] = {
	{EkaSource::kMRX2_TOP, (EkaLSI)0},
	{EkaSource::kMRX2_TOP, (EkaLSI)1},
	{EkaSource::kMRX2_TOP, (EkaLSI)2},
	{EkaSource::kMRX2_TOP, (EkaLSI)3},
	{EkaSource::kMRX2_TOP, (EkaLSI)4},
	{EkaSource::kMRX2_TOP, (EkaLSI)5},
	{EkaSource::kMRX2_TOP, (EkaLSI)6},
	{EkaSource::kMRX2_TOP, (EkaLSI)7},
	{EkaSource::kMRX2_TOP, (EkaLSI)8},
	{EkaSource::kMRX2_TOP, (EkaLSI)9},
	{EkaSource::kMRX2_TOP, (EkaLSI)10},
	{EkaSource::kMRX2_TOP, (EkaLSI)11},
};

#endif

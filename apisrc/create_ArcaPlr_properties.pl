#!/usr/bin/perl

#ARCA Options TOP Feed (BBO)

print <<EOF;
#ifndef _EFH_ARCA_PLR_TOP__PROPS_H
#define _EFH_ARCA_PLR_TOP__PROPS_H
EOF

$exch_name = "ARCA_PLR";
$groups = 15;
$auth = "AOGTSXDP01";

$retrans_tcp = "162.69.107.250:35068";

$mc_base        = "224.0.96.";
$mc_lsb_base    = 48;
$mc_port_base        = 41051;

$retrans_base        = "224.0.96.";
$retrans_lsb_base    = 64;
$retrans_port_base   = 42051;

$refresh_base        = "224.0.96.";
$refresh_lsb_base    = 80;
$refresh_port_base   = 43051;

$ch_id_base = 51;

print "EkaProp efhArcaPlrInitCtxEntries_A[] = {\n";
for ($i=0; $i<$groups - 1;$i++) {
    $mc_lsb = $mc_lsb_base + $i;
    $mc_port = $mc_port_base + $i;
    $ch_id = $ch_id_base + $i;
    $retrans_lsb = $retrans_lsb_base + $i;
    $refresh_lsb = $refresh_lsb_base + $i;
    $refresh_port = $refresh_port_base + $i;
    $retrans_port = $retrans_port_base + $i;

print <<EOI;    
    {"efh.ARCA_PLR.group.$i.products","vanilla_book"},
    {"efh.ARCA_PLR.group.$i.mcast.addr",      "$mc_base$mc_lsb:$mc_port"},
    {"efh.ARCA_PLR.group.$i.refresh.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.refresh.udpAddr", "$refresh_base$refresh_lsb:$refresh_port"},
    {"efh.ARCA_PLR.group.$i.retrans.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.retrans.udpAddr", "$retrans_base$retrans_lsb:$retrans_port"},
    {"efh.ARCA_PLR.group.$i.ChannelId","$ch_id"},
    {"efh.ARCA_PLR.group.$i.SourceId","ARGTSXD01"},

EOI
}

$i = $groups;

print <<EOT;
// trades
    {"efh.ARCA_PLR.group.$i.products","vanilla_trades"},
    {"efh.ARCA_PLR.group.$i.mcast.addr",      "224.0.96.96:4401"},
    {"efh.ARCA_PLR.group.$i.refresh.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.refresh.udpAddr", "224.0.96.98:4601"},
    {"efh.ARCA_PLR.group.$i.retrans.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.retrans.udpAddr", "224.0.96.97:4501"},
    {"efh.ARCA_PLR.group.$i.ChannelId","1"},
    {"efh.ARCA_PLR.group.$i.SourceId","ARGTSXD01"},

};
EOT
#------------------------------------------------
print "EkaProp efhArcaPlrInitCtxEntries_B[] = {\n";
for ($i=0; $i<$groups - 1;$i++) {
    $mc_lsb = $mc_lsb_base + $i;
    $mc_port = $mc_port_base + $i;
    $ch_id = $ch_id_base + $i;
    $retrans_lsb = $retrans_lsb_base + $i;
    $refresh_lsb = $refresh_lsb_base + $i;
    $refresh_port = $refresh_port_base + $i;
    $retrans_port = $retrans_port_base + $i;

print <<EOI;    
    {"efh.ARCA_PLR.group.$i.products","vanilla_book"},
    {"efh.ARCA_PLR.group.$i.mcast.addr",      "$mc_base$mc_lsb:$mc_port"},
    {"efh.ARCA_PLR.group.$i.refresh.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.refresh.udpAddr", "$refresh_base$refresh_lsb:$refresh_port"},
    {"efh.ARCA_PLR.group.$i.retrans.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.retrans.udpAddr", "$retrans_base$retrans_lsb:$retrans_port"},
    {"efh.ARCA_PLR.group.$i.ChannelId","$ch_id"},
    {"efh.ARCA_PLR.group.$i.SourceId","ARGTSXD01"},

EOI
}

$i = $groups;

print <<EOT;
// trades
    {"efh.ARCA_PLR.group.$i.products","vanilla_trades"},
    {"efh.ARCA_PLR.group.$i.mcast.addr",      "224.0.96.96:4401"},
    {"efh.ARCA_PLR.group.$i.refresh.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.refresh.udpAddr", "224.0.96.98:4601"},
    {"efh.ARCA_PLR.group.$i.retrans.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.retrans.udpAddr", "224.0.96.97:4501"},
    {"efh.ARCA_PLR.group.$i.ChannelId","1"},
    {"efh.ARCA_PLR.group.$i.SourceId","ARGTSXD01"},

};
EOT

##################################################

    print "const EkaGroup arcaPlrGroups[] = {\n";
for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print <<DUMMY_AMEX;
// DUMMY
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
const EkaGroup amexPlrGroups[] = {
	{EkaSource::kAMEX_PLR, (EkaLSI)0},
};
DUMMY_AMEX


print "#endif\n";

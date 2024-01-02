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
    {"efh.ARCA_PLR.group.$i.SourceId","$auth"},

EOI
}

$i = $groups - 1;

print <<EOT;
// trades
    {"efh.ARCA_PLR.group.$i.products","vanilla_trades"},
    {"efh.ARCA_PLR.group.$i.mcast.addr",      "224.0.96.96:44001"},
    {"efh.ARCA_PLR.group.$i.refresh.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.refresh.udpAddr", "224.0.96.98:46001"},
    {"efh.ARCA_PLR.group.$i.retrans.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.retrans.udpAddr", "224.0.96.97:45001"},
    {"efh.ARCA_PLR.group.$i.ChannelId","1"},
    {"efh.ARCA_PLR.group.$i.SourceId","$auth"},

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
    {"efh.ARCA_PLR.group.$i.SourceId","$auth"},

EOI
}

$i = $groups-1;

print <<EOT;
// trades
    {"efh.ARCA_PLR.group.$i.products","vanilla_trades"},
    {"efh.ARCA_PLR.group.$i.mcast.addr",      "224.0.96.96:44001"},
    {"efh.ARCA_PLR.group.$i.refresh.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.refresh.udpAddr", "224.0.96.98:46001"},
    {"efh.ARCA_PLR.group.$i.retrans.tcpAddr", "$retrans_tcp"},
    {"efh.ARCA_PLR.group.$i.retrans.udpAddr", "224.0.96.97:45001"},
    {"efh.ARCA_PLR.group.$i.ChannelId","1"},
    {"efh.ARCA_PLR.group.$i.SourceId","$auth"},

};
EOT

##################################################

    print "const EkaGroup arcaPlrGroups[] = {\n";
for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

#Firm GTS Securities LLC
#SenderCompId AXGTSXDP01
#Session_Type Market Data
#User_Type xdp_rcf
#MIC AMXO
#Port 35218
#Primary_IP 162.69.127.250
#Secondary_IP 162.69.123.250
#DR_Primary_IP 162.68.127.250
#DR_Secondary_IP 162.68.123.250

$exch = "AMEX_PLR";
$groups = 15;
$auth = "AXGTSXDP01";

$retrans_tcp = "162.69.127.250:35218";
$attr = "vanilla_book";

print "EkaProp efhAmexPlrInitCtxEntries_A[] = {\n";
for ($i=0; $i<14;$i++) {
    $gr=$i;
    print "\t\{\"efh.$exch.group.$gr.products\",\"$attr\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.mcast.addr\"   ,\"224.0.196.",48 + $i,":", 41051+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.retrans.udpAddr\",\"224.0.196.",64 + $i,":", 42051+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.refresh.udpAddr\",\"224.0.196.",80 + $i,":", 43051+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.refresh.tcpAddr\",\"$retrans_tcp\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.retrans.tcpAddr\",\"$retrans_tcp\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.ChannelId\",\"",51+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.SourceId\",\"$auth\"\},\n";

    print "\n";
}

$attr = "vanilla_trades";

for ($i=0; $i<14;$i++) {
    $gr= 14 + $i;
    print "\t\{\"efh.$exch.group.$gr.products\",\"$attr\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.mcast.addr\"   ,\"224.0.197.",48 + $i,":", 44001+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.retrans.udpAddr\",\"224.0.197.",64 + $i,":", 45001+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.refresh.udpAddr\",\"224.0.197.",80 + $i,":", 46001+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.refresh.tcpAddr\",\"$retrans_tcp\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.retrans.tcpAddr\",\"$retrans_tcp\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.ChannelId\",\"",51+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.SourceId\",\"$auth\"\},\n";

    print "\n";
}
print "};\n\n";

##################################################

print "EkaProp efhAmexPlrInitCtxEntries_B[] = {\n";
for ($i=0; $i<14;$i++) {
    $gr=$i;
    print "\t\{\"efh.$exch.group.$gr.products\",\"$attr\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.mcast.addr\"   ,\"224.0.196.",48 + $i,":", 41051+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.retrans.udpAddr\",\"224.0.196.",64 + $i,":", 42051+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.refresh.udpAddr\",\"224.0.196.",80 + $i,":", 43051+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.refresh.tcpAddr\",\"$retrans_tcp\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.retrans.tcpAddr\",\"$retrans_tcp\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.ChannelId\",\"",51+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.SourceId\",\"$auth\"\},\n";

    print "\n";
}

$attr = "vanilla_trades";

for ($i=0; $i<14;$i++) {
    $gr= 14 + $i;
    print "\t\{\"efh.$exch.group.$gr.products\",\"$attr\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.mcast.addr\"   ,\"224.0.197.",48 + $i,":", 44001+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.retrans.udpAddr\",\"224.0.197.",64 + $i,":", 45001+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.refresh.udpAddr\",\"224.0.197.",80 + $i,":", 46001+$i,"\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.refresh.tcpAddr\",\"$retrans_tcp\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.retrans.tcpAddr\",\"$retrans_tcp\"\},\n";

    print "\t\{\"efh.$exch.group.$gr.ChannelId\",\"",51+$i,"\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.SourceId\",\"$auth\"\},\n";

    print "\n";
}
print "};\n\n";

##################################################

    print "const EkaGroup amexPlrGroups[] = {\n";
for ($i = 0; $i < 28; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";




print "#endif\n";

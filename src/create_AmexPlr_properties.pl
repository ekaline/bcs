#!/usr/bin/perl

#AMEX Options TOP Feed (BBO)

print <<EOF;
#ifndef _EFH_AMEX_PLR_TOP__PROPS_H
#define _EFH_AMEX_PLR_TOP__PROPS_H
EOF


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
$auth = "AOGTSXDP01";

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


#####################################################


print "#endif\n";

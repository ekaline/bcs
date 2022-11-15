#!/usr/bin/perl

# MRX2TOP_DPTH

print <<EOF;
#ifndef _EFH_MRX2TOP_DPTH__PROPS_H
#define _EFH_MRX2TOP_DPTH__PROPS_H

EOF

$groups = $#ARGV == 0 ? $ARGV[0] : 8;

$exch_name = "MRX2_TOP";


$mc_port        = 18000;
$mc_base        = "233.104.73.";
$mc_lsb_base     = 0;

$mold_ip        = "206.200.182.";
$mold_lsb_base  = 64;
$mold_port      = 18100;

$glimpse_ip_base = "206.200.182.";
$glimpse_ip_lsb  = 72;
$glimpse_port   = 18300;

$username = "MGGT04";
$passwd   = "UWYYFCYI";

$sqf_mc_port = 18008;
$sqf_mc_ip_base = "233.104.73.";
$sqf_mc_ip_lsb  = 8;

$sqf_mold_port    = 18108;
$sqf_mold_ip_base = "206.200.182.";
$sqf_mold_ip_lsb  = 88;
$sqf_mold_ip_incr = 3;

$sqf_soupbin_port    = 18108;
$sqf_soupbin_ip_base = "206.200.182.";
$sqf_soupbin_ip_lsb  = 89;
$sqf_soupbin_ip_incr = 3;


#New York Metro Area - "A" Feed
#  RP: 207.251.255.168
#    Broadcast - Channel 1 (A-D)	233.104.73.8	18008	206.200.182.0/26
#    Broadcast - Channel 2 (E-J)	233.104.73.9	18009	206.200.182.0/26
#    Broadcast - Channel 3 (K-R)	233.104.73.10	18010	206.200.182.0/26
#    Broadcast - Channel 4 (S-Z)	233.104.73.11	18011	206.200.182.0/26
#
#    Rerequest UDP - Channel 1 (A-D)	 	18108	206.200.182.88
#    Rerequest UDP - Channel 2 (E-J)	 	18109	206.200.182.91
#    Rerequest UDP - Channel 3 (K-R)	 	18110	206.200.182.94
#    Rerequest UDP - Channel 4 (S-Z)	 	18111	206.200.182.97
#
#    Rerequest TCP - Channel 1 (A-D)	 	18108	206.200.182.89
#    Rerequest TCP - Channel 2 (E-J)	 	18109	206.200.182.92
#    Rerequest TCP - Channel 3 (K-R)	 	18110	206.200.182.95
#    Rerequest TCP - Channel 4 (S-Z)	 	18111	206.200.182.98
    
print "EkaProp efhMrx2TopInitCtxEntries_A[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    $mc_lsb        = $mc_lsb_base       + $i;
    $mold_lsb  = $mold_lsb_base + $i * 2;

    print "\t\{\"efh.$exch_name.group.$i.products\",\"vanilla_book\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$mold_ip$mold_lsb:$mold_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"${glimpse_ip_base}${glimpse_ip_lsb}:$glimpse_port\"\}, \t// TCP GLIMPSE\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port   ++;
    $mold_port ++;
    $glimpse_port ++;
    $glimpse_ip_lsb ++;
    
}

for ($i=$groups; $i<2 * $groups;$i++) {    
    $mc_lsb        = $mc_lsb_base       + $i;
    $mold_lsb  = $mold_lsb_base + $i * 2;

    print "\t\{\"efh.$exch_name.group.$i.products\",\"vanilla_auction\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$sqf_mc_ip_base$sqf_mc_ip_lsb:$sqf_mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$sqf_mold_ip_base$sqf_mold_ip_lsb:$sqf_mold_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$sqf_soupbin_ip_base$sqf_soupbin_ip_lsb:$sqf_soupbin_port\"\}, \t// TCP GLIMPSE\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $sqf_mc_port   ++;
    $sqf_mold_port ++;
    $sqf_soupbin_port ++;
    $sqf_mc_ip_lsb ++;
    $sqf_mold_ip_lsb += $sqf_mold_ip_incr;
    $sqf_soupbin_ip_lsb += $sqf_soupbin_ip_incr;
    
}

print "};\n\n";
#####################################################
$mc_port        = 18000;
$mc_base        = "233.104.56.";
$mc_lsb_base     = 0;

$mold_ip        = "206.200.182.";
$mold_lsb_base  = 192;
$mold_port      = 18100;

$glimpse_port   = 18300;
$glimpse_ip_base = "206.200.182.";
$glimpse_ip_lsb  = 200;

$username = "MGGT04";
$passwd   = "UWYYFCYI";

print "EkaProp efhMrx2TopInitCtxEntries_B[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    $mc_lsb        = $mc_lsb_base       + $i;
    $mold_lsb  = $mold_lsb_base + $i * 2;

    print "\t\{\"efh.$exch_name.group.$i.products\",\"vanilla_book\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$mold_ip$mold_lsb:$mold_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"${glimpse_ip_base}${glimpse_ip_lsb}:$glimpse_port\"\}, \t// TCP GLIMPSE\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port       ++;
    $mold_port ++;
    $glimpse_port ++;
    $glimpse_ip_lsb ++;
}
print "};\n\n";
#####################################################


print "const EkaGroup mrx2TopGroups[] = {\n";
for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print "#endif\n";

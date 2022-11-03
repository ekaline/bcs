#!/usr/bin/perl

# MRX2TOP_DPTH

print <<EOF;
#ifndef _EFH_MRX2TOP_DPTH__PROPS_H
#define _EFH_MRX2TOP_DPTH__PROPS_H

EOF

$groups = $#ARGV == 0 ? $ARGV[0] : 4;

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

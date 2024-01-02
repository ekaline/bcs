#!/usr/bin/perl

# MIAX_TOM

print <<EOF;
#ifndef _EFH_MIAX_PROPS_H
#define _EFH_MIAX_PROPS_H

EOF
    
print "EkaProp efhMiaxInitCtxEntries_A[] = {\n";



$port = 51001;
$rt_mc_base  = "224.0.105.";
$rt_mc_lsb = 1;
$sesm_base = "199.168.152.";
$sesm_lsb = 161;

$sesm_port = 51101;

$exch_name = "MIAX_TOM";

for ($i=0; $i<24;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$sesm_lsb++;
    }

    $username = $i+1 . "tr1";
    $passwd = $i+1 . "tr1";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\}, \t\t\t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n\n";

    $rt_mc_lsb++;
    $port++;
    $sesm_port++;
}
print "};\n\n";
#####################################################

print "EkaProp efhMiaxInitCtxEntries_B[] = {\n";

$port = 53001;
$rt_mc_base  = "233.105.122.";
$rt_mc_lsb = 1;
$sesm_base = "199.168.153.";
$sesm_lsb = 161;

$sesm_port = 53101;

for ($i=0; $i<24;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$sesm_lsb++;
    }

    $username = $i+1 . "tr1";
    $passwd = $i+1 . "tr1";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\}, \t\t\t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n\n";

    $rt_mc_lsb++;
    $port++;
    $sesm_port++;
}
print "};\n\n";
#####################################################

print "const EkaGroup miaxGroups[] = {\n";

for ($i = 0; $i < 24; $i ++) {
    print "\t{EkaSource::kMIAX_TOM, (EkaLSI)$i},\n";

}
print "};\n\n";

#####################################################

$exch_name = "PEARL_TOM";

print "EkaProp efhPearlInitCtxEntries_A[] = {\n";


$port = 57001;
$rt_mc_base  = "224.0.141.";
$rt_mc_lsb = 1;
$sesm_base = "199.168.156.";
$sesm_lsb = 164;

$sesm_port = 57101;

for ($i=0; $i<12;$i++) {
    if ($i > 0 && $i % 3 == 0) {
	$sesm_lsb++;
    }

    $username = $i+1 . "pr1";
    $passwd = $i+1 . "pr1";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\}, \t\t\t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n\n";

    $rt_mc_lsb++;
    $port++;
    $sesm_port++;
}
print "};\n\n";

#####################################################

print "EkaProp efhPearlInitCtxEntries_B[] = {\n";


$port = 58001;
$rt_mc_base  = "233.87.168.";
$rt_mc_lsb = 1;
$sesm_base = "199.168.156.";
$sesm_lsb = 164;

$sesm_port = 58101;

for ($i=0; $i<12;$i++) {
    if ($i > 0 && $i % 3 == 0) {
	$sesm_lsb++;
    }

    $username = $i+1 . "pr1";
    $passwd = $i+1 . "pr1";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\}, \t\t\t// SESM \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$sesm_base$sesm_lsb:$sesm_port\"\}, \t// SESM \n\n";

    $rt_mc_lsb++;
    $port++;
    $sesm_port++;
}
print "};\n\n";

#####################################################

print "const EkaGroup pearlGroups[] = {\n";

for ($i = 0; $i < 12; $i ++) {
    print "\t{EkaSource::kPEARL_TOM, (EkaLSI)$i},\n";

}
print "};\n\n";

print "#endif\n";

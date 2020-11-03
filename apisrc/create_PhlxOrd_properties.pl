#!/usr/bin/perl

# PHLX_ORD

print <<EOF;
#ifndef _EFH_PHLX_ORD__PROPS_H
#define _EFH_PHLX_ORD__PROPS_H

EOF

$groups = $#ARGV == 0 ? $ARGV[0] : 4;

$exch_name = "PHLX_ORD";


$mc_port = 19101;
$mc_base = "233.47.179.";
$mc_lsb_base  = 140;

$snapshot_ip   = "159.79.3.";
$snapshot_lsb_base  = 138;
$snapshot_port = 19201;

$recovery_ip   = "159.79.3.";
$recovery_lsb_base  = 139;
$recovery_port = 19201;

$username = "XXXX";
$passwd   = "YYYY";

print "EkaProp efhPhlxOrdInitCtxEntries_A[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    if ($i == 3) {
	$mc_lsb      = $mc_lsb_base;
	$snapshot_lsb = $snapshot_lsb_base + 8;
	$recovery_lsb = $recovery_lsb_base + 8;
    } else {
	$mc_lsb        = $mc_lsb_base       + $i + 1;
	$snapshot_lsb  = $snapshot_lsb_base + 2 * $i;
	$recovery_lsb  = $recovery_lsb_base + 2 * $i;
    }

    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$snapshot_ip$snapshot_lsb:$snapshot_port\"\}, \t// TCP SOUPBIN\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip$recovery_lsb:$recovery_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port       ++;
    $recovery_port ++;
    $snapshot_port ++;
}
print "};\n\n";
#####################################################
$mc_port = 19101;
$mc_base = "233.47.179.";
$mc_lsb_base  = 156;

$snapshot_ip   = "159.79.3.";
$snapshot_lsb_base  = 202;
$snapshot_port = 19201;

$recovery_ip   = "159.79.3.";
$recovery_lsb_base  = 203;
$recovery_port = 19201;

$username = "XXXX";
$passwd   = "YYYY";

print "EkaProp efhPhlxOrdInitCtxEntries_B[] = {\n";
for ($i=0; $i<$groups;$i++) {    
    if ($i == 3) {
	$mc_lsb      = $mc_lsb_base;
	$snapshot_lsb = $snapshot_lsb_base + 8;
	$recovery_lsb = $recovery_lsb_base + 8;
    } else {
	$mc_lsb        = $mc_lsb_base       + $i + 1;
	$snapshot_lsb  = $snapshot_lsb_base + 2 * $i;
	$recovery_lsb  = $recovery_lsb_base + 2 * $i;
    }

    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\"   ,\"$mc_base$mc_lsb:$mc_port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$snapshot_ip$snapshot_lsb:$snapshot_port\"\}, \t// TCP SOUPBIN\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$recovery_ip$recovery_lsb:$recovery_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$username:$passwd\"\},\n";
    print "\n";

    $mc_port       ++;
    $recovery_port ++;
    $snapshot_port ++;
}
print "};\n\n";
#####################################################


print "const EkaGroup phlxOrdGroups[] = {\n";
for ($i = 0; $i < $groups; $i ++) {
    print "\t{EkaSource::k$exch_name, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print "#endif\n";

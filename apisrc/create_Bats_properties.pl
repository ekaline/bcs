#!/usr/bin/perl

print <<EOF;
#ifndef _EFH_BATS_PROPS_H
#define _EFH_BATS_PROPS_H

EOF

# C1_A

$port = 30301;
$unit = 1;
$rt_mc_base  = "224.0.74.";
$rt_mc_lsb = 46;
$grp_mc_base = "224.0.74.";
$grp_mc_lsb = 55;

$auth = "BCAP:sb1bcap";
$spin_ip_base = "170.137.114.";
$spin_port = 18901;

$sessionSubID = 57;

$exch_name = "C1_PITCH";

print "EkaProp efhBatsC1InitCtxEntries_A[] = {\n";

for ($i=0; $i<35;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$rt_mc_lsb++;
	$grp_mc_lsb++;
    }
    $spin_ip_lsb = 
	$i < 12 ? 100 : 
	$i < 24 ? 101 :
	102;

#    print "$unit: RT MC: $rt_mc_base$rt_mc_lsb:$port, GRP MC: $grp_mc_base$grp_mc_lsb:$port, spin: $spin_ip_base${spin_ip_lsb}:$spin_port\n";

    print "\t\{\"efh.$exch_name.group.$i.unit\",\"$unit\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$spin_ip_base$spin_ip_lsb:$spin_port\"\}, \t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$auth\"\}, \t\t// SPIN \n";
    printf ("\t\{\"efh.%s.group.%d.snapshot.sessionSubID\",\"%04d\"\}, \t\t// SPIN \n",$exch_name,$i,$sessionSubID);
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n\n";

    $port++;
    $spin_port++;
    $unit++;
    $sessionSubID += 2;
}

print "};\n\n";

#####################################################
# C1_B

print "EkaProp efhBatsC1InitCtxEntries_B[] = {\n";

$port = 30301;
$unit = 1;
$rt_mc_base  = "233.182.199.";
$rt_mc_lsb = 174;
$grp_mc_base = "233.182.199.";
$grp_mc_lsb = 183;

$spin_ip_base = "170.137.114.";
$spin_port = 18901;

$sessionSubID = 57;

$exch_name = "C1_PITCH";

for ($i=0; $i<35;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$rt_mc_lsb++;
	$grp_mc_lsb++;
    }
    $spin_ip_lsb = 
	$i < 12 ? 100 : 
	$i < 24 ? 101 :
	102;

#    print "$unit: RT MC: $rt_mc_base$rt_mc_lsb:$port, GRP MC: $grp_mc_base$grp_mc_lsb:$port, spin: $spin_ip_base${spin_ip_lsb}:$spin_port\n";

    print "\t\{\"efh.$exch_name.group.$i.unit\",\"$unit\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$spin_ip_base$spin_ip_lsb:$spin_port\"\}, \t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$auth\"\}, \t\t// SPIN \n";
    printf ("\t\{\"efh.%s.group.%d.snapshot.sessionSubID\",\"%04d\"\}, \t\t// SPIN \n",$exch_name,$i,$sessionSubID);
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n\n";

    $port++;
    $spin_port++;
    $unit++;
    $sessionSubID += 2;
}
print "};\n\n";

#####################################################
# C1_C

print "EkaProp efhBatsC1InitCtxEntries_C[] = {\n";

$port = 30301;
$unit = 1;
$rt_mc_base  = "224.0.74.";
$rt_mc_lsb = 0;
$grp_mc_base = "224.0.74.";
$grp_mc_lsb = 37;

$spin_ip_base = "174.137.114.";
$spin_port = 18901;

$sessionSubID = 57;

$exch_name = "C1_PITCH";

for ($i=0; $i<35;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$grp_mc_lsb++;
    }
    $spin_ip_lsb = 
	$i < 12 ? 100 : 
	$i < 24 ? 101 :
	102;

#    print "$unit: RT MC: $rt_mc_base$rt_mc_lsb:$port, GRP MC: $grp_mc_base$grp_mc_lsb:$port, spin: $spin_ip_base${spin_ip_lsb}:$spin_port\n";

    print "\t\{\"efh.$exch_name.group.$i.unit\",\"$unit\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$spin_ip_base$spin_ip_lsb:$spin_port\"\}, \t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$auth\"\}, \t\t// SPIN \n";
    printf ("\t\{\"efh.%s.group.%d.snapshot.sessionSubID\",\"%04d\"\}, \t\t// SPIN \n",$exch_name,$i,$sessionSubID);
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n\n";

    $rt_mc_lsb++;
    $port++;
    $spin_port++;
    $unit++;
    $sessionSubID += 2;
}

print "};\n\n";

#####################################################
# C1_D
print "EkaProp efhBatsC1InitCtxEntries_D[] = {\n";

$port = 30301;
$unit = 1;
$rt_mc_base  = "233.182.199.";
$rt_mc_lsb = 128;
$grp_mc_base = "233.182.199.";
$grp_mc_lsb = 165;

$spin_ip_base = "174.137.114.";
$spin_port = 18901;

$sessionSubID = 57;

$exch_name = "C1_PITCH";

for ($i=0; $i<35;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$grp_mc_lsb++;
    }
    $spin_ip_lsb = 
	$i < 12 ? 100 : 
	$i < 24 ? 101 :
	102;

#    print "$unit: RT MC: $rt_mc_base$rt_mc_lsb:$port, GRP MC: $grp_mc_base$grp_mc_lsb:$port, spin: $spin_ip_base${spin_ip_lsb}:$spin_port\n";

    print "\t\{\"efh.$exch_name.group.$i.unit\",\"$unit\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$spin_ip_base$spin_ip_lsb:$spin_port\"\}, \t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$auth\"\}, \t\t// SPIN \n";
    printf ("\t\{\"efh.%s.group.%d.snapshot.sessionSubID\",\"%04d\"\}, \t\t// SPIN \n",$exch_name,$i,$sessionSubID);
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n\n";

    $rt_mc_lsb++;
    $port++;
    $spin_port++;
    $unit++;
    $sessionSubID += 2;
}
print "};\n\n";
#####################################################
print "const EkaGroup batsC1Groups[] = {\n";
for ($i=0; $i<35;$i++) {
    print "    {EkaSource::kC1_PITCH, (EkaLSI)$i},\n";

}
print "};\n\n";

print "#endif\n";

exit;

#####################################################
# C2

$port = 30201;
$unit = 1;
$rt_mc_base  = "233.130.124.";
$rt_mc_lsb = 208;
$grp_mc_base = "233.130.124.";
$grp_mc_lsb = 224;

$spin_ip_base = "174.136.168.";
$spin_port = 15903;

$sessionSubID = 119;

$exch_name = "BATS_C2";

for ($i=0; $i<35;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$rt_mc_lsb++;
	$grp_mc_lsb++;
    }

    $spin_ip_lsb = $i < 16 ? 132 : 134;

#    print "$unit: RT MC: $rt_mc_base$rt_mc_lsb:$port, GRP MC: $grp_mc_base$grp_mc_lsb:$port, spin: $spin_ip_base${spin_ip_lsb}:$spin_port\n";

    print "\t\{\"efh.$exch_name.group.$i.unit\",\"$unit\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$spin_ip_base$spin_ip_lsb:$spin_port\"\}, \t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"$auth\"\}, \t\t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.sessionSubID\",\"0$sessionSubID\"\}, \t\t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n\n";

    $port++;
    $spin_port++;
    $unit++;
    $sessionSubID += 2;
}
print "#" x 40,"\n";

#####################################################

# BZX

$port = 30101;
$unit = 1;
$rt_mc_base  = "224.0.131.";
$rt_mc_lsb = 32;
$grp_mc_base = "224.0.131.";
$grp_mc_lsb = 48;

$spin_ip_base = "174.136.163.";
$spin_port = 15935;

$sessionSubID = 223;

$username  = "BCAP";
$passwd = "so4bcap2";

$exch_name = "BZX_PITCH";

for ($i=0; $i<32;$i++) {
    if ($i > 0 && $i % 4 == 0) {
	$rt_mc_lsb++;
	$grp_mc_lsb++;
    }
    $spin_ip_lsb = $i < 16 ? 149 : 150;
#    print "$unit: RT MC: $rt_mc_base$rt_mc_lsb:$port, GRP MC: $grp_mc_base$grp_mc_lsb:$port, spin: $spin_ip_base${spin_ip_lsb}:$spin_port\n";

    print "\t\{\"efh.$exch_name.group.$i.unit\",\"$unit\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc_base$rt_mc_lsb:$port\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"$spin_ip_base$spin_ip_lsb:$spin_port\"\}, \t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"${username}:${passwd}\"\}, \t\t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.sessionSubID\",\"0$sessionSubID\"\}, \t\t// SPIN \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t\t// GRP \n\n";

    $port++;
    $spin_port++;
    $unit++;
    $sessionSubID += 2;
}





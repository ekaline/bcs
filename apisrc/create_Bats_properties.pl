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

$auth = "GTSS:sb2gtss";
$spin_ip_base = "170.137.114.";
$spin_port = 18901;

$sessionSubID = 432;

$exch_name = "C1_PITCH";

$grpIp   = "170.137.114.102";
$grpPort = 17006;
$grpUser = "GTSS";
$grpPswd = "eb3gtss";
$grpSessionSubId = "0587";


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
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP MC\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAddr\",\"$grpIp:$grpPort\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAuth\",\"$grpUser:$grpPswd\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpSessionSubID\",\"$grpSessionSubId\"\}, \t// GRP TCP\n";
    print "\n";

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

$sessionSubID = 432;

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
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAddr\",\"$grpIp:$grpPort\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAuth\",\"$grpUser:$grpPswd\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpSessionSubID\",\"$grpSessionSubId\"\}, \t// GRP TCP\n";
    print "\n";

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

$spin_ip_base = "170.137.114.";
$spin_port = 18901;

$sessionSubID = 432;

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
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAddr\",\"$grpIp:$grpPort\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAuth\",\"$grpUser:$grpPswd\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpSessionSubID\",\"$grpSessionSubId\"\}, \t// GRP TCP\n";
    print "\n";

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

$sessionSubID = 432;

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
    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc_base$grp_mc_lsb:$port\"\}, \t// GRP \n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAddr\",\"$grpIp:$grpPort\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAuth\",\"$grpUser:$grpPswd\"\}, \t// GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpSessionSubID\",\"$grpSessionSubId\"\}, \t// GRP TCP\n";
    print "\n";

    $rt_mc_lsb++;
    $port++;
    $spin_port++;
    $unit++;
    $sessionSubID += 2;
}
print "};\n\n";



#####################################################

# CAC C1 Complex feed
# C1_D
print "EkaProp efhBatsC1InitCtxEntries_CAC[] = {\n";

$port = 30351;
$unit = 1;

$spin_port = 18701;

$sessionSubID = 502;

$spinUsername  = "GTSS";
$spinPasswd = "cb2gtss";

$exch_name = "C1_PITCH";

$grpIp   = "170.137.114.108";
$grpPort = 17045;

$grpUser = "GTSS";
$grpPswd = "cgb1gtss";

$grpSessionSubId = "0879";

for ($i=0; $i<35;$i++) {
    if ($i < 17) {
	$rt_mc  = "224.0.74.80";
	$grp_mc = "224.0.74.82";
    } else {
	$rt_mc  = "224.0.74.81";
	$grp_mc = "224.0.74.83";
    }
    
    if ($i < 13) {
	$spin_ip = "170.137.114.106";
    } elsif ($i < 25) {
	$spin_ip = "170.137.114.107";
    } else {
	$spin_ip = "170.137.114.108";
    }

    print "\t\{\"efh.$exch_name.group.$i.unit\",\"$unit\"\},\n";
    print "\t\{\"efh.$exch_name.group.$i.mcast.addr\",\"$rt_mc:$port\"\},\n";
    
    print "\t\{\"efh.$exch_name.group.$i.snapshot.addr\",\"${spin_ip}:${spin_port}\"\}, \t// Complex SPIN\n";
    print "\t\{\"efh.$exch_name.group.$i.snapshot.auth\",\"${spinUsername}:${spinPasswd}\"\}, \t\t// Complex SPIN \n";
#    print "\t\{\"efh.$exch_name.group.$i.snapshot.sessionSubID\",\"$sessionSubID\"\}, \t\t// Complex SPIN \n";
    printf ("\t\{\"efh.%s.group.%d.snapshot.sessionSubID\",\"%04d\"\}, \t\t// Complex SPIN \n",$exch_name,$i,$sessionSubID);

    print "\t\{\"efh.$exch_name.group.$i.recovery.addr\",\"$grp_mc:$port\"\}, \t\t// Complex GRP UDP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAddr\",\"$grpIp:$grpPort\"\}, \t// Complex GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpAuth\",\"$grpUser:$grpPswd\"\}, \t// Complex GRP TCP\n";
#    print "\t\{\"efh.$exch_name.group.$i.recovery.grpSessionSubID\",\"$grpSessionSubId\"\}, \t// Complex GRP TCP\n";
    print "\t\{\"efh.$exch_name.group.$i.recovery.grpSessionSubID\",\"$grpSessionSubId\"\}, \t// Complex GRP TCP\n";

    print "\n";

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

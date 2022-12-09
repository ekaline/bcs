#!/usr/bin/perl

# MRX2TOP_DPTH




print <<EOF;
#ifndef _EFH_MRX2TOP_DPTH__PROPS_H
#define _EFH_MRX2TOP_DPTH__PROPS_H

EOF

$groupsPerFeed = 4;
$totalFeeds = 3;

$exch = "MRX2_TOP";

$username = "MGGT04";
$passwd   = "UWYYFCYI";


print "EkaProp efhMrx2TopInitCtxEntries_A[] = {\n";
#Nasdaq MRX Top of Market Feed 2.01
#UDP/IP Protocol Standard: MoldUDP64
#Symbology Compliant: Yes
#Initial Release Date: November 7, 2022
#Site	Channel/Purpose	Group	Port	Source IP Address
#New York Metro Area - "A" Feed
#RP: 207.251.255.168
#       Broadcast - Channel 1 (A-D)	233.104.73.0	18000	206.200.182.0/26
# 	Broadcast - Channel 2 (E-J)	233.104.73.1	18001	206.200.182.0/26
# 	Broadcast - Channel 3 (K-R)	233.104.73.2	18002	206.200.182.0/26
# 	Broadcast - Channel 4 (S-Z)	233.104.73.3	18003	206.200.182.0/26

# 	Rerequest UDP - Channel 1 (A-D)	 	18100	206.200.182.64
# 	Rerequest UDP - Channel 2 (E-J)	 	18101	206.200.182.66
# 	Rerequest UDP - Channel 3 (K-R)	 	18102	206.200.182.68
# 	Rerequest UDP - Channel 4 (S-Z)	 	18103	206.200.182.70

# 	Rerequest TCP - Channel 1 (A-D)	 	18100	206.200.182.65
# 	Rerequest TCP - Channel 2 (E-J)	 	18101	206.200.182.67
# 	Rerequest TCP - Channel 3 (K-R)	 	18102	206.200.182.69
#       Rerequest TCP - Channel 4 (S-Z)	 	18103	206.200.182.71

  
$feed = 0;
print "// Top Feed used for Quotes and Trade Statuses only\n";

$mc_port        = 18000;
$mc_base        = "233.104.73.";
$mc_lsb         = 0;

$mold_ip        = "206.200.182.";
$mold_lsb       = 64;
$mold_port      = 18100;
$mold_incr      = 2;

# Glimpse Credentials:
#    MGGT04/UWYYFCYI : 206.200.182.72:18300
#    MGGT04/UWYYFCYI : 206.200.182.73:18301
#    MGGT04/UWYYFCYI : 206.200.182.74:18302
#    MGGT04/UWYYFCYI : 206.200.182.75:18303
$glimpse_base = "206.200.182.";
$glimpse_lsb  = 72;
$glimpse_port = 18300;

for ($i = 0 ; $i < $groupsPerFeed ;$i ++) {    
    printFeedProps ($exch,
		    $i + $feed * $groupsPerFeed,
		    "vanilla_book",       # attr
		    "$mc_base$mc_lsb",
		    $mc_port,
		    "$mold_ip$mold_lsb",  # recovery addr
		    $mold_port,           # recovery port,
		    "$glimpse_base$glimpse_lsb", # snapshot_addr,
		    $glimpse_port,               # snapshot_port,
		    $username,
		    $passwd
	);

    $mc_lsb    ++;
    $mc_port   ++;

    $mold_lsb += $mold_incr;
    $mold_port ++;
    
    $glimpse_port ++;
    $glimpse_lsb ++;
    
}

#Nasdaq MRX Order Feed 2.01
#UDP/IP Protocol Standard: MoldUDP64
#Symbology Compliant: Yes
#Initial Release Date: November 7, 2022
#    Site	Channel/Purpose	Group	Port	Source IP Address
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

$feed ++;
print "// Order Feed used for SQFs only\n";

$mc_port = 18008;
$mc_base = "233.104.73.";
$mc_lsb  = 8;

$mold_port    = 18108;
$mold_base = "206.200.182.";
$mold_lsb  = 88;
$mold_incr = 3;

#$soupbin_port    = 18108;
#$soupbin_base = "206.200.182.";
#$soupbin_lsb  = 89;
#$soupbin_incr = 3;

$soupbin_base = "206.200.182.";
$soupbin_lsb  = 72;
$soupbin_port = 18300;
$soupbin_incr = 1;

for ($i = 0 ; $i < $groupsPerFeed ;$i ++) {    
    printFeedProps ($exch,
		    $i + $feed * $groupsPerFeed,
		    "vanilla_auction",    # attr
		    "$mc_base$mc_lsb",
		    $mc_port,
		    "$mold_ip$mold_lsb",  # recovery addr
		    $mold_port,           # recovery port,
		    "$soupbin_base$soupbin_lsb", # snapshot_addr,
		    $soupbin_port,               # snapshot_port,
		    $username,
		    $passwd
	);

    $mc_lsb    ++;
    $mc_port   ++;

    $mold_lsb += $mold_incr;
    $mold_port ++;
    
    $soupbin_lsb += $soupbin_incr;
    $soupbin_port ++;   
}

#Nasdaq MRX Trade Feed 2.01
#UDP/IP Protocol Standard: MoldUDP64
#Symbology Compliant: Yes
#Initial Release Date: November 7, 2022
#Site	Channel/Purpose	Group	Port	Source IP Address
#New York Metro Area - "A" Feed
#RP: 207.251.255.168
#       Broadcast - Channel 1 (A-D)	233.104.73.12	18012	206.200.182.0/26
#	Broadcast - Channel 2 (E-J)	233.104.73.13	18013	206.200.182.0/26
# 	Broadcast - Channel 3 (K-R)	233.104.73.14	18014	206.200.182.0/26
# 	Broadcast - Channel 4 (S-Z)	233.104.73.15	18015	206.200.182.0/26
#
# 	Rerequest UDP - Channel 1 (A-D)	 	18112	206.200.182.100
#	Rerequest UDP - Channel 2 (E-J)	 	18113	206.200.182.103
# 	Rerequest UDP - Channel 3 (K-R)	 	18114	206.200.182.105
# 	Rerequest UDP - Channel 4 (S-Z)	 	18115	206.200.182.107
#
# 	Rerequest TCP - Channel 1 (A-D)	 	18112	206.200.182.101
# 	Rerequest TCP - Channel 2 (E-J)	 	18113	206.200.182.104
#	Rerequest TCP - Channel 3 (K-R)	 	18114	206.200.182.106
#       Rerequest TCP - Channel 4 (S-Z)	 	18115	206.200.182.108

$feed ++;
print "// Trade Feed used for Trades only\n";

$mc_port = 18012;
$mc_base = "233.104.73.";
$mc_lsb  = 12;

$mold_port    = 18112;
$mold_base = "206.200.182.";
$mold_lsb  = 100;

#$soupbin_port    = 18112;
#$soupbin_base = "206.200.182.";
#$soupbin_lsb  = 101;

$soupbin_base = "206.200.182.";
$soupbin_lsb  = 72;
$soupbin_port = 18300;
$soupbin_incr = 1;


for ($i = 0 ; $i < $groupsPerFeed ;$i ++) {    
    printFeedProps ($exch,
		    $i + $feed * $groupsPerFeed,
		    "vanilla_trades",    # attr
		    "$mc_base$mc_lsb",
		    $mc_port,
		    "$mold_ip$mold_lsb",  # recovery addr
		    $mold_port,           # recovery port,
		    "$soupbin_base$soupbin_lsb", # snapshot_addr,
		    $soupbin_port,               # snapshot_port,
		    $username,
		    $passwd
	);
    $mold_incr    = $i == 0 ? 3 : 2;
    $soupbin_incr = $i == 0 ? 3 : 2;
    
    $mc_lsb    ++;
    $mc_port   ++;

    $mold_lsb += $mold_incr;
    $mold_port ++;
    
    $soupbin_incr = 1;
    $soupbin_lsb += $soupbin_incr;
    $soupbin_port ++;  
    
}    
print "};\n\n";
#####################################################
$mc_port        = 18000;
$mc_base        = "233.104.56.";
$mc_lsb_base     = 0;

$mold_ip        = "206.200.182.";
$mold_lsb_base  = 192;
$mold_port      = 18100;

$soupbin_port   = 18300;
$soupbin_ip_base = "206.200.182.";
$soupbin_ip_lsb  = 200;

$username = "MGGT04";
$passwd   = "UWYYFCYI";

print "EkaProp efhMrx2TopInitCtxEntries_B[] = {\n";
#Nasdaq MRX Top of Market Feed 2.01
#UDP/IP Protocol Standard: MoldUDP64
#Symbology Compliant: Yes
#Initial Release Date: November 7, 2022
#Site	Channel/Purpose	Group	Port	Source IP Address

#New York Metro Area - "B" Feed
#RP: 207.251.255.184
#       Broadcast - Channel 1 (A-D)	233.127.56.0	18000	206.200.182.128/26
# 	Broadcast - Channel 2 (E-J)	233.127.56.1	18001	206.200.182.128/26
# 	Broadcast - Channel 3 (K-R)	233.127.56.2	18002	206.200.182.128/26
# 	Broadcast - Channel 4 (S-Z)	233.127.56.3	18003	206.200.182.128/26
#
# 	Rerequest UDP - Channel 1 (A-D)	 	18100	206.200.182.192
# 	Rerequest UDP - Channel 2 (E-J)	 	18101	206.200.182.194
# 	Rerequest UDP - Channel 3 (K-R)	 	18102	206.200.182.196
#	Rerequest UDP - Channel 4 (S-Z)	 	18103	206.200.182.198
#
# 	Rerequest TCP - Channel 1 (A-D)	 	18100	206.200.182.193
# 	Rerequest TCP - Channel 2 (E-J)	 	18101	206.200.182.195
# 	Rerequest TCP - Channel 3 (K-R)	 	18102	206.200.182.197
#       Rerequest TCP - Channel 4 (S-Z)	 	18103	206.200.182.199
    

  
$feed = 0;
print "// Top Feed used for Quotes and Trade Statuses only\n";

$mc_port        = 18000;
$mc_base        = "233.127.56.";
$mc_lsb         = 0;

$mold_ip        = "206.200.182.";
$mold_lsb       = 192;
$mold_port      = 18100;
$mold_incr      = 2;

# Glimpse Credentials: A Feed!!!!
#    MGGT04/UWYYFCYI : 206.200.182.72:18300
#    MGGT04/UWYYFCYI : 206.200.182.73:18301
#    MGGT04/UWYYFCYI : 206.200.182.74:18302
#    MGGT04/UWYYFCYI : 206.200.182.75:18303
$glimpse_base = "206.200.182.";
$glimpse_lsb  = 72;
$glimpse_port = 18300;

for ($i = 0 ; $i < $groupsPerFeed ;$i ++) {    
    printFeedProps ($exch,
		    $i + $feed * $groupsPerFeed,
		    "vanilla_book",       # attr
		    "$mc_base$mc_lsb",
		    $mc_port,
		    "$mold_ip$mold_lsb",  # recovery addr
		    $mold_port,           # recovery port,
		    "$glimpse_base$glimpse_lsb", # snapshot_addr,
		    $glimpse_port,               # snapshot_port,
		    $username,
		    $passwd
	);

    $mc_lsb    ++;
    $mc_port   ++;

    $mold_lsb += $mold_incr;
    $mold_port ++;
    
    $glimpse_port ++;
    $glimpse_lsb ++;
    
}

#Nasdaq MRX Order Feed 2.01
#UDP/IP Protocol Standard: MoldUDP64
#Symbology Compliant: Yes
#Initial Release Date: November 7, 2022
#    Site	Channel/Purpose	Group	Port	Source IP Address
#New York Metro Area - "B" Feed
#RP: 207.251.255.184
#       Broadcast - Channel 1 (A-D)	233.127.56.8	18008	206.200.182.128/26
# 	Broadcast - Channel 2 (E-J)	233.127.56.9	18009	206.200.182.128/26
# 	Broadcast - Channel 3 (K-R)	233.127.56.10	18010	206.200.182.128/26
# 	Broadcast - Channel 4 (S-Z)	233.127.56.11	18011	206.200.182.128/26
#
# 	Rerequest UDP - Channel 1 (A-D)	 	18108	206.200.182.216
# 	Rerequest UDP - Channel 2 (E-J)	 	18109	206.200.182.219
#	Rerequest UDP - Channel 3 (K-R)	 	18110	206.200.182.222
# 	Rerequest UDP - Channel 4 (S-Z)	 	18111	206.200.182.225
#
# 	Rerequest TCP - Channel 1 (A-D)	 	18108	206.200.182.217
# 	Rerequest TCP - Channel 2 (E-J)	 	18109	206.200.182.220
# 	Rerequest TCP - Channel 3 (K-R)	 	18110	206.200.182.223
#	Rerequest TCP - Channel 4 (S-Z)	 	18111	206.200.182.226

$feed ++;
print "// Order Feed used for SQFs only\n";

$mc_port = 18008;
$mc_base = "233.127.56.";
$mc_lsb  = 8;

$mold_port    = 18108;
$mold_base = "206.200.182.";
$mold_lsb  = 216;
$mold_incr = 3;

$soupbin_port    = 18108;
$soupbin_base = "206.200.182.";
$soupbin_lsb  = 217;
$soupbin_incr = 3;

for ($i = 0 ; $i < $groupsPerFeed ;$i ++) {    
    printFeedProps ($exch,
		    $i + $feed * $groupsPerFeed,
		    "vanilla_auction",    # attr
		    "$mc_base$mc_lsb",
		    $mc_port,
		    "$mold_ip$mold_lsb",  # recovery addr
		    $mold_port,           # recovery port,
		    "$soupbin_base$soupbin_lsb", # snapshot_addr,
		    $soupbin_port,               # snapshot_port,
		    $username,
		    $passwd
	);

    $mc_lsb    ++;
    $mc_port   ++;

    $mold_lsb += $mold_incr;
    $mold_port ++;
    
    $soupbin_lsb += $soupbin_incr;
    $soupbin_port ++;   
}

#Nasdaq MRX Trade Feed 2.01
#UDP/IP Protocol Standard: MoldUDP64
#Symbology Compliant: Yes
#Initial Release Date: November 7, 2022
#Site	Channel/Purpose	Group	Port	Source IP Address
#New York Metro Area - "B" Feed
#RP: 207.251.255.184
#       Broadcast - Channel 1 (A-D)	233.127.56.12	18012	206.200.182.128/26
# 	Broadcast - Channel 2 (E-J)	233.127.56.13	18013	206.200.182.128/26
# 	Broadcast - Channel 3 (K-R)	233.127.56.14	18014	206.200.182.128/26
# 	Broadcast - Channel 4 (S-Z)	233.127.56.15	18015	206.200.182.128/26
#
# 	Rerequest UDP - Channel 1 (A-D)	 	18112	206.200.182.228
# 	Rerequest UDP - Channel 2 (E-J)	 	18113	206.200.182.230
# 	Rerequest UDP - Channel 3 (K-R)	 	18114	206.200.182.232
# 	Rerequest UDP - Channel 4 (S-Z)	 	18115	206.200.182.234
#
# 	Rerequest TCP - Channel 1 (A-D)	 	18112	206.200.182.229
# 	Rerequest TCP - Channel 2 (E-J)	 	18113	206.200.182.231
# 	Rerequest TCP - Channel 3 (K-R)	 	18114	206.200.182.233
# 	Rerequest TCP - Channel 4 (S-Z)	 	18115	206.200.182.235

$feed ++;
print "// Trade Feed used for Trades only\n";

$mc_port = 18012;
$mc_base = "233.127.56.";
$mc_lsb  = 12;

$mold_port    = 18112;
$mold_base = "206.200.182.";
$mold_lsb  = 228;

$soupbin_port    = 18112;
$soupbin_base = "206.200.182.";
$soupbin_lsb  = 229;

for ($i = 0 ; $i < $groupsPerFeed ;$i ++) {    
    printFeedProps ($exch,
		    $i + $feed * $groupsPerFeed,
		    "vanilla_trades",    # attr
		    "$mc_base$mc_lsb",
		    $mc_port,
		    "$mold_ip$mold_lsb",  # recovery addr
		    $mold_port,           # recovery port,
		    "$soupbin_base$soupbin_lsb", # snapshot_addr,
		    $soupbin_port,               # snapshot_port,
		    $username,
		    $passwd
	);
    $mold_incr    = 2;
    $soupbin_incr = 2;
    
    $mc_lsb    ++;
    $mc_port   ++;

    $mold_lsb += $mold_incr;
    $mold_port ++;
    
    $soupbin_lsb += $soupbin_incr;
    $soupbin_port ++;  
    
} 

print "};\n\n";
#####################################################


print "const EkaGroup mrx2TopGroups[] = {\n";
for ($i = 0; $i < $totalFeeds * $groupsPerFeed; $i ++) {
    print "\t{EkaSource::k$exch, (EkaLSI)$i},\n";

}
print "};\n\n";


#####################################################

print "#endif\n";

sub printFeedProps {
    my ($exch,
	$gr,
	$attr,
	$mc_addr,
	$mc_port,
	$recovery_addr,
	$recovery_port,
	$snapshot_addr,
	$snapshot_port,
	$user,
	$passwd
	) = @_;

    print "\t\{\"efh.$exch.group.$gr.products\",\"$attr\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.mcast.addr\"   ,\"$mc_addr:$mc_port\"\},\n";
    print "\t\{\"efh.$exch.group.$gr.recovery.addr\",\"$recovery_addr:$recovery_port\"\}, \t// MOLD RECOVERY\n";
    print "\t\{\"efh.$exch.group.$gr.snapshot.addr\",\"$snapshot_addr:$snapshot_port\"\}, \t// TCP GLIMPSE/SOUPBIN\n";
    print "\t\{\"efh.$exch.group.$gr.snapshot.auth\",\"$user:$passwd\"\},\n";
    print "\n";
}


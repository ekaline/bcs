#

REL_DIR=$HOME/BCS_RELEASES
BUILD_DIR=$HOME/bcs/build
REL_NAME=REL_$1
DST_DIR=${REL_DIR}/${REL_NAME}


echo DST_DIR=$DST_DIR
rm -rf $DST_DIR
mkdir -p ${DST_DIR}

mkdir -p ${DST_DIR}/lib
mkdir -p ${DST_DIR}/utils
mkdir -p ${DST_DIR}/SmartNIC_SW/driver
mkdir -p ${DST_DIR}/SmartNIC_SW/bin
mkdir -p ${DST_DIR}/SmartNIC_SW/tools

cp $BUILD_DIR/lib/EkaBcs/libEkaBcs.* $DST_DIR/lib

cp $BUILD_DIR/utils/EkaBcs/eka_version $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/eka_tcpdump $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/eka_udp_mc_server $DST_DIR/utils

cp $BUILD_DIR/../SmartNIC_SW/driver/*.sh ${DST_DIR}/SmartNIC_SW/driver
cp $BUILD_DIR/../SmartNIC_SW/driver/*.ko ${DST_DIR}/SmartNIC_SW/driver

cp $BUILD_DIR/../SmartNIC_SW/bin/* ${DST_DIR}/SmartNIC_SW/bin
cp $BUILD_DIR/../SmartNIC_SW/tools/fbconfig ${DST_DIR}/SmartNIC_SW/tools


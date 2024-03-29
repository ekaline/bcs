#

REL_DIR=$HOME/BCS_RELEASES
BUILD_DIR=$HOME/bcs/build
REL_NAME=$1

VERSIONDATE=$(date "+%y%m%d")
VERSIONTIME=$(date "+%H")$(date "+%M")

CURR_DIR=`pwd`
cd $BUILD_DIR
REL_GIT_COMMIT_ID=$(git rev-parse --short HEAD)
GIT_IS_DIRTY="c"
if git describe --dirty --always | grep -q "dirty"; then
    GIT_IS_DIRTY="d"
fi
cd $CURR_DIR

REL_NAME=${VERSIONDATE}_${VERSIONTIME}_${REL_GIT_COMMIT_ID}_${GIT_IS_DIRTY}_$1

DST_DIR=$HOME/BCS_RELEASES/$REL_NAME

echo DST_DIR=$DST_DIR
rm -rf $DST_DIR
mkdir -p ${DST_DIR}

mkdir -p ${DST_DIR}/api
mkdir -p ${DST_DIR}/lib
mkdir -p ${DST_DIR}/utils
mkdir -p ${DST_DIR}/tests
mkdir -p ${DST_DIR}/SmartNIC_SW/driver
mkdir -p ${DST_DIR}/SmartNIC_SW/bin
mkdir -p ${DST_DIR}/SmartNIC_SW/tools

cp $BUILD_DIR/../README.TXT $DST_DIR/

cp $BUILD_DIR/../api/EkaBcs.h $DST_DIR/api
cp $BUILD_DIR/lib/EkaBcs/libEkaBcs.so $DST_DIR/lib

cp $BUILD_DIR/../tests/Makefile ${DST_DIR}/tests
cp $BUILD_DIR/../tests/*.h ${DST_DIR}/tests
cp $BUILD_DIR/../tests/bcsMdTest.cpp ${DST_DIR}/tests
cp $BUILD_DIR/../tests/bcsTcpConnect.cpp ${DST_DIR}/tests
cp $BUILD_DIR/../tests/fireReportOnly.cpp ${DST_DIR}/tests

cp $BUILD_DIR/utils/EkaBcs/eka_version $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/eka_tcpdump $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/eka_udp_mc_server $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/sn_read $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/sn_write $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/efc_state $DST_DIR/utils
cp $BUILD_DIR/utils/EkaBcs/mc_receive $DST_DIR/utils

cp $BUILD_DIR/pcapParsers/EkaBcs/bcsPcapParse $DST_DIR/utils

cp $BUILD_DIR/../SmartNIC_SW/driver/*.sh ${DST_DIR}/SmartNIC_SW/driver
cp $BUILD_DIR/../SmartNIC_SW/driver/*.ko ${DST_DIR}/SmartNIC_SW/driver

cp $BUILD_DIR/../SmartNIC_SW/tools/fbconfig ${DST_DIR}/SmartNIC_SW/tools
cp $BUILD_DIR/../SmartNIC_SW/tools/fbupdate/fbupdate ${DST_DIR}/SmartNIC_SW/tools
cp $BUILD_DIR/../SmartNIC_SW/tools/fbconfig ${DST_DIR}/SmartNIC_SW/bin
cp $BUILD_DIR/../SmartNIC_SW/tools/fbupdate/fbupdate ${DST_DIR}/SmartNIC_SW/bin

cd $HOME/BCS_RELEASES

tar cvzf ${REL_NAME}.tar.gz $REL_NAME

scp ${REL_NAME}.tar.gz gw01:~

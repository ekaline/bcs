#!/bin/bash

if [ $USER = "vitaly" ]; then
    U=${HOME}/ekatools/smartnic
else
    U=${HOME}
fi

LIBEKA=${U}/libeka
SN=${U}/SmartNIC_SW

VER=$1

export EKA_RPM_NAME=ekaline2

export EKA_LIB_RPM_NAME=${EKA_RPM_NAME}-$VER
export EKA_BUILD_DATE=$(date "+%y%m%d")_$(date "+%H")$(date "+%M")
export EKA_RPM_VER=${VER}

cd ${LIBEKA}/apisrc

make clean && make -j 64 GCC_EXTERNAL_DEFINES="-DEKA_BUILD_DATE=$EKA_BUILD_DATE -DEKA_RPM_VER=${EKA_RPM_VER}"

if [ ! -f lib${EKA_RPM_NAME}.so ]; then
    exit -1
fi

\rm -rf /tmp/${EKA_LIB_RPM_NAME}
mkdir /tmp/${EKA_LIB_RPM_NAME}

export EkaSnDir=/apps/SmartNIC_SW/driver
export EkaInclDir=/usr/share/eka_include
#export EkaLibDir=/usr/lib64
export EkaLibDir=/usr/share/${EKA_RPM_NAME}_lib
export EkaUtilsDir=/usr/share/eka_utils

cd /tmp/${EKA_LIB_RPM_NAME}
mkdir -p ./${EkaSnDir}
mkdir -p ./${EkaInclDir}
mkdir -p ./${EkaLibDir}
mkdir -p ./${EkaUtilsDir}

cp ${LIBEKA}/apisrc/lib${EKA_RPM_NAME}.so ./${EkaLibDir}
cp ${LIBEKA}/apisrc/compat/*.h ${LIBEKA}/apisrc/compat/*.inl ./${EkaInclDir}
cp ${SN}/driver/smartnic.ko ./${EkaSnDir}

for f in `find ${LIBEKA}/eka_utils -executable -type f`; do
    cp $f ./${EkaUtilsDir}
done

cd /tmp

\rm ${HOME}/rpmbuild/SOURCES/${EKA_LIB_RPM_NAME}.tar.gz

tar -cvzf ${HOME}/rpmbuild/SOURCES/${EKA_LIB_RPM_NAME}.tar.gz ${EKA_LIB_RPM_NAME}

\rm -rf /tmp/${EKA_LIB_RPM_NAME}

rpmbuild --define "version $VER" -bb ${HOME}/rpmbuild/SPECS/${EKA_RPM_NAME}.spec --clean


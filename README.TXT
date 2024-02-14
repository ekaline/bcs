#

#How to compile the tests:

export EKA_SW_HOME=/home/ekaline/EKA_BCS_SW

g++ $EKA_SW_HOME/tests/bcsMdTest.cpp \
-o bcsMdTest \
-I $EKA_SW_HOME/api \
-I $EKA_SW_HOME/tests \
-L $EKA_SW_HOME/lib \
-lEkaBcs \
-Wl,-rpath,$EKA_SW_HOME/lib


g++ $EKA_SW_HOME/tests/bcsTcpConnect.cpp \
-o bcsTcpConnect \
-I $EKA_SW_HOME/api \
-I $EKA_SW_HOME/tests \
-L $EKA_SW_HOME/lib \
-lEkaBcs \
-Wl,-rpath,$EKA_SW_HOME/lib

How to compile a test:
g++ tests/bcsMdTest.cpp -o bcsMdTest -I api -I tests -L lib -lEkaBcs
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib


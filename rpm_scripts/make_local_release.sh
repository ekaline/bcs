#!/bin/bash

\rm ~/ekaline2_lib_last_release/*

make clean && make -j 64 && cp ./compat/*.h ./compat/*.inl ./*.so ~/ekaline2_lib_last_release


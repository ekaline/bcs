#!/bin/bash

cd $HOME/git/root_libeka/libeka/apisrc
make clean
cd $HOME/git/root_libeka
tar -cvzf $HOME/git/root_libeka/tars2send/libeka.$1.tar.gz libeka


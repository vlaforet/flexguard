#! /bin/bash

cd ext/jemalloc
./autogen.sh --disable-initial-exec-tls
./configure --disable-initial-exec-tls
make
sudo make install

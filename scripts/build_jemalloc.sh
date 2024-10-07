#! /bin/bash

cd ext/jemalloc
./autogen.sh
./configure --disable-initial-exec-tls
make
sudo make install

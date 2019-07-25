#!/bin/sh
set -e

cpputest_dir=$(pwd)/cpputest
mkdir -p $cpputest_dir
cd $cpputest_dir

# download sources
if [ ! -e "cpputest-3.7dev.tar.gz" ]; then
    wget https://github.com/cpputest/cpputest.github.io/raw/master/releases/cpputest-3.7dev.tar.gz
fi

# build + install
mkdir -p $cpputest_dir/src
tar xf "cpputest-3.7dev.tar.gz" -C $cpputest_dir/src --strip-components=1
cd $cpputest_dir/src
./configure --prefix=$cpputest_dir --disable-memory-leak-detection
make && make install

# cleanup
#cd $cpputest_dir && rm -rf $cpputest_dir/src

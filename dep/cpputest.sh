#!/bin/bash
set -e

cpputest_dir=$(pwd)/cpputest
mkdir -p $cpputest_dir
cd $cpputest_dir

# download sources
if [ ! -e "cpputest-4.0.tar.gz" ]; then
    wget https://github.com/cpputest/cpputest/releases/download/v4.0/cpputest-4.0.tar.gz
fi

# build + install
mkdir -p $cpputest_dir/src
tar xf "cpputest-4.0.tar.gz" -C $cpputest_dir/src --strip-components=1
cd $cpputest_dir/src
./configure --prefix=$cpputest_dir --disable-memory-leak-detection
make && make install

# cleanup
rm -rf $cpputest_dir/src

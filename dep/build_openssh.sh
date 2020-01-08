#!/bin/bash
set -e
set -x

unset CFLAGS CPPFLAGS LDFLAGS

root_dir=$(pwd)/build_openssh
deps_dir=$root_dir/deps
install_dir=$root_dir/install
prefix_dir_in_container=/opt/sarus/openssh

mkdir -p $deps_dir/src

# openssl
cd $deps_dir/src
wget https://github.com/openssl/openssl/archive/OpenSSL_1_0_2o.tar.gz
tar xf OpenSSL_1_0_2o.tar.gz
cd openssl-OpenSSL_1_0_2o
./config --prefix=$deps_dir -static -no-shared
make && make install

# zlib
cd $deps_dir/src
wget https://zlib.net/zlib-1.2.11.tar.gz
tar xf zlib-1.2.11.tar.gz
cd zlib-1.2.11
./configure --prefix=$deps_dir --static
make -j && make install

# openssh
cd $root_dir
wget https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-7.7p1.tar.gz
tar xf openssh-7.7p1.tar.gz
cd openssh-7.7p1
./configure --without-pam --with-ssl-dir=$deps_dir --with-zlib=$deps_dir --prefix=$prefix_dir_in_container --with-privsep-path=/tmp/var/empty --with-ldflags=-static
make -j && make install DESTDIR=$install_dir

# sshd_config
cat <<EOF > $install_dir/$prefix_dir_in_container/etc/sshd_config
Port 15263
StrictModes yes
PasswordAuthentication no
ChallengeResponseAuthentication no
AuthorizedKeysFile $prefix_dir_in_container/etc/userkeys/authorized_keys
X11Forwarding yes
Subsystem sftp $prefix_dir_in_container/libexec/sftp-server
PermitUserEnvironment no
AcceptEnv *
EOF

# ssh_config
cat <<EOF > $install_dir/$prefix_dir_in_container/etc/ssh_config
Host *
  StrictHostKeyChecking no
  UserKnownHostsFile /dev/null
  Port 15263
  IdentityFile $prefix_dir_in_container/etc/userkeys/id_rsa
EOF

cd $install_dir/$prefix_dir_in_container/..
tar cf $root_dir/../openssh.tar openssh

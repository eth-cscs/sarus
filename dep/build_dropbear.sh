#!/bin/bash
set -e
set -x

mkdir -p dropbear-build

cat <<EOF >dropbear-build/localoptions.h
#ifndef DROPBEAR_LOCAL_OPTIONS_H_
#define DROPBEAR_LOCAL_OPTIONS_H_

/* Do not print message of the day */
#define DO_MOTD 0

/* Enable X11 Forwarding - server only */
#define DROPBEAR_X11FWD 1

/* Disable password authentication on server */
#define DROPBEAR_SVR_PASSWORD_AUTH 0

/* Disable password authentication from client */
#define DROPBEAR_CLI_PASSWORD_AUTH 0

/* Disable possibility of running an SFTP server
 * (not included with Dropbear) */
#define DROPBEAR_SFTPSERVER 0

/* Default PATH when logging into remote hosts */
#define DEFAULT_PATH "/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin"

/* Include verbose debug output, enabled with -v at runtime. */
#define DEBUG_TRACE 1

#endif /* DROPBEAR_LOCAL_OPTIONS_H_ */
EOF

[ -e dropbear ] || git clone https://github.com/mkj/dropbear.git
cd dropbear
git checkout DROPBEAR_2020.81
autoconf
autoheader

cd ../dropbear-build
../dropbear/configure --enable-static
make PROGRAMS="dropbear dbclient dropbearkey" MULTI=1 -j # build single executable a la busybox
cp dropbearmulti ..

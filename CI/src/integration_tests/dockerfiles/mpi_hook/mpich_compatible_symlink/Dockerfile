FROM debian:jessie

RUN mkdir /usr/lib/subdir
COPY libdummy.so /usr/lib/subdir/libmpich.so.12.5.5
RUN ln -s /usr/lib/subdir/libmpich.so.12.5.5 /usr/lib/libmpich.so.12.5.5

RUN ldconfig
# docker build -f Dockerfile -t \
#   ethcscs/mpich:ub1804_cuda92_mpi314_gpudirect-all_gather .
FROM ethcscs/mpich:ub1804_cuda92_mpi314

COPY all_gather.cpp /opt/mpi_gpudirect/all_gather.cpp
WORKDIR /opt/mpi_gpudirect
RUN mpicxx -g all_gather.cpp -o all_gather -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart

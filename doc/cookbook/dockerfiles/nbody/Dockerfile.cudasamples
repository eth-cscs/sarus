FROM nvidia/cuda:9.2-devel

RUN apt-get update && apt-get install -y --no-install-recommends \
        cuda-samples-$CUDA_PKG_VERSION && \
    rm -rf /var/lib/apt/lists/*

RUN (cd /usr/local/cuda/samples/1_Utilities/bandwidthTest && make)
RUN (cd /usr/local/cuda/samples/1_Utilities/deviceQuery && make)
RUN (cd /usr/local/cuda/samples/1_Utilities/deviceQueryDrv && make)
RUN (cd /usr/local/cuda/samples/1_Utilities/p2pBandwidthLatencyTest && make)
RUN (cd /usr/local/cuda/samples/1_Utilities/topologyQuery && make)
RUN (cd /usr/local/cuda/samples/5_Simulations/nbody && make)

CMD ["/usr/local/cuda/samples/1_Utilities/deviceQuery/deviceQuery"]

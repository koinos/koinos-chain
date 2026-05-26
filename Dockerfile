FROM alpine:3.18 AS builder

RUN apk update && \
    apk add  \
        gcc \
        g++ \
        ccache \
        musl-dev \
        linux-headers \
        libgmpxx \
        cmake \
        make \
        git

WORKDIR /workspace

ENV CC=gcc
ENV CXX=g++
ENV CMAKE_C_COMPILER_LAUNCHER=ccache
ENV CMAKE_CXX_COMPILER_LAUNCHER=ccache
ENV CCACHE_DIR=/workspace/.ccache

# NOTE:
# This Dockerfile expects build context at the mono-repo root (koinos-code),
# e.g.:
# docker build -f koinos-chain/Dockerfile -t koinos-chain-local-compile .
ADD koinos-proto-cpp /workspace/koinos-proto-cpp
ADD koinos-state-db-cpp /workspace/koinos-state-db-cpp
ADD koinos-chain /workspace/koinos-chain

RUN git -C /workspace/koinos-proto-cpp submodule update --init --recursive && \
    rm -rf /workspace/koinos-proto-cpp/build-docker && \
    cmake -S /workspace/koinos-proto-cpp -B /workspace/koinos-proto-cpp/build-docker -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build /workspace/koinos-proto-cpp/build-docker --config Release --parallel 3 && \
    cmake --install /workspace/koinos-proto-cpp/build-docker

# koinos-state-db-cpp resolves koinos_proto through Hunter. Mirror the locally built
# proto artifacts into Hunter install roots so state_db compiles with previous_value.
RUN for install_dir in /root/.hunter/_Base/*/*/*/Install; do \
      if [ -d "${install_dir}" ]; then \
        mkdir -p "${install_dir}/include/koinos/protocol" && \
        cp -f /usr/local/include/koinos/protocol/protocol.pb.h "${install_dir}/include/koinos/protocol/protocol.pb.h" && \
        if [ -f /usr/local/include/koinos/protocol/protocol.grpc.pb.h ]; then cp -f /usr/local/include/koinos/protocol/protocol.grpc.pb.h "${install_dir}/include/koinos/protocol/protocol.grpc.pb.h"; fi && \
        mkdir -p "${install_dir}/lib" && \
        if [ -f /usr/local/lib/libkoinos_proto.a ]; then cp -f /usr/local/lib/libkoinos_proto.a "${install_dir}/lib/libkoinos_proto.a"; fi && \
        if [ -f /usr/local/lib/libkoinos_protod.a ]; then cp -f /usr/local/lib/libkoinos_protod.a "${install_dir}/lib/libkoinos_protod.a"; fi && \
        if [ -d /usr/local/lib/cmake/koinos_proto ]; then mkdir -p "${install_dir}/lib/cmake" && rm -rf "${install_dir}/lib/cmake/koinos_proto" && cp -a /usr/local/lib/cmake/koinos_proto "${install_dir}/lib/cmake/koinos_proto"; fi; \
      fi; \
    done

RUN git -C /workspace/koinos-state-db-cpp submodule update --init --recursive && \
    rm -rf /workspace/koinos-state-db-cpp/build-docker && \
    cmake -S /workspace/koinos-state-db-cpp -B /workspace/koinos-state-db-cpp/build-docker -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_PREFIX_PATH=/usr/local

# Re-apply local proto over Hunter installs created during state-db configure.
RUN for install_dir in /root/.hunter/_Base/*/*/*/Install; do \
      if [ -d "${install_dir}" ]; then \
        mkdir -p "${install_dir}/include/koinos/protocol" && \
        cp -f /usr/local/include/koinos/protocol/protocol.pb.h "${install_dir}/include/koinos/protocol/protocol.pb.h" && \
        if [ -f /usr/local/include/koinos/protocol/protocol.grpc.pb.h ]; then cp -f /usr/local/include/koinos/protocol/protocol.grpc.pb.h "${install_dir}/include/koinos/protocol/protocol.grpc.pb.h"; fi && \
        mkdir -p "${install_dir}/lib" && \
        if [ -f /usr/local/lib/libkoinos_proto.a ]; then cp -f /usr/local/lib/libkoinos_proto.a "${install_dir}/lib/libkoinos_proto.a"; fi && \
        if [ -f /usr/local/lib/libkoinos_protod.a ]; then cp -f /usr/local/lib/libkoinos_protod.a "${install_dir}/lib/libkoinos_protod.a"; fi && \
        if [ -d /usr/local/lib/cmake/koinos_proto ]; then mkdir -p "${install_dir}/lib/cmake" && rm -rf "${install_dir}/lib/cmake/koinos_proto" && cp -a /usr/local/lib/cmake/koinos_proto "${install_dir}/lib/cmake/koinos_proto"; fi; \
      fi; \
    done

RUN cmake --build /workspace/koinos-state-db-cpp/build-docker --config Release --parallel 3 && \
    cmake --install /workspace/koinos-state-db-cpp/build-docker

RUN git -C /workspace/koinos-chain submodule update --init --recursive && \
    rm -rf /workspace/koinos-chain/build-docker && \
    cmake -S /workspace/koinos-chain -B /workspace/koinos-chain/build-docker -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local

# Re-apply local proto over Hunter installs created during chain configure.
RUN for install_dir in /root/.hunter/_Base/*/*/*/Install; do \
      if [ -d "${install_dir}" ]; then \
        mkdir -p "${install_dir}/include/koinos/protocol" && \
        cp -f /usr/local/include/koinos/protocol/protocol.pb.h "${install_dir}/include/koinos/protocol/protocol.pb.h" && \
        if [ -f /usr/local/include/koinos/protocol/protocol.grpc.pb.h ]; then cp -f /usr/local/include/koinos/protocol/protocol.grpc.pb.h "${install_dir}/include/koinos/protocol/protocol.grpc.pb.h"; fi && \
        mkdir -p "${install_dir}/lib" && \
        if [ -f /usr/local/lib/libkoinos_proto.a ]; then cp -f /usr/local/lib/libkoinos_proto.a "${install_dir}/lib/libkoinos_proto.a"; fi && \
        if [ -f /usr/local/lib/libkoinos_protod.a ]; then cp -f /usr/local/lib/libkoinos_protod.a "${install_dir}/lib/libkoinos_protod.a"; fi && \
        if [ -d /usr/local/lib/cmake/koinos_proto ]; then mkdir -p "${install_dir}/lib/cmake" && rm -rf "${install_dir}/lib/cmake/koinos_proto" && cp -a /usr/local/lib/cmake/koinos_proto "${install_dir}/lib/cmake/koinos_proto"; fi; \
      fi; \
    done

RUN cmake --build /workspace/koinos-chain/build-docker --config Release --parallel 3

FROM alpine:latest
RUN apk update && \
    apk add \
        musl \
        libstdc++
COPY --from=builder /workspace/koinos-chain/build-docker/src/koinos_chain /usr/local/bin
ENTRYPOINT [ "/usr/local/bin/koinos_chain" ]
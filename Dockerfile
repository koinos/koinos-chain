FROM alpine:3.18 as builder

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

ADD . /build
WORKDIR /build

ENV CC=gcc
ENV CXX=g++
ENV CMAKE_C_COMPILER_LAUNCHER=ccache
ENV CMAKE_CXX_COMPILER_LAUNCHER=ccache
ENV CCACHE_DIR /build/.ccache

RUN git submodule update --init --recursive && \
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build . --config Release --parallel 3

FROM alpine:latest
RUN apk update && \
    apk add \
        musl \
        libstdc++
COPY --from=builder /build/src/koinos_chain /usr/local/bin
ENTRYPOINT [ "/usr/local/bin/koinos_chain" ]
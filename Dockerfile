FROM alpine:latest as builder

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
        git \
        perl \
        python3 \
        py3-pip \
        py3-setuptools && \
    pip3 install --user dataclasses_json Jinja2 importlib_resources pluginbase gitpython

ADD . /koinos-chain
WORKDIR /koinos-chain

ENV CC=/usr/lib/ccache/bin/gcc
ENV CXX=/usr/lib/ccache/bin/g++

RUN mkdir -p /koinos-chain/.ccache && \
    ln -s /koinos-chain/.ccache $HOME/.ccache && \
    git submodule update --init --recursive && \
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build . --config Release --parallel

FROM alpine:latest
RUN apk update && \
    apk add \
        musl \
        libstdc++
COPY --from=builder /koinos-chain/programs/koinos_chain/koinos_chain /usr/local/bin
COPY --from=builder /koinos-chain/programs/koinos_transaction_signer/koinos_transaction_signer /usr/local/bin
ENTRYPOINT [ "/usr/local/bin/koinos_chain" ]

FROM phusion/baseimage:focal-1.0.0alpha1-amd64 as builder

RUN apt-get update && \
    apt-get install -y \
        cmake \
        build-essential \
        git \
        libgmp-dev \
        python3 \
        python3-pip \
        python3-setuptools && \
    pip3 install --user dataclasses_json Jinja2 importlib_resources pluginbase gitpython

ADD . /koinos-chain
WORKDIR /koinos-chain

RUN git submodule update --init --recursive && \
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build . --config Release --parallel

FROM phusion/baseimage:focal-1.0.0alpha1-amd64
COPY --from=builder /koinos-chain/programs/koinos_chain/koinos_chain /usr/local/bin
COPY --from=builder /koinos-chain/programs/koinos_transaction_signer/koinos_transaction_signer /usr/local/bin
CMD  /usr/local/bin/koinos_chain

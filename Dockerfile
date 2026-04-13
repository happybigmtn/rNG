# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8

RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        build-essential \
        ca-certificates \
        cmake \
        libboost-dev \
        libevent-dev \
        libsqlite3-dev \
        pkgconf \
        python3 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B /build \
        -DBUILD_TESTS=OFF \
        -DBUILD_GUI=OFF \
        -DENABLE_IPC=OFF \
        -DWITH_ZMQ=OFF \
        -DENABLE_WALLET=ON \
        -DCMAKE_BUILD_TYPE=Release && \
    cmake --build /build -j"$(nproc)" --target rngd rng-cli

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV RNG_DATA_DIR=/home/rng/.rng

RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        ca-certificates \
        libevent-2.1-7t64 \
        libevent-extra-2.1-7t64 \
        libevent-pthreads-2.1-7t64 \
        libsqlite3-0 \
        libstdc++6 && \
    rm -rf /var/lib/apt/lists/* && \
    useradd --create-home --shell /usr/sbin/nologin rng

COPY --from=build /build/bin/rngd /usr/local/bin/rngd
COPY --from=build /build/bin/rng-cli /usr/local/bin/rng-cli
COPY contrib/docker-entrypoint.sh /usr/local/bin/rng-docker-entrypoint

RUN chmod 755 /usr/local/bin/rngd \
        /usr/local/bin/rng-cli \
        /usr/local/bin/rng-docker-entrypoint && \
    mkdir -p /home/rng/.rng && \
    chown -R rng:rng /home/rng

USER rng
WORKDIR /home/rng
VOLUME ["/home/rng/.rng"]
EXPOSE 8432 8433

ENTRYPOINT ["rng-docker-entrypoint"]
CMD ["rngd", "-printtoconsole"]

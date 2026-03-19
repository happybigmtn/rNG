# RNG - Bitcoin fork for AI agents
# Multi-stage build for minimal final image

# Stage 1: Build
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config \
    libboost-all-dev libssl-dev libevent-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source
COPY . .

# Get RandomX (pinned to v1.2.1)
RUN git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx

# Build
RUN cmake -B build \
    -DBUILD_TESTING=OFF \
    -DENABLE_IPC=OFF \
    -DWITH_ZMQ=OFF \
    -DENABLE_WALLET=ON \
    && cmake --build build -j$(nproc)

# Stage 2: Runtime
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libevent-2.1-7 \
    libevent-pthreads-2.1-7 \
    libsqlite3-0 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -m -s /bin/bash rng

# Copy binaries from builder
COPY --from=builder /build/build/bin/rngd /usr/local/bin/
COPY --from=builder /build/build/bin/rng-cli /usr/local/bin/

# Create data directory
RUN mkdir -p /home/rng/.rng && chown -R rng:rng /home/rng

USER rng
WORKDIR /home/rng

# Default config with the live public peers
RUN printf 'server=1\nrpcuser=agent\nrpcpassword=changeme\nrpcbind=127.0.0.1\nrpcallowip=127.0.0.1\nminerandomx=fast\naddnode=95.111.239.142:8433\naddnode=161.97.114.192:8433\naddnode=185.218.126.23:8433\naddnode=185.239.209.227:8433\n' > /home/rng/.rng/rng.conf

EXPOSE 8432 8433

ENTRYPOINT ["rngd"]
CMD ["-printtoconsole"]

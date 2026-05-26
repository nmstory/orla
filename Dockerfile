# Stage 1: Build Orla
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .
RUN if [ -d .git ]; then \
    --mount=type=ssh git submodule update --init --recursive || true; \
    else \
    echo "no .git in build context; assuming submodules are already present"; \
    fi

ARG ORLA_TSAN=OFF

# TODO: remove debugging tools in production image
RUN cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DORLA_TSAN=${ORLA_TSAN}
RUN cmake --build build -j$(nproc)

# Stage 2: Docker CLI binary
FROM docker:27-cli AS docker-cli

# Stage 3: Runtime Image
FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y \
    libpthread-stubs0-dev \
    libtsan2 \
    # TODO: remove debugging tools in production image
    gdb \
    strace \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/orla /usr/local/bin/orla
COPY --from=docker-cli /usr/local/bin/docker /usr/local/bin/docker

# Expose UDP ports for simulation traffic and TCP for Prometheus scraping
EXPOSE 4000-4100/udp
EXPOSE 9100-9102/tcp

ENV ROLE=edge \
    PORT=4001 \
    CONTROLLER_ADDR=controller \
    LOSS_RATE=0.0 \
    MAX_LATENCY_MS=0 \
    PROMETHEUS_PORT=9090

ENTRYPOINT ["/usr/local/bin/orla"]

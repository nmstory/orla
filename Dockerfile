# Stage 1: Build Orla
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .
RUN if [ -d .git ]; then \
    --mount=type=ssh git submodule update --init --recursive || true; \
    else \
    echo "no .git in build context; assuming submodules are already present"; \
    fi

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# Stage 2: Runtime Image
FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y \
    libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/orla /usr/local/bin/orla

# Expose UDP ports
EXPOSE 4000-4100/udp

# Environment variables for configuration
ENV ROLE=edge \
    PORT=4001 \
    CONTROLLER_ADDR=controller \
    LOSS_RATE=0.0 \
    MAX_LATENCY_MS=0 \
    PROMETHEUS_PORT=9090

# Entry point: launch the correct role
ENTRYPOINT ["/usr/local/bin/orla"]
CMD ["--role=${ROLE}", "--port=${PORT}", "--controller=${CONTROLLER_ADDR}"]

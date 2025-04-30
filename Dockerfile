# Build stage
FROM ubuntu:24.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libzmq5-dev \
    && rm -rf /var/lib/apt/lists/*

# turn the detached message off
RUN git config --global advice.detachedHead false

# Install Tracy
RUN git clone --depth 1 --branch v0.11.1 https://github.com/wolfpld/tracy.git /tmp/tracy \
    && cd /tmp/tracy \
    && mkdir build && cd build \
    && cmake .. && make -j$(nproc) && make install \
    && rm -rf /tmp/tracy

# Copy and build source code
WORKDIR /app

ADD src ./

RUN ls 

RUN g++ -std=c++23 -O3 -march=native -Wall -Wextra -I/usr/local/include -I.\
    -DENABLE_TRACY main.cpp zmq_server.cpp message_dispatcher.cpp jsonrpc_handler.cpp deps/simdjson-3.10.1/singleheader/simdjson.cpp\
    -o zmq_server -lzmq -lsimdjson -ltracy -pthread

# Runtime stage
FROM ubuntu:24.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libzmq5 \
    && rm -rf /var/lib/apt/lists/*

# Copy binary
COPY --from=builder /app/zmq_server /usr/local/bin/

# Expose PUB port
EXPOSE 5556

# Run the application
CMD ["zmq_server", "--benchmark"]
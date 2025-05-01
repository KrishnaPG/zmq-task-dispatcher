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

# Copy and build source code
WORKDIR /app

ADD src ./src
ADD CMakeLists.txt ./

# Create build directory
RUN mkdir build
# Change to the build directory
WORKDIR /app/build

# Generate makefiles using CMake
RUN cmake ..

# Compile the project using make
RUN make -j$(nproc)

# RUN g++ -std=c++23 -O3 -march=native -Wall -Wextra -I/usr/local/include -I. main.cpp shutdown.cpp -o zmq_task_dispatcher -lzmq

# Runtime stage
FROM ubuntu:24.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libzmq5 \
    && rm -rf /var/lib/apt/lists/*

# Copy binary
COPY --from=builder /app/zmq_task_dispatcher /usr/local/bin/

# Expose PUB port
EXPOSE 5556

# Run the application
CMD ["zmq_task_dispatcher"]
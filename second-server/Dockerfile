# Start from a base image
FROM ubuntu:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libc-dev \
    git \
    cmake \
    libjson-c-dev

# Set the working directory
WORKDIR /app

# Clone cJSON from GitHub
RUN git clone https://github.com/DaveGamble/cJSON.git \
    && cd cJSON \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make \
    && make install

# Copy the server code into the container
COPY . .

ENV LD_LIBRARY_PATH=/usr/local/lib

# Compile the server code
RUN gcc -o server main.c -lcjson -lm

# Set the command to run the server
CMD ["./server"]

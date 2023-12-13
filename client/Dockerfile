# Start from a base image
FROM ubuntu:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libc-dev \
    libncurses5-dev \
    libncursesw5-dev \
    locales \
    locales-all \
    git \
    cmake \
    libjson-c-dev

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

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

# Copy the client code into the container
COPY . .

ENV LD_LIBRARY_PATH=/usr/local/lib

# Compile the client code
RUN gcc -o client main.c -lncurses -lcjson -lm

# Set the command to run the client
CMD ["./client"]

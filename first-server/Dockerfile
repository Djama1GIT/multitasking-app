# Start from a base image
FROM ubuntu:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libc-dev \
    libjson-c-dev

# Set the working directory
WORKDIR /app

# Copy the server code into the container
COPY . .

# Compile the server code
RUN gcc -o server main.c -ljson-c

# Set the command to run the server
CMD ["./server"]

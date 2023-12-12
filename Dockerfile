# Start from a base image
FROM ubuntu:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libc-dev

# Set the working directory
WORKDIR /app

# Copy the client code into the container
COPY client .

# Compile the client code
RUN gcc -o client main.c

# Set the command to run the client
CMD ["./client"]

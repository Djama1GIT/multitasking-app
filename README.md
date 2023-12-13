# Multitasking App

This is a multitasking app that communicates with two servers to perform various tasks. The app uses sockets to connect to the servers, sends commands, and reads their responses. The app is written in C and uses Docker for creating isolated environments.

## Installation and Setup <sup><sub>(tested on Linux)</sub></sup>

1. Install Make, Docker and Docker Compose if they are not already installed on your system.

2. Clone the project repository:

```bash
git clone https://github.com/Djama1GIT/multitasking-app.git
cd multitasking-app
```
3. Start the project:

```bash
make build
```

```bash
make run-servers
```

```bash
make run-client
```

## Usage

The app communicates with two servers: first-server and second-server. The following commands can be entered:

    1 - Display the architecture of the processor (first-server)
    2 - Display the number of logical processors (first-server)
    3 - Display the number of processes in the system (second-server)
    4 - Display the number of modules of the server process (second-server)

## User Interface

Console :)

## Technologies Used

- C - The programming language used for the project.
- Websocket - WebSocket is a computer communications protocol, providing simultaneous two-way communication channels over a single Transmission Control Protocol connection.
- Docker - A platform used in the project for creating, deploying, and managing containers, allowing the application to run in an isolated environment.

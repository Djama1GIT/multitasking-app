#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cpuid.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>

void get_cpu_architecture(char *arch){
    unsigned int level = 0;
    unsigned int eax, ebx, ecx, edx;
    __get_cpuid(level, &eax, &ebx, &ecx, &edx);
    ((unsigned *)arch)[0] = ebx;
    ((unsigned *)arch)[1] = edx;
    ((unsigned *)arch)[2] = ecx;
}

int get_logical_processors_count(){
    return sysconf(_SC_NPROCESSORS_ONLN);
}

pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

// Global variable to hold client sockets
int* client_sockets = NULL;
int client_sockets_count = 0;
pthread_mutex_t client_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_client(void* arg) {
    pthread_mutex_lock(&counter_mutex);
    counter++;
    int handler_num = counter;
    pthread_mutex_unlock(&counter_mutex);

    printf("New connection(%d) is opened\n", counter);
    int socket = *((int*)arg);
    char buffer[1024] = {0};
    int valread;

    pthread_mutex_lock(&client_sockets_mutex);
    client_sockets = realloc(client_sockets, sizeof(int) * (client_sockets_count + 1));
    client_sockets[client_sockets_count++] = socket;
    pthread_mutex_unlock(&client_sockets_mutex);

    while((valread = read(socket, buffer, sizeof(buffer))) > 0) {
        printf("Received command: %s\n", buffer);
        
        if (strcmp(buffer, "get_cpu_architecture") == 0) {
            char arch[13] = {0};
            get_cpu_architecture(arch);
            send(socket, arch, strlen(arch), 0);
        } else if (strcmp(buffer, "get_logical_processors_count") == 0) {
            int count = get_logical_processors_count();
            char response[50];
            sprintf(response, "%d", count);
            send(socket, response, strlen(response), 0);
        } else {
            char* response = "Invalid command";
            send(socket, response, strlen(response), 0);
        }

        memset(buffer, 0, sizeof(buffer));
    }

    pthread_mutex_lock(&client_sockets_mutex);
    for (int i = 0; i < client_sockets_count; i++) {
        if (client_sockets[i] == socket) {
            client_sockets[i] = client_sockets[client_sockets_count - 1];
            client_sockets_count--;
            client_sockets = realloc(client_sockets, sizeof(int) * client_sockets_count);
            break;
        }
    }
    pthread_mutex_unlock(&client_sockets_mutex);

    printf("Connection(%d) closed\n", handler_num);
    shutdown(socket, 2);
    close(socket);
    return NULL;
}

void sigint_handler(int sig) {
    pthread_mutex_lock(&client_sockets_mutex);
    for (int i = 0; i < client_sockets_count; i++) {
        close(client_sockets[i]);
    }
    free(client_sockets);
    pthread_mutex_unlock(&client_sockets_mutex);
    exit(0);
}

int main() {
    setbuf(stdout, NULL);
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Создаем файловый дескриптор сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Привязываем сокет к порту 7701
    address.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &(address.sin_addr));
    address.sin_port = htons(7701);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("First server started\n");
    while(1) {
        int* new_socket_ptr = malloc(sizeof(int));
        if ((*new_socket_ptr = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            free(new_socket_ptr);
            continue;
        }

        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, handle_client, new_socket_ptr) != 0) {
            perror("could not create thread");
            return 1;
        }
    }

    return 0;
}

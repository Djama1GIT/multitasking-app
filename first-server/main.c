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
#include <json-c/json.h>

void logger(const char* tag, const char* message) {
   time_t now;
   time(&now);
   printf("%s [%s]: %s\n", ctime(&now), tag, message);
}


void get_cpu_architecture(char *arch) {
    unsigned int level = 0;
    unsigned int eax, ebx, ecx, edx;
    __get_cpuid(level, &eax, &ebx, &ecx, &edx);
    ((unsigned *)arch)[0] = ebx;
    ((unsigned *)arch)[1] = edx;
    ((unsigned *)arch)[2] = ecx;
}

int get_logical_processors_count() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

char* create_json(char* cpu_arch, int logical_processor_count) {
    json_object *jobj = json_object_new_object();
    
    json_object *jstring = json_object_new_string(cpu_arch);
    json_object *jint = json_object_new_int(logical_processor_count);

    if (strcmp(cpu_arch, "") != 0) json_object_object_add(jobj, "CpuArchitecture", jstring);
    if (logical_processor_count >= 0) json_object_object_add(jobj, "LogicalProcessorCount", jint);
    const char* temp = json_object_to_json_string(jobj);
    char* result = strdup(temp);
    logger("create_json", result);
    return result;
}

typedef struct {
    int client_id;
    char* subscription_type;
} Subscription;

Subscription* subscriptions = NULL;
int subscriptions_count = 0;
pthread_mutex_t subscriptions_mutex = PTHREAD_MUTEX_INITIALIZER;

void subscribe(int client_id, char* subscription_type) {
    logger("subscribe", subscription_type);
    pthread_mutex_lock(&subscriptions_mutex);
    for (int i = 0; i < subscriptions_count; i++) {
        if (subscriptions[i].client_id == client_id && strcmp(subscriptions[i].subscription_type, subscription_type) == 0) {
            pthread_mutex_unlock(&subscriptions_mutex);
            return;
        }
    }
    subscriptions = realloc(subscriptions, sizeof(Subscription) * (subscriptions_count + 1));
    Subscription new_subscription;
    new_subscription.client_id = client_id;
    new_subscription.subscription_type = strdup(subscription_type);
    subscriptions[subscriptions_count++] = new_subscription;
    pthread_mutex_unlock(&subscriptions_mutex);
}

void unsubscribe(int client_id, char* subscription_type) {
    logger("unsubscribe", subscription_type);
    pthread_mutex_lock(&subscriptions_mutex);
    for (int i = 0; i < subscriptions_count; i++) {
        if (subscriptions[i].client_id == client_id && strcmp(subscriptions[i].subscription_type, subscription_type) == 0) {
            free(subscriptions[i].subscription_type);
            subscriptions[i] = subscriptions[subscriptions_count - 1];
            subscriptions_count--;
            subscriptions = realloc(subscriptions, sizeof(Subscription) * subscriptions_count);
            pthread_mutex_unlock(&subscriptions_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&subscriptions_mutex);
}

void send_updates(char* subscription_type, char* update) {
    logger("send_updated", update);
    pthread_mutex_lock(&subscriptions_mutex);
    for (int i = 0; i < subscriptions_count; i++) {
        if (strcmp(subscriptions[i].subscription_type, subscription_type) == 0) {
            int client_id = subscriptions[i].client_id;
            sprintf(update, "%s updates for %d", update, client_id);
            logger("send_updated", update);
            send(client_id, update, strlen(update), 0);
        }
    }
    pthread_mutex_unlock(&subscriptions_mutex);
}

void* update_thread(void* arg) {
    char old_cpu_arch[1024] = {0};
    int old_logical_processor_count = -1;
    char current_cpu_arch[1024];
    int current_logical_processor_count;

    while(1) {
        get_cpu_architecture(current_cpu_arch);
        current_logical_processor_count = get_logical_processors_count();

        if (strcmp(current_cpu_arch, old_cpu_arch) != 0) {
            strncpy(old_cpu_arch, current_cpu_arch, sizeof(old_cpu_arch));
            char* response = create_json(current_cpu_arch, -1);
            send_updates("get_cpu_architecture", response);
        }

        if (current_logical_processor_count != old_logical_processor_count) {
            old_logical_processor_count = current_logical_processor_count;
            char* response = create_json("", current_logical_processor_count);
            send_updates("get_logical_processors_count", response);
        }

        sleep(1);  // Sleep for one second
    }

    return NULL;
}

pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

int* client_sockets = NULL;
int client_sockets_count = 0;
pthread_mutex_t client_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_client(void* arg) {
    pthread_mutex_lock(&counter_mutex);
    counter++;
    int handle_num = counter;
    pthread_mutex_unlock(&counter_mutex);

    printf("New connection(%d) is opened\n", counter);
    int socket = *((int*)arg);
    char buffer[1024] = {0};
    int valread;

    pthread_mutex_lock(&client_sockets_mutex);
    client_sockets = realloc(client_sockets, sizeof(int) * (client_sockets_count + 1));
    client_sockets[client_sockets_count++] = socket;
    pthread_mutex_unlock(&client_sockets_mutex);

    char cpu_arch[1024];
    get_cpu_architecture(cpu_arch);
    int logical_processor_count = get_logical_processors_count();
    
    while(1) {
        valread = read(socket, buffer, 1024);
        buffer[valread] = '\0';
        if (valread == 0) {
            break;
        }
        logger("command", buffer);
        if (strcmp(buffer, "subscribe_cpu_architecture") == 0) {
            char* initial_data = create_json(cpu_arch, -1);
            send(socket, initial_data, strlen(initial_data), 0);
            free(initial_data);
            subscribe(socket, "get_cpu_architecture");
        } else if (strcmp(buffer, "unsubscribe_cpu_architecture") == 0) {
            unsubscribe(socket, "get_cpu_architecture");
        } else if (strcmp(buffer, "subscribe_logical_processors_count") == 0) {
            char* initial_data = create_json("", logical_processor_count);
            send(socket, initial_data, strlen(initial_data), 0);
            free(initial_data);
            subscribe(socket, "get_logical_processors_count");
        } else if (strcmp(buffer, "unsubscribe_logical_processors_count") == 0) {
            unsubscribe(socket, "get_logical_processors_count");
        }
    }

    pthread_mutex_lock(&subscriptions_mutex);
    for (int i = 0; i < subscriptions_count; i++) {
        if (subscriptions[i].client_id == socket) {
            free(subscriptions[i].subscription_type);
            subscriptions[i] = subscriptions[subscriptions_count - 1];
            subscriptions_count--;
            subscriptions = realloc(subscriptions, sizeof(Subscription) * subscriptions_count);
        }
    }
    pthread_mutex_unlock(&subscriptions_mutex);

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

    printf("Connection(%d) closed\n", handle_num);
    shutdown(socket, 2);
    close(socket);
    free(arg);
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

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

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

    pthread_t update_thread_id;
    if(pthread_create(&update_thread_id, NULL, update_thread, NULL) != 0) {
        perror("could not create thread");
        return 1;
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

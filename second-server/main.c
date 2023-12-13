#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <stdarg.h>

#define LOG(level, format, ...) logger(level, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)

typedef enum {
        TRACE,   // Detailed information, typically of interest only when diagnosing problems.
        DEBUG,   // More detailed information.
        INFO,    // Confirmation that things are working as expected.
        WARNING, // An indication that something unexpected happened, or there may be a problem in the near future (e.g. ‘disk space low’). The software is still working as expected.
        ERROR,   // Due to a more serious problem, the software has not been able to perform some function.
        FATAL    // A very severe error event that will likely lead the application to abort.
    } LogLevel;


void logger(LogLevel level, const char* tag, const char* format, ...) {
    const char* levelStrings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

    if (level < TRACE || level > FATAL) {
        return; // Invalid log level.
    }

    time_t now;
    time(&now);
    printf("%s [%s] [%s]: ", ctime(&now), levelStrings[level], tag);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

int get_process_count() {
    int count = 0;
    struct dirent *de;
    DIR *dr = opendir("/proc");
    if (dr == NULL) {
        return -1;
    }
    while ((de = readdir(dr)) != NULL) {
        if (isdigit(de->d_name[0])) {
            count++;
        }
    }
    closedir(dr);
    return count;
}

int get_module_count(int pid) {
    char filename[256];
    FILE *fp;
    char line[256];
    int count = 0;
    sprintf(filename, "/proc/%d/maps", pid);
    fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }
    while (fgets(line, sizeof(line), fp)) {
        count++;
    }
    fclose(fp);
    return count;
}

char* create_json(int process_count, int module_count) {
    json_object *jobj = json_object_new_object();
    json_object *jint1 = json_object_new_int(process_count);
    json_object *jint2 = json_object_new_int(module_count);
    if (process_count >= 0) json_object_object_add(jobj, "ProcessCount", jint1);
    if (module_count >= 0) json_object_object_add(jobj, "ModuleCount", jint2);
    const char* temp = json_object_to_json_string(jobj);
    char* result = strdup(temp); 
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
    logger(INFO, "subscribe", "Client ID: %d, Subscription type: %s", client_id, subscription_type);
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
    logger(INFO, "unsubscribe", "Client ID: %d, Subscription type: %s", client_id, subscription_type);
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
    logger(INFO, "send updates", "Subscription type: %s, Update: %s", subscription_type, update);
    pthread_mutex_lock(&subscriptions_mutex);
    for (int i = 0; i < subscriptions_count; i++) {
        if (strcmp(subscriptions[i].subscription_type, subscription_type) == 0) {
            int client_id = subscriptions[i].client_id;
            logger(INFO, "send updates", "Client ID: %d, Subscription type: %s, Update: %s", client_id, subscription_type, update);
            send(client_id, update, strlen(update), 0);
        }
    }
    pthread_mutex_unlock(&subscriptions_mutex);
}

void* update_thread(void* arg) {
    int old_process_count = -1;
    int old_module_count = -1;
    int current_process_count;
    int current_module_count;

    while(1) {
        current_process_count = get_process_count();
        current_module_count = get_module_count(getpid());

        if (current_process_count != old_process_count) {
            old_process_count = current_process_count;
            char* response = create_json(current_process_count, -1);
            send_updates("get_process_count", response);
        }

        if (current_module_count != old_module_count) {
            old_module_count = current_module_count;
            char* response = create_json(-1, current_module_count);
            send_updates("get_module_count", response);
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

    logger(INFO, "new connection", "Client ID: %d", handle_num);
    int socket = *((int*)arg);
    char buffer[1024] = {0};
    int valread;

    pthread_mutex_lock(&client_sockets_mutex);
    client_sockets = realloc(client_sockets, sizeof(int) * (client_sockets_count + 1));
    client_sockets[client_sockets_count++] = socket;
    pthread_mutex_unlock(&client_sockets_mutex);

    int process_count = get_process_count();
    int module_count = get_module_count(getpid());

    while(1) {
        valread = read(socket, buffer, 1024);
        buffer[valread] = '\0';
        if (valread == 0) {
            break;
        }
        logger(INFO, "Command executed", "Client ID: %d, Comamnd: %s", socket, buffer);
        if (strcmp(buffer, "subscribe_process_count") == 0) {
            char* initial_data = create_json(process_count, -1);
            send(socket, initial_data, strlen(initial_data), 0);
            free(initial_data);
            subscribe(socket, "get_process_count");
        } else if (strcmp(buffer, "unsubscribe_process_count") == 0) {
            unsubscribe(socket, "get_process_count");
        } else if (strcmp(buffer, "subscribe_module_count") == 0) {
            char* initial_data = create_json(-1, module_count);
            send(socket, initial_data, strlen(initial_data), 0);
            free(initial_data);
            subscribe(socket, "get_module_count");
        } else if (strcmp(buffer, "unsubscribe_module_count") == 0) {
            unsubscribe(socket, "get_module_count");
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

    logger(INFO, "connection closed", "Client ID: %d", handle_num);
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
        LOG(ERROR, "An error occurred in sigaction: %s", strerror(errno));
        exit(1);
    }
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        LOG(ERROR, "An error occurred while socket creating: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &(address.sin_addr));
    address.sin_port = htons(7702);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        LOG(ERROR, "An error occurred while binding: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        LOG(ERROR, "An error occurred while listening: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pthread_t update_thread_id;
    if(pthread_create(&update_thread_id, NULL, update_thread, NULL) != 0) {
        LOG(ERROR, "An error occurred while creating thread: %s", strerror(errno));
        return 1;
    }

    printf("Second server started\n");
    while(1) {
        int* new_socket_ptr = malloc(sizeof(int));
        if ((*new_socket_ptr = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            LOG(ERROR, "Accept: %s", strerror(errno));
            free(new_socket_ptr);
            continue;
        }
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, handle_client, new_socket_ptr) != 0) {
            LOG(ERROR, "An error occurred while creating thread: %s", strerror(errno));
            return 1;
        }
    }
    return 0;
}


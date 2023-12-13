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

        json_object_object_add(jobj, "ProcessCount", jint1);
        json_object_object_add(jobj, "ModuleCount", jint2);

        return (char*) json_object_to_json_string(jobj);
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
        
        int old_process_count = -1;
        int old_module_count = -1;
        int current_process_count;
        int current_module_count;
        
        pthread_mutex_lock(&client_sockets_mutex);
        client_sockets = realloc(client_sockets, sizeof(int) * (client_sockets_count + 1));
        client_sockets[client_sockets_count++] = socket;
        pthread_mutex_unlock(&client_sockets_mutex);

        while(1) {
            current_process_count = get_process_count();
            current_module_count = get_module_count(getpid());

            if (current_process_count != old_process_count || current_module_count != old_module_count) {
                old_process_count = current_process_count;
                old_module_count = current_module_count;
                
                char* response = create_json(current_process_count, current_module_count);
                usleep(100000);
                ssize_t sendResult = send(socket, response, strlen(response), 0);
                if (sendResult == -1) {
                    break;
                }
            }
        
            sleep(1);  // Sleep for one second
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

        printf("Connection(%d) closed\n", handle_num);
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

        // Привязываем сокет к порту 7702
        address.sin_family = AF_INET;
        inet_pton(AF_INET, "0.0.0.0", &(address.sin_addr));
        address.sin_port = htons(7702);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        printf("Second server started\n");
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
#include <curses.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ncurses.h>
#include <locale.h>
#include <json-c/json.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

struct connection_info {
    int server_num;
    int sock;
    int connected;
    char* ip;
    int port;
    WINDOW* win;
};

bool subscribe_process;
bool subscribe_module;
bool subscribe_cpu_architecture;
bool subscribe_processors_count;

char* last_cpu_architecture = NULL;
char* last_logical_processors_count = NULL;
char* last_process_count = NULL;
char* last_module_count = NULL;

void show_commands(WINDOW* win) {
    werase(win);
    wattron(win, COLOR_PAIR(6));
    wprintw(win, "Commands:\n");
    wattroff(win, COLOR_PAIR(6));

    const char* commands[] = {
        "\n", "Servers:", "\n",
        "Press 1", " to ", "Connect", "/", "Disconnect", " to ", "first server", "\n",
        "Press 2", " to ", "Connect", "/", "Disconnect", " to ", "second server", "\n",

        "\n", "Subscriptions:", "\n",
        "Press 3", " to ", "Subscribe", "/", "Unsubscribe", " to ", "get_cpu_architecture", "\n",
        "Press 4", " to ", "Subscribe", "/", "Unsubscribe", " to ", "get_logical_processors_count", "\n",
        "Press 5", " to ", "Subscribe", "/", "Unsubscribe", " to ", "get_process_count", "\n",
        "Press 6", " to ", "Subscribe", "/", "Unsubscribe", " to ", "get_module_count", "\n",
    };

    for(int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if(strcmp(commands[i], "Disconnect") == 0 || strcmp(commands[i], "Unsubscribe") == 0) {
            wattron(win, COLOR_PAIR(2)); 
        } else if (strcmp(commands[i], "Subscribe") == 0) {
            wattron(win, COLOR_PAIR(1)); 
        }  else if (strcmp(commands[i], "Connect") == 0) {
            wattron(win, COLOR_PAIR(3)); 
        } else if(strcmp(commands[i], "Servers:") == 0 || strcmp(commands[i], "Subscriptions:") == 0) {
            wattron(win, COLOR_PAIR(4)); 
        } else if (strcmp(commands[i], "Press 1") == 0 || strcmp(commands[i], "Press 2") == 0 || 
                  strcmp(commands[i], "Press 3") == 0 || strcmp(commands[i], "Press 4") == 0 || 
                  strcmp(commands[i], "Press 5") == 0 || strcmp(commands[i], "Press 6") == 0) {
            wattron(win, COLOR_PAIR(5)); 
        } else if (strcmp(commands[i], "first server") == 0 || strcmp(commands[i], "second server") == 0 || 
                  strcmp(commands[i], "get_cpu_architecture") == 0 || strcmp(commands[i], "get_process_count") == 0 || 
                  strcmp(commands[i], "get_logical_processors_count") == 0 || strcmp(commands[i], "get_module_count") == 0) {
            wattron(win, COLOR_PAIR(7)); 
        }
        wprintw(win, "%s", commands[i]);
        wattroff(win, COLOR_PAIR(1)); 
        wattroff(win, COLOR_PAIR(2)); 
        wattroff(win, COLOR_PAIR(3)); 
        wattroff(win, COLOR_PAIR(4));  
        wattroff(win, COLOR_PAIR(5)); 
        wattroff(win, COLOR_PAIR(7)); 
    }

    touchwin(win);
    wrefresh(win);
}

void update_display(struct connection_info* info, 
                    char* last_cpu_architecture, char* last_logical_processors_count, 
                    char* last_process_count, char* last_module_count) {
    werase(info->win);

    
    if (info->connected) {
        wattron(info->win, COLOR_PAIR(3)); 
        wprintw(info->win, "Server %d connected\n", info->server_num);
        wattroff(info->win, COLOR_PAIR(3)); 
    } else {
        wattron(info->win, COLOR_PAIR(2)); 
        wprintw(info->win, "Server %d disconnected\n", info->server_num);
        wattroff(info->win, COLOR_PAIR(2)); 
    }
    

    // Display the last received values
    if (info->server_num == 1) {
        if (last_cpu_architecture != NULL) {
            wattron(info->win, subscribe_cpu_architecture ? COLOR_PAIR(1) : COLOR_PAIR(2));
            wprintw(info->win, "CPU Architecture: %s\n", last_cpu_architecture);
            wattroff(info->win, subscribe_cpu_architecture ? COLOR_PAIR(1) : COLOR_PAIR(2));
        }
        if (last_logical_processors_count != NULL) {
            wattron(info->win, subscribe_processors_count ? COLOR_PAIR(1) : COLOR_PAIR(2));
            wprintw(info->win, "Logical Processors Count: %s\n", last_logical_processors_count);
            wattroff(info->win, subscribe_processors_count ? COLOR_PAIR(1) : COLOR_PAIR(2));
        }
    } else if (info->server_num == 2) {
        if (last_process_count != NULL) {
            wattron(info->win, subscribe_process ? COLOR_PAIR(1) : COLOR_PAIR(2));
            wprintw(info->win, "Process Count: %s\n", last_process_count);
            wattroff(info->win, subscribe_process ? COLOR_PAIR(1) : COLOR_PAIR(2));
        }
        if (last_module_count != NULL) {
            wattron(info->win, subscribe_module ? COLOR_PAIR(1) : COLOR_PAIR(2));
            wprintw(info->win, "Module Count: %s\n", last_module_count);
            wattroff(info->win, subscribe_module ? COLOR_PAIR(1) : COLOR_PAIR(2));
        }
    }

    wrefresh(info->win);
}

void* receive_data(void* arg) {
    struct connection_info* info = (struct connection_info*)arg;
    char buffer[1024] = {0};
    int valread;
    
    werase(info->win);
    wattron(info->win, COLOR_PAIR(3)); 
    mvwprintw(info->win, 0, 0, "Server %d connected\n", info->server_num);
    wattroff(info->win, COLOR_PAIR(3)); 
    wattron(info->win, COLOR_PAIR(2)); 
    wprintw(info->win, "Server %d: Subscribe to any source\n", info->server_num);
    wattroff(info->win, COLOR_PAIR(2)); 
    wrefresh(info->win);

    if (info->server_num == 1) {
        last_cpu_architecture = NULL;
        last_logical_processors_count = NULL;
    } else if (info->server_num == 2){
        last_process_count = NULL;
        last_module_count = NULL;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;  // Set timeout to 5 seconds
    timeout.tv_usec = 0;
    

    if (setsockopt(info->sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        wprintw(info->win, "\nsetsockopt failed\n");
        wrefresh(info->win);
        return NULL;
    }

    while(info->connected) {
        valread = read(info->sock, buffer, sizeof(buffer));
        if (valread < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                wrefresh(info->win);
            } else {
                wprintw(info->win, "\nRead error: %s\n", strerror(errno));
                wrefresh(info->win);
                break;
            }
        } else if (valread == 0) {
            wattron(info->win, COLOR_PAIR(2)); 
            mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
            wattroff(info->win, COLOR_PAIR(2)); 
            pthread_cancel(pthread_self());
            info->connected = 0;
            wrefresh(info->win);
            if (info->server_num == 1) {
                subscribe_process = false;
                subscribe_module = false;
            } else if (info->server_num == 2) {
                subscribe_cpu_architecture = false;
                subscribe_processors_count = false;
            }
            break;
        }

        // Parse the JSON data
        json_object *jobj = json_tokener_parse(buffer);
        if (jobj == NULL) {
            wrefresh(info->win);
        } else {
            json_object_object_foreach(jobj, key, val) {
                // Update the last received values
                if (strcmp(key, "CpuArchitecture") == 0) {
                    if (last_cpu_architecture != NULL) free(last_cpu_architecture);
                    last_cpu_architecture = strdup(json_object_to_json_string(val));
                } else if (strcmp(key, "LogicalProcessorCount") == 0) {
                    if (last_logical_processors_count != NULL) free(last_logical_processors_count);
                    last_logical_processors_count = strdup(json_object_to_json_string(val));
                } else if (strcmp(key, "ProcessCount") == 0) {
                    if (last_process_count != NULL) free(last_process_count);
                    last_process_count = strdup(json_object_to_json_string(val));
                } else if (strcmp(key, "ModuleCount") == 0) {
                    if (last_module_count != NULL) free(last_module_count);
                    last_module_count = strdup(json_object_to_json_string(val));
                }
            }

            update_display(info, last_cpu_architecture, last_logical_processors_count, last_process_count, last_module_count);
            memset(buffer, 0, sizeof(buffer));
        }
    }

   if (last_cpu_architecture != NULL) free(last_cpu_architecture);
   if (last_logical_processors_count != NULL) free(last_logical_processors_count);
   if (last_process_count != NULL) free(last_process_count);
   if (last_module_count != NULL) free(last_module_count);

   return NULL;
}

void* check_connection(void* arg) {
    struct connection_info* info = (struct connection_info*)arg;
    char buffer[1]; 
    while(1) {
        int result = recv(info->sock, buffer, sizeof(buffer), MSG_PEEK);
        if (result == 0) {
            info->connected = 0;
            break;
        }
        sleep(1);
    }
    return NULL;
}

struct hostent *he;

int main() {
    setlocale(LC_ALL, "en_US.UTF-8");

    initscr();
    nodelay(stdscr, TRUE);
    cbreak();
    noecho();

    start_color();
    
    init_pair(1, COLOR_WHITE, COLOR_BLACK);  
    init_pair(2, COLOR_RED, COLOR_BLACK);  
    init_pair(3, COLOR_GREEN, COLOR_BLACK); 
    init_pair(4, COLOR_BLUE, COLOR_BLACK);  
    init_pair(5, COLOR_YELLOW, COLOR_BLACK); 
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);

    setbuf(stdout, NULL);

    struct connection_info connection1 = { .sock = 0, .connected = 0, .ip = "first-server", .port = 7701 };
    struct connection_info connection2 = { .sock = 0, .connected = 0, .ip = "second-server", .port = 7702 };

    struct sockaddr_in address;
    int addrlen = sizeof(address);

    pthread_t thread_id1, thread_id2, check_conn_thread_id1, check_conn_thread_id2;

    int h, w;
    getmaxyx(stdscr, h, w);

    WINDOW* command_win = newwin(h, w/2, 0, 0);
    WINDOW* server1_win = newwin(h/2, w/2, 0, w/2);
    WINDOW* server2_win = newwin(h/2, w/2, h/2, w/2);
    scrollok(server1_win, TRUE);
    scrollok(server2_win, TRUE);
    connection1.win = server1_win;
    connection1.server_num = 1;
    connection2.win = server2_win;
    connection2.server_num = 2;

    subscribe_process = false;
    subscribe_module = false;
    subscribe_cpu_architecture = false;
    subscribe_processors_count = false;

    show_commands(command_win);

    while(1) {
        int ch = getch();
        if (ch != ERR && (ch >= '0' && ch <= '6')) {
            int command_number = ch - '0';
            struct connection_info* info = NULL;
            pthread_t* thread_id = NULL;

            if (command_number == 0) {
                if (connection1.connected) {
                    pthread_cancel(thread_id1);
                    close(connection1.sock);
                }
                if (connection2.connected) {
                    pthread_cancel(thread_id2);
                    close(connection2.sock);
                }
                endwin();
                return 0;
            } else if (command_number <= 2) {
                info = command_number == 1 ? &connection1 : &connection2;
                thread_id = command_number == 1 ? &thread_id1 : &thread_id2;
            }

            if (info != NULL && command_number <= 2) {
                if (info->connected) {
                    pthread_cancel(*thread_id);
                    close(info->sock);
                    werase(info->win);
                    wattron(info->win, COLOR_PAIR(2)); 
                    mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                    wattroff(info->win, COLOR_PAIR(2)); 
                    wrefresh(info->win);
                    info->connected = 0;
                    if (info->server_num == 1) {
                        subscribe_process = false;
                        subscribe_module = false;
                    } else if (info->server_num == 2) {
                        subscribe_cpu_architecture = false;
                        subscribe_processors_count = false;
                    }
                } else {
                    info->sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (info->sock == -1) {
                        werase(info->win);
                        wattron(info->win, COLOR_PAIR(2)); 
                        mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                        wattroff(info->win, COLOR_PAIR(2)); 
                        wprintw(info->win, "\nSocket creation failed: %s\n", strerror(errno));
                        wrefresh(info->win);
                        if (info->server_num == 1) {
                            subscribe_process = false;
                            subscribe_module = false;
                        } else if (info->server_num == 2) {
                            subscribe_cpu_architecture = false;
                            subscribe_processors_count = false;
                        }
                        continue;
                    }

                    address.sin_family = AF_INET;
                    address.sin_port = htons(info->port);
                    if ((he = gethostbyname(info->ip)) == NULL) {
                        werase(info->win);
                        wattron(info->win, COLOR_PAIR(2)); 
                        mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                        wattroff(info->win, COLOR_PAIR(2)); 
                        wprintw(info->win, "\ngethostbyname error: %s\n", strerror(h_errno));
                        wrefresh(info->win);
                        close(info->sock);
                        if (info->server_num == 1) {
                            subscribe_process = false;
                            subscribe_module = false;
                        } else if (info->server_num == 2) {
                            subscribe_cpu_architecture = false;
                            subscribe_processors_count = false;
                        }
                        continue;
                    }
                    
                    memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);
                    if (connect(info->sock, (struct sockaddr *)&address, addrlen) < 0) {
                        werase(info->win);
                        wattron(info->win, COLOR_PAIR(2)); 
                        mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                        wattroff(info->win, COLOR_PAIR(2)); 
                        wprintw(info->win, "\nConnection Failed: %s\n", strerror(errno));
                        wrefresh(info->win);
                        close(info->sock);
                        if (info->server_num == 1) {
                            subscribe_process = false;
                            subscribe_module = false;
                        } else if (info->server_num == 2) {
                            subscribe_cpu_architecture = false;
                            subscribe_processors_count = false;
                        }
                        continue;
                    }

                    if (pthread_create(thread_id, NULL, receive_data, info) != 0) {
                        werase(info->win);
                        wattron(info->win, COLOR_PAIR(2)); 
                        mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                        wattroff(info->win, COLOR_PAIR(2)); 
                        wprintw(info->win, "\nFailed to create thread: %s\n", strerror(errno));
                        wrefresh(info->win);
                        close(info->sock);
                        if (info->server_num == 1) {
                            subscribe_process = false;
                            subscribe_module = false;
                        } else if (info->server_num == 2) {
                            subscribe_cpu_architecture = false;
                            subscribe_processors_count = false;
                        }
                        continue;
                    }

                    werase(info->win);
                    wattron(info->win, COLOR_PAIR(3)); 
                    mvwprintw(info->win, 0, 0, "Server %d connected\n", info->server_num);
                    wattroff(info->win, COLOR_PAIR(3)); 
                    wrefresh(info->win);
                    info->connected = 1;
                }
                usleep(60000);
            } else if (command_number <= 4) {
                const char* command;
                if (command_number == 3) {
                    command = subscribe_cpu_architecture ? "unsubscribe_cpu_architecture" : "subscribe_cpu_architecture";
                    subscribe_cpu_architecture = !subscribe_cpu_architecture; 
                } else if (command_number == 4) {
                    command = subscribe_processors_count ? "unsubscribe_logical_processors_count" : "subscribe_logical_processors_count";
                    subscribe_processors_count = !subscribe_processors_count; 
                }
                update_display(&connection1, last_cpu_architecture, last_logical_processors_count, last_process_count, last_module_count);
                if (connection1.connected) {
                    send(connection1.sock, command, strlen(command), 0);
                }
            } else if (command_number <= 6) {
                const char* command;
                if (command_number == 5) {
                    command = subscribe_process ? "unsubscribe_process_count" : "subscribe_process_count";
                    subscribe_process = !subscribe_process; 
                } else if (command_number == 6) {
                    command = subscribe_module ? "unsubscribe_module_count" : "subscribe_module_count";
                    subscribe_module = !subscribe_module; 
                }
                update_display(&connection2, last_cpu_architecture, last_logical_processors_count, last_process_count, last_module_count);
                if (connection2.connected) {
                    send(connection2.sock, command, strlen(command), 0);
                }
            }
        }
        
        usleep(60000);
        touchwin(command_win);
        wrefresh(command_win);

    }

    endwin();
    return 0;
}

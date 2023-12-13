#include <curses.h>
#include <errno.h>
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

struct connection_info {
    int server_num;
    int sock;
    int connected;
    char* ip;
    int port;
    WINDOW* win;
};


void* receive_data(void* arg) {
    struct connection_info* info = (struct connection_info*)arg;
    char buffer[1024] = {0};
    int valread;

    FILE *log_file = fopen("server_log.txt", "a");
    if (log_file == NULL) {
        wprintw(info->win, "\nFailed to open log file\n");
        return NULL;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;  // Устанавливаем таймаут в 5 секунд
    timeout.tv_usec = 0;

    if (setsockopt(info->sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        wprintw(info->win, "\nsetsockopt failed\n");
        return NULL;
    }

    
    while(1) {
        valread = read(info->sock, buffer, sizeof(buffer));
        werase(info->win);
        if (valread < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                wprintw(info->win, "Timeout occurred\n");
            } else {
                wprintw(info->win, "\nRead error: %s\n", strerror(errno));
                break;
            }
        } else if (valread == 0) {
            wattron(info->win, COLOR_PAIR(2)); 
            mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
            wattroff(info->win, COLOR_PAIR(2)); 
            pthread_cancel(pthread_self());
            info->connected = 0;
            wrefresh(info->win);
            break;
        }

        wattron(info->win, COLOR_PAIR(3)); 
        wprintw(info->win, "Server %d connected\n", info->server_num);
        wattroff(info->win, COLOR_PAIR(3)); 
        // Get current time
        time_t rawtime;
        struct tm * timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        // Parse the JSON data
        json_object *jobj = json_tokener_parse(buffer);
        if (jobj == NULL) {
            wprintw(info->win, "Failed to parse JSON data: %s\n", buffer);
        } else {
            json_object_object_foreach(jobj, key, val) {
                wprintw(info->win, "%s: %s\n", key, json_object_to_json_string(val));
                fprintf(log_file, "[%02d-%02d-%04d %02d:%02d:%02d] Server %d: %s: %s\n", 
                        timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, 
                        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, 
                        info->server_num, key, json_object_to_json_string(val));
            }
            fflush(log_file);

            wrefresh(info->win);
            memset(buffer, 0, sizeof(buffer));
        }
    }

    fclose(log_file);
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

void show_commands(WINDOW* win) {
    werase(win);
    wattron(win, COLOR_PAIR(4));
    wprintw(win, "Commands:\n");
    wattroff(win, COLOR_PAIR(4));

    wattron(win, COLOR_PAIR(5));
    wprintw(win, "Press 1 ");
    wattroff(win, COLOR_PAIR(5));
    wattron(win, COLOR_PAIR(1));
    wprintw(win, "to Connect/Disconnect to ");
    wattroff(win, COLOR_PAIR(1));
    wattron(win, COLOR_PAIR(3));
    wprintw(win, "first server\n");
    wattroff(win, COLOR_PAIR(3));

    wattron(win, COLOR_PAIR(5));
    wprintw(win, "Press 2 ");
    wattroff(win, COLOR_PAIR(5));
    wattron(win, COLOR_PAIR(1));
    wprintw(win, "to Connect/Disconnect to ");
    wattroff(win, COLOR_PAIR(1));
    wattron(win, COLOR_PAIR(3));
    wprintw(win, "second server\n");
    wattroff(win, COLOR_PAIR(3));

    touchwin(win);
    wrefresh(win);
}

struct hostent *he;

int main() {
    setlocale(LC_ALL, "en_US.UTF-8");

    initscr();
    nodelay(stdscr, TRUE);
    cbreak();
    noecho();

    start_color();
    
    // Определяем цветовые пары
    init_pair(1, COLOR_WHITE, COLOR_BLACK);  // Белый текст на черном фоне
    init_pair(2, COLOR_RED, COLOR_BLACK);    // Красный текст на черном фоне
    init_pair(3, COLOR_GREEN, COLOR_BLACK);  // Зеленый текст на черном фоне
    init_pair(4, COLOR_BLUE, COLOR_BLACK);  // Синий текст на черном фоне
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);  // Синий текст на черном фоне

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

    show_commands(command_win);

    while(1) {
        int ch = getch();
        if (ch != ERR && (ch >= '0' && ch <= '2')) {
            int command_number = ch - '0';
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
            }

            struct connection_info* info = command_number == 1 ? &connection1 : &connection2;
            pthread_t* thread_id = command_number == 1 ? &thread_id1 : &thread_id2;

            if (info->connected) {
                pthread_cancel(*thread_id);
                close(info->sock);
                werase(info->win);
                wattron(info->win, COLOR_PAIR(2)); 
                mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                wattroff(info->win, COLOR_PAIR(2)); 
                wrefresh(info->win);
                info->connected = 0;
            } else {
                info->sock = socket(AF_INET, SOCK_STREAM, 0);
                if (info->sock == -1) {
                    werase(info->win);
                    wattron(info->win, COLOR_PAIR(2)); 
                    mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                    wattroff(info->win, COLOR_PAIR(2)); 
                    wprintw(info->win, "\nSocket creation failed: %s\n", strerror(errno));
                    wrefresh(info->win);
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
                    continue;
                }

                if (pthread_create(command_number == 1 ? &check_conn_thread_id1 : &check_conn_thread_id2, NULL, check_connection, info) != 0) {
                    werase(info->win);
                    wattron(info->win, COLOR_PAIR(2)); 
                    mvwprintw(info->win, 0, 0, "Server %d disconnected\n", info->server_num);
                    wattroff(info->win, COLOR_PAIR(2)); 
                    wprintw(info->win, "\nFailed to create thread: %s\n", strerror(errno));
                    wrefresh(info->win);
                    pthread_cancel(*thread_id);
                    close(info->sock);
                    continue;
                }
                pthread_create(thread_id, NULL, receive_data, info);
                pthread_create(command_number == 1 ? &check_conn_thread_id1 : &check_conn_thread_id2, NULL, check_connection, info);
                werase(info->win);
                wattron(info->win, COLOR_PAIR(3)); 
                mvwprintw(info->win, 0, 0, "Server %d connected\n", info->server_num);
                wattroff(info->win, COLOR_PAIR(3)); 
                wrefresh(info->win);
                info->connected = 1;
            }
            usleep(60000);
        }
        usleep(60000);
        touchwin(command_win);
        wrefresh(command_win);
    }

    return 0;
}

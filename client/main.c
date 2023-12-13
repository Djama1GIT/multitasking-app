#include <curses.h>
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

void send_command(int sock, const char* command) {
   char buffer[1024] = {0};
   int valread;

   // Отправляем команду серверу
   send(sock, command, strlen(command), 0);

   // Читаем ответ от сервера
   valread = read(sock, buffer, 1024);
   if (strcmp(buffer, "") == 0) {
        printw("\n\nПотеряно соединение с сервером\n");
        exit(-1);
   }
   printw("%s", buffer);

   memset(buffer, 0, sizeof(buffer));
}

void show_commands() {
    attron(COLOR_PAIR(1));
    printw("Commands:\n");
    attroff(COLOR_PAIR(1));
    
    attron(COLOR_PAIR(2));
    printw("Server 1:\n");
    attroff(COLOR_PAIR(2));
    
    attron(COLOR_PAIR(4));
    printw("1 - Print CPU architecture\n");
    printw("2 - Print the number of logical processors\n");
    attroff(COLOR_PAIR(4));
    
    attron(COLOR_PAIR(2));
    printw("Server 2:\n");
    attroff(COLOR_PAIR(2));
    
    attron(COLOR_PAIR(4));
    printw("3 - Print the number of system processes\n");
    printw("4 - Print the number of server process modules\n");
    attroff(COLOR_PAIR(4));
    
    attron(COLOR_PAIR(1));
    printw("Press on a command number (0 to exit)\n");
    attroff(COLOR_PAIR(1));

    nodelay(stdscr, TRUE);
}

struct hostent *he;

int main() {
   setlocale(LC_ALL, "en_US.UTF-8");

   initscr();
   cbreak();
   noecho();

   // Инициализируем систему цветов
   start_color();   

   // Определяем цветовые пары
   init_pair(1, COLOR_WHITE, COLOR_BLACK);  // Белый текст на черном фоне
   init_pair(2, COLOR_GREEN, COLOR_BLACK);  // Зеленый текст на черном фоне
   init_pair(3, COLOR_BLUE, COLOR_BLACK);  // Синий текст на черном фоне
   init_pair(4, COLOR_RED, COLOR_BLACK);    // Красный текст на черном фоне

   setbuf(stdout, NULL);
   char* server1_ip = "first-server";
   int server1_port = 7701;

   char* server2_ip = "second-server";
   int server2_port = 7702;

   int sock1, sock2;
   struct sockaddr_in server1, server2;

   // Создаем сокеты
   if ((sock1 = socket(AF_INET, SOCK_STREAM, 0)) < 0 || (sock2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
       perror("Socket creation error");
       return 1;
   }

   server1.sin_family = AF_INET;
   server1.sin_port = htons(server1_port);
   server2.sin_family = AF_INET;
   server2.sin_port = htons(server2_port);

   if ((he = gethostbyname(server1_ip)) == NULL) {
       perror("gethostbyname error");
       return 1;
   }
   memcpy(&server1.sin_addr, he->h_addr_list[0], he->h_length);

   if ((he = gethostbyname(server2_ip)) == NULL) {
       perror("gethostbyname error");
       return 1;
   }
   memcpy(&server2.sin_addr, he->h_addr_list[0], he->h_length);

   // Подключаемся к серверам
   if (connect(sock1, (struct sockaddr *)&server1, sizeof(server1)) < 0 || connect(sock2, (struct sockaddr *)&server2, sizeof(server2)) < 0) {
       perror("Connection Failed");
       return 1;
   }

   char command[1024];
   int command_number;

   show_commands();
   while(1) {    
        int ch;
        while(1) {
            ch = getch();
            if (ch >= '0' && ch <= '4') {
                command_number = ch - '0';
                break;
            }
        }

        nodelay(stdscr, FALSE);
    
        if (command_number == 0) {
            break;
        } else if (command_number < 1 || command_number > 4) {
            clear();
            attron(COLOR_PAIR(1));
            printw("Invalid command number\n");
            attroff(COLOR_PAIR(1));
            continue;
        }
    
        switch (command_number) {
            case 1:
                clear();
                show_commands();
                strcpy(command, "get_cpu_architecture");
                attron(COLOR_PAIR(2));
                printw("Server 1: CPU Architecture: ");
                attroff(COLOR_PAIR(2));
                send_command(sock1, command);
                printw("\n");
                break;
            case 2:
                clear();
                show_commands();
                strcpy(command, "get_logical_processors_count");
                attron(COLOR_PAIR(2));
                printw("Server 1: Number of Logical Processors: ");
                attroff(COLOR_PAIR(2));
                send_command(sock1, command);
                printw("\n");
                break;
            case 3:
                clear();
                show_commands();
                strcpy(command, "get_process_count");
                attron(COLOR_PAIR(3));
                printw("Server 2: Number of System Processes: ");
                attroff(COLOR_PAIR(3));
                send_command(sock2, command);
                printw("\n");
                break;
            case 4:
                clear();
                show_commands();
                strcpy(command, "get_module_count");
                attron(COLOR_PAIR(3));
                printw("Server 2: Number of Server Process Modules: ");
                attroff(COLOR_PAIR(3));
                send_command(sock2, command);
                printw("\n");
                break;
        }
    }

   // Закрываем сокеты
   close(sock1);
   close(sock2);

   endwin();

   return 0;
}

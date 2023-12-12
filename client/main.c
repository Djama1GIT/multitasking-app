#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

void send_command(int sock, const char* command) {
   char buffer[1024] = {0};
   int valread;

   // Отправляем команду серверу
   send(sock, command, strlen(command), 0);

   // Читаем ответ от сервера
   valread = read(sock, buffer, 1024);
   if (strcmp(buffer, "") == 0) {
        printf("\n\nПотеряно соединение с сервером\n");
        exit(-1);
   }
   printf("%s", buffer);

   memset(buffer, 0, sizeof(buffer));
}

struct hostent *he;

int main() {
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

   while(1) {
       printf("\nКоманды:\n");
       printf("Сервер 1:\n");
       printf("1 - Вывести архитектуру процессора\n");
       printf("2 - Вывести количество логических процессоров\n");
       printf("Сервер 2:\n");
       printf("3 - Вывести количество процессов в системе\n");
       printf("4 - Вывести количество модулей серверного процесса\n");
       printf("Введите номер команды (0 для выхода): ");
       scanf("%d", &command_number);
       getchar(); // Читаем символ новой строки, который остался после вызова scanf

       if (command_number == 0) {
           break;
       } else if (command_number < 1 || command_number > 4) {
           printf("Неверный номер команды\n");
           continue;
       }

       switch (command_number) {
           case 1:
               strcpy(command, "get_cpu_architecture");
               printf("Сервер 1: Архитектура CPU: ");
               send_command(sock1, command);
               break;
           case 2:
               strcpy(command, "get_logical_processors_count");
               printf("Сервер 1: Количество логических процессоров: ");
               send_command(sock1, command);
               break;
           case 3:
               strcpy(command, "get_process_count");
               printf("Сервер 2: Количество процессов в системе: ");
               send_command(sock2, command);
               break;
           case 4:
               strcpy(command, "get_module_count");
               printf("Сервер 2: Количество модулей серверного процесса: ");
               send_command(sock2, command);
               break;
       }
       printf("\n");
   }

   // Закрываем сокеты
   close(sock1);
   close(sock2);

   return 0;
}

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <string.h>

enum errors {
    OK,
    ERR_INCORRECT_ARGS,
    ERR_SOCKET,
    ERR_SETSOCKETOPT,
    ERR_BIND,
    ERR_LISTEN
};

int init_socket(int port) {
    int server_socket, socket_option = 1;
    struct sockaddr_in server_address;

    //open socket, return socket descriptor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Fail: open socket");
        exit(ERR_SOCKET);
    }

    //set socket option
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &socket_option, (socklen_t) sizeof socket_option);
    if (server_socket < 0) {
        perror("Fail: set socket options");
        exit(ERR_SETSOCKETOPT);
    }

    //set socket address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *) &server_address, (socklen_t) sizeof server_address) < 0) {
        perror("Fail: bind socket address");
        exit(ERR_BIND);
    }

    //listen mode start
    if (listen(server_socket, 5) < 0) {
        perror("Fail: bind socket address");
        exit(ERR_LISTEN);
    }
    return server_socket;
}

int getFname(char *fname, int client_socket) {
    int size = 0;
    char http[28];
    while(read(client_socket, &(fname[size]), 1) > 0) {
        size++;
        if (fname[size-1] == ' ') {
            fname[size-1] = 0;
            break;
        }
    }
    read(client_socket, http, 28);
    return size;
}
//
void writeNum(int client_socket, int size) {
    char buf[1000] = {0};
    char bufSize = 0;
    int tmp = size;
    while (tmp > 0) {
        bufSize++;
        tmp /= 10;
    }
    if (size == 0) {
        bufSize++;
    }
    snprintf(buf, bufSize + 1, "%d", size);
    write(client_socket, buf, bufSize); 
}

void sendDataBack(char *fname, int client_socket) {
    char ch;
    int fd =  open(fname, O_RDONLY);
    if (fd > 0) {
        char h1[] = "HTTP/1.1 200\n";
        char h2[] = "content-type: html/text\n";
        char h3[] = "content-length: ";
        write(client_socket, h1, strlen(h1));
        write(client_socket, h2, strlen(h2));
        write(client_socket, h3, strlen(h3));
        int size = 0; 
        char buf[10000] = {0};
        while (read(fd, &ch, 1)) {
            buf[size] = ch;
            size++;
        }
        writeNum(client_socket, size);
        write(client_socket, "\n\n", 2);
        write(client_socket, buf, size);
    } else {
        char h[] = "HTTP/1.1 404\n";
        write(client_socket, h, strlen(h));
    }
}

void infoExchange (int client_socket) {
    int j = 0;
    char ch;
    while (read(client_socket, &ch, 1) > 0) {
        if (ch == ' ') {
            char fname[100] = {0};
            j = getFname(fname, client_socket);
            if (fork() == 0) {
                sendDataBack(fname, client_socket);
                exit(0);
            }
            wait(NULL);
            j = 0; 
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        puts("Incorrect args.");
        puts("./server <port> <clients>");
        puts("Example:");
        puts("./server 5000 2");
        return ERR_INCORRECT_ARGS;
    }
    int port = atoi(argv[1]);
    int server_socket = init_socket(port);
    puts("Wait for connection");
    struct sockaddr_in client_address;
    socklen_t size = sizeof(client_address);
    int clients = atoi(argv[2]);
    int *client_socket = malloc(clients * sizeof(int));

    for (int i = 0; i < clients; i++) {
        if (fork() == 0) {
            client_socket[i] = accept(server_socket, 
                               (struct sockaddr *) &client_address,
                               &size);
            infoExchange(client_socket[i]);
            close(client_socket[i]);
            exit(0);
        }
    }

    for (int i = 0; i < clients; i++) {
        wait(NULL);
    }

    free(client_socket);
    return OK;
}

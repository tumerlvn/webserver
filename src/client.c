#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <sys/wait.h>

enum errors {
    OK,
    ERR_INCORRECT_ARGS,
    ERR_SOCKET,
    ERR_CONNECT
};

int init_socket(const char *ip, int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *host = gethostbyname(ip);
    struct sockaddr_in server_address;

    //open socket, result is socket descriptor
    if (server_socket < 0) {
        perror("Fail: open socket");
        exit(ERR_SOCKET);
    }

    //prepare server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    memcpy(&server_address.sin_addr, host -> h_addr_list[0],
           (socklen_t) sizeof server_address.sin_addr);

    //connection
    if (connect(server_socket, (struct sockaddr*) &server_address,
        (socklen_t) sizeof server_address) < 0) {
        perror("Fail: connect");
        exit(ERR_CONNECT);
    }
    return server_socket;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        puts("Incorrect args.");
        puts("./client <ip> <port>");
        puts("Example:");
        puts("./client 127.0.0.1 5000");
        return ERR_INCORRECT_ARGS;
    }
    char *ip = argv[1];
    int port = atoi(argv[2]);
    int server = init_socket(ip, port);

    char ch;
    
    puts("Write data:");
    if (fork() == 0) {
        int j = 0;
        while (read(server, &ch, 1) >= 0) {
            write(1, &ch, 1);
        }
        exit(0);
    }
    char word[100] = {0};
    int j = 0;
    while (read(0, &(word[j]), 1) >= 0) {
        j++;
        if (word[j-1] == '\n' || word[j-1] == ' ') {
            char h1[] = "GET ";
            char h2[] = " HTTP/1.1\nHost: mymath.info\n\n";
            write(server, h1, strlen(h1));
            write(server, word, j-1); 
            write(server, h2, strlen(h2));
            j = 0;
        }
    }

    wait(NULL);
    close(server);
    return OK;
}

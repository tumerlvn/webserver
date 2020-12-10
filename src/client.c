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

int init_socket(const char *ip, int port);
char *getWord();
int getIpAndPort(char *request, char **inIp, int *inPort);
int send_request(int server, char* word);
char* request_path(char* request, int index);


int main(int argc, char **argv) {
    puts("Write data:");
    char ch;
    int server = -1;
    int index;
    char *ip = NULL;
    int port = 0;   

    char *request = getWord(); 
    while (1) {
        index = getIpAndPort(request, &ip, &port);
        if (index != -1) {
            puts(ip);
            printf("%d\n%d\n", port, index);

            close(server);
            server = init_socket(ip, port);
       
            if (fork() == 0) {
                int j = 0;
                while (read(server, &ch, 1) > 0) {
                    write(1, &ch, 1);
                }
                close(server);
                exit(0);
            }
            char *word = request_path(request, index);
            puts(word);
            send_request(server, word);
            free(word);
            free(ip);
        }
        free(request);
        request = getWord();

    }
    free(request); 
    return OK;
}

char* request_path(char* request, int index) {
    int j = index;
   
    char *word = NULL;
    int size = 0;
    while (request[j] != '\0' && request[j] != ' ') {
        size++;
        word = realloc(word, size * sizeof(char));
        word[size-1] = request[j];
        j++;
    }
    size++;
    word = realloc(word, size * sizeof(char));
    word[size-1] = '\0';
    return word;
}

int send_request(int server, char* word) {
    char h1[] = "GET /";
    char h2[] = " HTTP/1.1\r\nHost: mymath.info\r\n\r\n";
    write(server, h1, strlen(h1));
    write(server, word, strlen(word)); 
    write(server, h2, strlen(h2));
}

char *getWord() {
    char *word = NULL;
    char ch;
    int size = 0;
    ch = getchar();
    while (ch != '\n' && ch != ' ') {
        size++;
        word = realloc(word, size * sizeof(char));
        word[size-1] = ch;
        ch = getchar();
    }
    size++;
    word = realloc(word, size * sizeof(char));
    word[size-1] = '\0';
    return word;
}

int getIpAndPort(char *request, char **inIp, int *inPort) { //returns index of '/' + 1;
    int i = 0;
    int i1 = -1, i2 = -1;
    while (request[i] != '\0') {
        if (request[i] == ':' && i1 < 0) {
            i1 = i;
        } else if (request[i] == '/' && i2 < 0) {
            i2 = i;
        }
        i++;
    }
    if (i1 <= 0 || i2 <= 0 || (i2 - i1) <= 1) {
        return -1;
    } else {
        i = 0;
        char *ip = NULL;
        int port = 0;
        while (request[i] != ':') {
            i++;
            ip = realloc(ip, i * sizeof(char)); 
            ip[i-1] = request[i-1];
        }
        i++;
        ip = realloc(ip, i * sizeof(char));
        ip[i-1] = '\0';
        while (request[i] != '/') {
            port *= 10;
            port += request[i] - '0';
            i++;
        }
        i++;
        if (i == strlen(request)) {
            free(ip);
            return -1;
        } else {
            *inIp = ip;
            *inPort = port;
            return i;
        }
    }
}

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

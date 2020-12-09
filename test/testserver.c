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

/*
int isEndline(char ch);
char* getFname(int client_socket, int *flag);
void writeNum(int client_socket, int size);
void sendDataBack(char *fname, int client_socket);
void infoExchange (int client_socket);
*/

int init_socket(int port);
int interaction_client(int client_socket);
char* get_request(int client_socket);
char* get_path(char* request_text);
void send_to_client(int client_socket, char* request_path);
void c200(int client_socket, int fd);
void c404(int client_socket);

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
        client_socket[i] = accept(server_socket, 
                           (struct sockaddr *) &client_address,
                           &size);
        printf("Client-#%d connected\n", i);
        if (fork() == 0) {
            interaction_client(client_socket[i]);
            exit(0);
        }
        close(client_socket[i]);
    }

    for (int i = 0; i < clients; i++) {
        wait(NULL);
    }

    free(client_socket);
    return OK;
}

int interaction_client(client_socket) {
    while(1) {
        char *request_text = get_request(client_socket);
        if (request_text && strlen(request_text)) {
            puts(request_text);
            char *request_path = get_path(request_text);
            send_to_client(client_socket, request_path); 
            free(request_text);
        } else {
            free(request_text);
            break;
        }
    }
    return 0;
}

void send_to_client(int client_socket, char* request_path) {
    int fd =  open(request_path + 1, O_RDONLY);
    if (fd < 0) {
        puts("NOT FOUND");
        c404(client_socket);
        return;
    } else {
        c200(client_socket, fd);
    }

    close(fd);
}

void c200(int client_socket, int fd) {
    char h1[] = "HTTP/1.1 200\r\ncontent-type: text/html\r\ncontent-length: %d\r\n\r\n";
    char h2[] = "<!DOCTYPE html><html><h1>";
    char h3[] = "</h1></html>";
    int size = 0; 
    char* buf = NULL;
    char msg[100];
    char ch;
    while (read(fd, &ch, 1) > 0) {
        size++;
        buf = realloc(buf, size * sizeof(char));
        buf[size-1] = ch;
    }
    size++;
    buf = realloc(buf, size * sizeof(char));
    buf[size-1] = '\0';
    sprintf(msg, h1, strlen(h2) + strlen(buf) + strlen(h3));
    write(client_socket, msg, strlen(msg));
    write(client_socket, h2, strlen(h2));
    write(client_socket, buf, strlen(buf));
    write(client_socket, h3, strlen(h3));
    free(buf);
}

void c404(int client_socket) {
    //char h[] = "HTTP/1.1 404 Not Found\r\n\r\n";
    //write(client_socket, h, strlen(h));
    char h1[] = "HTTP/1.1 404\r\ncontent-type: text/html\r\ncontent-length: %d\r\n\r\n";
    char buf[100];
    char h2[] = "<!DOCTYPE html><html><h1>Not Found</h1></html>";
    sprintf(buf, h1, strlen(h2));
    write(client_socket, buf, strlen(buf));
    write(client_socket, h2, strlen(h2));
}

char* get_path(char* request_text) {
    char *req;
    req = strtok(request_text, " \r\n");
    req = strtok(NULL, " \r\n");
    puts(req);
    return req;
}

char* get_request(client_socket) {
    char *request_text = NULL;
    int size = 0;
    char ch, prev = '\0';
    while (read(client_socket, &ch, 1) > 0) {
        size++;
        request_text = realloc(request_text, size * sizeof(char));
        request_text[size-1] = ch;
        if (prev == '\n' && ch == '\r') {
            read(client_socket, &ch, 1);
            size++;
            request_text = realloc(request_text, size * sizeof(char));
            request_text[size-1] = ch;
            break;
        }
        prev = ch;
    }
    size++;
    request_text = realloc(request_text, size * sizeof(char));
    request_text[size-1] = '\0';
    return request_text;
}

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


/*
int isEndline(char ch) {
    return ch == '\n' || ch == '\r';
}

char* getFname(int client_socket, int *flag) {
    char ch;
    char buf[100] = {0};
    int i = 0;
    char *req;
    while(read(client_socket, &ch, 1) > 0) {
        //write(1, &ch, 1);
        buf[i] = ch;
        i++;
        if (isEndline(ch)) {
            req = strtok(buf, " \r\n");
            puts(req);
            req = strtok(NULL, " \r\n");
            puts(req);
            *flag = 1;
            return req;
        }
    }
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
        char h1[] = "HTTP/1.1 200\r\n";
        char h2[] = "content-type: html/text\r\n";
        char h3[] = "content-length: ";
        char h4[] = "<html>";
        char h5[] = "</html>";
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
        write(client_socket, "\r\n\r\n", 4);
        write(client_socket, h4, strlen(h4));
        write(client_socket, buf, size);
        write(client_socket, h5, strlen(h5));
    } else {
        char h[] = "HTTP/1.1 404\n";
        write(client_socket, h, strlen(h));
    }
}

void infoExchange (int client_socket) {
    int j = 0;
    char ch;
    int i = 0;
    char buf[100] = {0};
    char *fname;
    char prev = '\0';
    int flag = 0;
    while (1) {
        if (!flag) {
            fname = getFname(client_socket, &flag); 
        }
        if (!flag && fork() == 0) {
            sendDataBack(fname, client_socket);
            exit(0);
        }
        wait(NULL);
    }
}
*/

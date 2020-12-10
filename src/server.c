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

typedef struct {
    char *ext;
    char *mediatype;
} extn;

//Possible media types
extn addition[] = {
    {"gif", "image/gif" },
    {"txt", "text/html" },//text/plain
    {"jpg", "image/jpg" },
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"ico", "image/ico" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {"php", "text/html" },
    {"pdf","application/pdf"},
    {"zip","application/octet-stream"},
    {"rar","application/octet-stream"},
    {0,0} 
};

int init_socket(int port);
int interaction_client(int client_socket);
char* get_request(int client_socket);
char* get_path(char* request_text);
void send_to_client(int client_socket, char* request_path, char* ext);
void c200(int client_socket, int fd, int index_extension);
void c404(int client_socket);
int run_binary(int client_socket, char* path);
int file_exists(char* path);
char* create_buf(int fd);

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

int interaction_client(int client_socket) {
    while(1) {
        char *request_text = get_request(client_socket);
        if (request_text && strlen(request_text)) {
            puts(request_text);
            char *request_path = get_path(request_text);
            char *ext = strchr(request_path, '.');
            if (!file_exists(request_path)) {
                c404(client_socket); 
            } else if (ext != NULL) {
                send_to_client(client_socket, request_path, ext + 1); 
            } else {
                run_binary(client_socket, request_path);
            }
            free(request_text);
        } else {
            free(request_text);
            break;
        }
    }
    puts("break");
    return 0;
}

char* create_buf(int fd) {
    char* buf = NULL;
    int size = 0;
    char ch;
    while (read(fd, &ch, 1) > 0) {
        size++;
        buf = realloc(buf, size * sizeof(char));
        buf[size-1] = ch;
    }
    size++;
    buf = realloc(buf, size * sizeof(char));
    buf[size-1] = '\0';
    return buf;
}

int file_exists (char* name) {
    return (access(name + 1, F_OK) != -1);
}

int run_binary(int client_socket, char* path) {//int run_binary(path, query_string)
    int pid1 = fork(); 
    if (pid1 == 0) {
        int fd[2];
        pipe(fd);
        int pid = fork();

        if (pid == 0) {
            dup2(fd[1], 1);
            close(fd[0]);
            close(fd[1]);
            execv(path + 1, NULL);
            exit(0);
        }
        close(fd[1]);
        c200(client_socket, fd[0], 1);
        close(fd[0]);
        waitpid(pid, NULL, 0);
    }
    waitpid(pid1, NULL, 0);
    return 0;
}

void send_to_client(int client_socket, char* request_path, char* ext) {
    int i = 0;
    while (addition[i].ext) {
        if (strcmp(addition[i].ext, ext) != 0) {
            i++;
        } else {
            break;
        }
    }
    if (!addition[i].ext) {
        puts("NOT FOUND");
        c404(client_socket);
        return;
    }
    int fd =  open(request_path + 1, O_RDONLY);
    puts(request_path + 1);
    if (fd < 0) {
        puts("NOT FOUND");
        c404(client_socket);
        return;
    } else {
        c200(client_socket, fd, i);
    }

    close(fd);
}

void c200(int client_socket, int fd, int index_extension) {
    char h1[] = "HTTP/1.1 200\r\ncontent-type: %s\r\ncontent-length: %d\r\n\r\n";
    char h2[] = "<!DOCTYPE html><html><h1>";
    char h3[] = "</h1></html>";
    char msg[100];
    char* buf = create_buf(fd);

    sprintf(msg, h1, addition[index_extension].mediatype, strlen(h2) + strlen(buf) + strlen(h3));
    write(client_socket, msg, strlen(msg));
    write(client_socket, h2, strlen(h2));
    write(client_socket, buf, strlen(buf));
    write(client_socket, h3, strlen(h3));
    free(buf);
}

void c404(int client_socket) {
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

char* get_request(int client_socket) {
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

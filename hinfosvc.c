#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // for HOST_NAME_MAX
#include <time.h>

#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define LINE_LEN_MAX 4096
#define HTTP_HEADER_MAX 2048
#define REQUEST_BUFSIZ 1024

#define CPU_LOAD_INTERVAL 500
#define LISTEN_BACKLOG 10

enum http_status {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_SERVER_ERR = 500,
};

const char *http_status_to_str(enum http_status status)
{
    switch (status) {
        case HTTP_OK:
            return "OK";
        case HTTP_BAD_REQUEST:
            return "Bad Request";
        case HTTP_NOT_FOUND:
            return "Not Found";
        case HTTP_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case HTTP_INTERNAL_SERVER_ERR:
            return "Internal Server Error";
        default:
            return NULL;
    }
}

int startswith(const char *prefix, const char *str)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

int send_response(int sockfd, enum http_status status, const char *body)
{
    const char *status_msg = http_status_to_str(status);

    char buff[HTTP_HEADER_MAX];
    snprintf(buff, HTTP_HEADER_MAX,
            "HTTP/1.1 %d %s\r\n"
            "Allow: GET\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %lu\r\n"
            "\r\n",
            status, status_msg, strlen(body));

    strcat(buff, body);

    if (send(sockfd, buff, strlen(buff), 0) == -1)
    {
        perror("hinfosvc: ");
        return 0;
    }
    return 1;
}

// return socket fd or -1 in case of error
// in case of error error message is printed
int init_socket(const char *port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res;
    int err = getaddrinfo(NULL, port, &hints, &res);
    if (err != 0)
    {
        fprintf(stderr, "Couldn't get address info: %s\n", gai_strerror(err));
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1)
    {
        perror("hinfosvc: ");
        freeaddrinfo(res);
        return -1;
    }

#ifdef DEBUG
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        // non-fatal error
        perror("hinfosvc setsockopt(REUSEADDR) failed: ");
#endif

    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("hinfosvc: ");
        freeaddrinfo(res);
        close(fd);
        return -1;
    }

    freeaddrinfo(res);

    if (listen(fd, LISTEN_BACKLOG) == -1)
    {
        perror("hinfosvc: ");
        close(fd);
        return -1;
    }
    return fd;
}

// return false in case of error, otherwise true
int get_cpu_name(char *dest)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f)
        return 0;

    char line[LINE_LEN_MAX];
    int found = 0;
    while (fgets(line, LINE_LEN_MAX, f) && !found)
    {
        if (startswith("model name", line))
        {
            char *name = strchr(line, ':') + 2;
            if (name)
            {
                // length is 100% at > 0
                strncpy(dest, name, strlen(name) - 1);
                found = 1;
            }
        }
    }
    fclose(f);
    return found;
}

// return cpu load or -1 in case of error
double get_cpu_load()
{
    FILE *f;
    if ((f = fopen("/proc/stat", "rb")) == NULL)
    {
        perror("hinfosvc: ");
        return -1;
    }

    int puser, pnice, psystem, pidle, piowait, pirq, psoftirq, psteal;
    fscanf(f, "cpu %d %d %d %d %d %d %d %d",
            &puser, &pnice, &psystem, &pidle,
            &piowait, &pirq, &psoftirq, &psteal);
    fclose(f);

    struct timespec ts = {
        .tv_sec = CPU_LOAD_INTERVAL / 1000,
        .tv_nsec = (CPU_LOAD_INTERVAL % 1000) * 1000000
    };
    nanosleep(&ts, NULL);

    if ((f = fopen("/proc/stat", "rb")) == NULL)
    {
        perror("hinfosvc: ");
        return -1;
    }
    int user, nice, system, idle, iowait, irq, softirq, steal;
    fscanf(f, "cpu %d %d %d %d %d %d %d %d",
            &user, &nice, &system, &idle,
            &iowait, &irq, &softirq, &steal);
    fclose(f);

    pidle = pidle + piowait;
    idle = idle + iowait;

    double pnonidle = puser + pnice + psystem + pirq + psoftirq + psteal;
    double nonidle = user + nice + system + irq + softirq + steal;

    double ptotal = pidle + pnonidle;
    double total = idle + nonidle;

    double totald = total - ptotal;
    double idled = idle - pidle;
    return (totald - idled) / totald * 100;
}

void handle_request(int sockfd, const char *method, const char *path)
{
    if (strcmp("GET", method) != 0)
    {
        send_response(sockfd, HTTP_METHOD_NOT_ALLOWED, "Method Not Allowed");
        return;
    }

    if (strcmp("/hostname", path) == 0)
    {
        char hostname[HOST_NAME_MAX + 1];
        if (gethostname(hostname, HOST_NAME_MAX + 1) == -1)
        {
            const char *message = "Failed to retrieve hostname";
            send_response(sockfd, HTTP_INTERNAL_SERVER_ERR, message);
            return;
        }
        send_response(sockfd, HTTP_OK, hostname);
    }
    else if (strcmp("/cpu-name", path) == 0)
    {
        char cpu_name[LINE_LEN_MAX];
        if (get_cpu_name(cpu_name))
        {
            send_response(sockfd, HTTP_OK, cpu_name);
        }
        else
        {
            const char *message = "Failed to retrieve CPU name";
            send_response(sockfd, HTTP_INTERNAL_SERVER_ERR, message);
        }
    }
    else if (strcmp("/load", path) == 0)
    {
        double load = get_cpu_load();
        if (load == -1.0)
        {
            const char *message = "Failed to retrieve CPU load";
            send_response(sockfd, HTTP_INTERNAL_SERVER_ERR, message);
            return;
        }
        char body[8];
        snprintf(body, 8, "%.2f%%", load);
        send_response(sockfd, HTTP_OK, body);
    }
    else
    {
        send_response(sockfd, HTTP_NOT_FOUND, "404 Not Found");
    }
}

static int welcome_fd = -1;
void handle_sigint(int signum)
{
    (void) signum;
    if (welcome_fd != -1)
        close(welcome_fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Expected one argument\n");
        return EXIT_FAILURE;
    }

    char *rest;
    int port = strtol(argv[1], &rest, 10);
    if (*rest != '\0')
    {
        fprintf(stderr, "Port must be an integer\n");
        return EXIT_FAILURE;
    }

    if (port < 0 || port > 65535)
    {
        fprintf(stderr, "Invalid port number: %d", port);
        return EXIT_FAILURE;
    }

    welcome_fd = init_socket(argv[1]);
    if (welcome_fd == -1)
    {
        // message is printed inside function
        return EXIT_FAILURE;
    }

    struct sigaction act;
    act.sa_handler = handle_sigint,
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    while (1)
    {
        // we do not care about who is the client so pass in NULLs
        int con_fd = accept(welcome_fd, NULL, NULL);
        if (con_fd == -1)
        {
            perror("hinfosvc: ");
            // Non-fatal error, we can continue receiving requests
            continue;
        }

        char buff[REQUEST_BUFSIZ] = { 0 };
        if (recv(con_fd, buff, sizeof(buff), 0) == -1)
        {
            perror("hinfosvc: ");
            // Non-fatal error, we can continue receiving requests
            continue;
        }

        char *method = strtok(buff, " ");
        fprintf(stderr, "method: %s\n", method);
        char *path = strtok(NULL, " ");
        fprintf(stderr, "path: %s\n", path);
        char *version = strtok(NULL, " \n");
        fprintf(stderr, "version: %s\n", version);

        if (!method || !path || !version)
            send_response(con_fd, HTTP_BAD_REQUEST, "Bad Request");
        else
            handle_request(con_fd, method, path);

        close(con_fd);
    }

    // practicaly unreachable
    close(welcome_fd);
    return EXIT_SUCCESS;
}

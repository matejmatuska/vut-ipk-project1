#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <limits.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#define LINE_LEN_MAX 4096
#define HTTP_HEADER_MAX 2048

#define HTTP_OK 200
#define HTTP_NOT_FOUND 404

#define LOAD_INTERVAL 500

int startswith(const char *prefix, const char *str)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

int send_response(int sockfd, unsigned status, const char *status_msg, const char *body)
{
    char buff[HTTP_HEADER_MAX];
    snprintf(buff, HTTP_HEADER_MAX,
            "HTTP/1.1 %u %s\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %lu\r\n"
            "\r\n"
            "%s",
            status, status_msg, strlen(body), body);

    if (send(sockfd, buff, HTTP_HEADER_MAX, 0) == -1)
    {
        perror("hinfosvc: ");
        return 0;
    }
    return 1;
}

// return socket fd or -1 in case of error
int init_socket(char *port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res;
    int ecode = getaddrinfo(NULL, port, &hints, &res) != 0;
    if (ecode != 0)
    {
        fprintf(stderr, "Couldn't get address info: %s\n", gai_strerror(ecode));
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        perror("hinfosvc: ");
        freeaddrinfo(res);
        return -1;
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("hinfosvc: ");
        freeaddrinfo(res);
        close(sockfd);
        return -1;
    }

    freeaddrinfo(res);

    if (listen(sockfd, 10) == -1)
    {
        perror("hinfosvc: ");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

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

    struct timespec ts = { 
        .tv_sec = LOAD_INTERVAL / 1000,
        .tv_nsec = (LOAD_INTERVAL % 1000) * 1000000
    };
    nanosleep(&ts, NULL);
    fclose(f);

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
    return (totald - idled)/totald * 100;
}

// error codes:
// 1 if 
int handle_request(int sockfd, char *method, char *path)
{
    if (strcmp("GET", method) != 0)
    {
        send_response(sockfd, HTTP_NOT_FOUND, "Not Found", "404 Not Found");
        close(sockfd);
    }

    if (strcmp("/hostname", path) == 0)
    {
        char hostname[HOST_NAME_MAX + 1];
        if (gethostname(hostname, HOST_NAME_MAX + 1) == -1)
        {
            perror("hinfosvc: ");
            // TODO what to do
        }
        send_response(sockfd, HTTP_OK, "OK", hostname);
    }
    else if (strcmp("/cpu-name", path) == 0)
    {
        FILE *f = fopen("/proc/cpuinfo", "r");
        if (!f)
        {
            perror("hinfosvvc: ");
            close(sockfd);
            return 1;
        }

        char line[LINE_LEN_MAX];
        char *cpu_name = "Failed to retrieve CPU name";
        while (fgets(line, LINE_LEN_MAX, f))
        {
            if (startswith("model name", line))
            {
                cpu_name = strchr(line, ':') + 1;
                break;
            }
        }
        fclose(f);
        send_response(sockfd, HTTP_OK, "OK", cpu_name);
    }
    else if (strcmp("/load", path) == 0)
    {
        double load = get_cpu_load();
        char body[5];
        snprintf(body, 5, "%d%%", (int) load);
        send_response(sockfd, HTTP_OK, "OK", body);
    }
    else
    {
        send_response(sockfd, HTTP_NOT_FOUND, "Not Found", "404 Not Found");
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Expected one argument\n");
        return EXIT_FAILURE;
    }

    int sockfd = init_socket(argv[1]);
    if (sockfd == -1)
        return EXIT_FAILURE;

    while (1)
    {
        // we do not care about who is the client so pass in NULLs
        int fd = accept(sockfd, NULL, NULL); 
        if (fd == -1)
        {
            perror("hinfosvc: ");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        char buff[HTTP_HEADER_MAX];
        if (recv(fd, buff, sizeof(buff), 0) == -1)
        {
            perror("hinfosvc: ");
            // Non-fatal error, we can continue receiving requests
        }

        char *method = strtok(buff, " ");
        char *path = strtok(NULL, " ");

        handle_request(fd, method, path);
        close(fd);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}



#include "myunp.h"

void err_quit(const char *format, ...)
{
    va_list ap;
    char    buffer[200];
    int     prefix_length;

    va_start(ap, format);

    strcpy(buffer, "err_quit: ");
    prefix_length = strlen(buffer);

    vsnprintf(buffer + prefix_length,
              sizeof buffer - prefix_length,
              format,
              ap);

    va_end(ap);

    printf("%s\n", buffer);
    exit(1);
}

void err_sys(const char *s, ...)
{
    printf("err_sys: %s\n", s); exit(1);
}

void bzero(void *p, size_t len) { memset(p, 0, len); }

int Socket(int family, int type, int protocol)
{
    int n;

    if ( (n = socket(family, type, protocol)) < 0)
    {
        err_sys("socket error");
    }

    return n;
}

int Bind(int sockfd, const struct sockaddr *myaddr, socklen_t addrlen)
{
    int n;

    if ( (n = bind(sockfd, myaddr, addrlen)) < 0)
    {
        err_sys("bind error");
    }

    return n;
}

void Listen(int sockfd, int backlog)
{
    char *ptr;

    if ( (ptr = getenv("LISTENQ")) != NULL)
    {
        backlog = atoi(ptr);
    }

    if (listen(sockfd, backlog) < 0)
    {
        err_sys("listen error");
    }
}

int Accept(int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
    int n;

    if ( (n = accept(sockfd, cliaddr, addrlen)) < 0)
    {
        err_sys("accept error");
    }

    return n;
}

int Close(int sockfd)
{
    int n;

    if ( (n = close(sockfd)) < 0)
    {
        err_sys("close error");
    }

    return n;
}

int Read(int sockfd, char *buffer, int bufferlen)
{
    int n;

    if ( (n = read(sockfd, buffer, bufferlen)) < 0)
    {
        err_sys("read error");
    }

    return n;
}

int Write(int sockfd, char *buffer, int bufferlen)
{
    int n;

    if ( (n = write(sockfd, buffer, bufferlen)) < 0)
    {
        err_sys("write error");
    }

    return n;
}

/*
 * Sean Middleditch
 * sean@sourcemud.org
 *
 * The author or authors of this code dedicate any and all copyright interest
 * in this code to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and successors. We
 * intend this dedication to be an overt act of relinquishment in perpetuity of
 * all present and future rights to this code under copyright law.
 */

#if !defined(_POSIX_SOURCE)
#       define _POSIX_SOURCE
#endif
#if !defined(_BSD_SOURCE)
#       define _BSD_SOURCE
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>

#ifdef HAVE_ZLIB
#include "zlib.h"
#endif

#include "libtelnet.h"

static struct termios orig_tios;
static telnet_t *telnet;
static int do_echo;
static int tSock;

static const telnet_telopt_t telopts[] = {
    { TELNET_TELOPT_ECHO,	   TELNET_WONT, TELNET_DO	},
    { TELNET_TELOPT_TTYPE,	   TELNET_WILL, TELNET_DONT },
    { TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DO	},
    { TELNET_TELOPT_MSSP,	   TELNET_WONT, TELNET_DO	},
    { -1,					   0,			0			}
};
static void _cleanup(void)
{
    tcsetattr(STDOUT_FILENO, TCSADRAIN, &orig_tios);
}


static void _input(char *buffer, int size)
{
    static char crlf[] = { '\r', '\n' };
    int i;

    for (i = 0; i != size; ++i)
    {
        /* if we got a CR or LF, replace with CRLF
         * NOTE that usually you'd get a CR in UNIX, but in raw
         * mode we get LF instead (not sure why)
         */
        if (buffer[i] == '\r' || buffer[i] == '\n')
        {
            if (do_echo)
                printf("\r\n");

            telnet_send(telnet, crlf, 2);
        }
        else
        {
            if (do_echo)
                putchar(buffer[i]);

            telnet_send(telnet, buffer + i, 1);
        }
    }

    fflush(stdout);
}


static int _send(int sock, const char *buffer, size_t size)
{
    int rs;

    /* send data */
    while (size > 0)
    {
        if ((rs = send(sock, buffer, size, 0)) == -1)
        {
            fprintf(stderr, "send() failed: %s\n", strerror(errno));
            return rs;
        }
        else if (rs == 0)
        {
            fprintf(stderr, "send() unexpectedly returned 0\n");
            return rs;
        }

        /* update pointer and size to see if we've got more to send */
        buffer += rs;
        size -= rs;
    }
    return size;
}


int send_telnet(const char *buffer, size_t size)
{
    int rv;
    rv= _send(tSock, buffer, size);
    return rv;
}


static void _event_handler(telnet_t *telnet, telnet_event_t *ev,
                           void *user_data)
{
    int sock = *(int *)user_data;

    switch (ev->type)
    {
    /* data received */
    case TELNET_EV_DATA:
        ////printf("%.*s", (int)ev->data.size, ev->data.buffer);
        ////fflush(stdout);
        break;

    /* data must be sent */
    case TELNET_EV_SEND:
        _send(sock, ev->data.buffer, ev->data.size);
        break;

    /* request to enable remote feature (or receipt) */
    case TELNET_EV_WILL:

        /* we'll agree to turn off our echo if server wants us to stop */
        if (ev->neg.telopt == TELNET_TELOPT_ECHO)
            do_echo = 0;

        break;

    /* notification of disabling remote feature (or receipt) */
    case TELNET_EV_WONT:

        if (ev->neg.telopt == TELNET_TELOPT_ECHO)
            do_echo = 1;

        break;

    /* request to enable local feature (or receipt) */
    case TELNET_EV_DO:
        break;

    /* demand to disable local feature (or receipt) */
    case TELNET_EV_DONT:
        break;

    /* respond to TTYPE commands */
    case TELNET_EV_TTYPE:

        /* respond with our terminal type, if requested */
        if (ev->ttype.cmd == TELNET_TTYPE_SEND)
            telnet_ttype_is(telnet, getenv("TERM"));

        break;

    /* respond to particular subnegotiations */
    case TELNET_EV_SUBNEGOTIATION:
        break;

    /* error */
    case TELNET_EV_ERROR:
        fprintf(stderr, "ERROR: %s\n", ev->error.msg);
        exit("telnet error");

    default:
        /* ignore */
        break;
    }
}


int init_telnet(char *hostname, unsigned port)
{
    int rs;
    struct sockaddr_in addr;
    struct addrinfo *ai;
    struct addrinfo hints;
    struct termios tios;

    char sPort[100];

    sprintf(sPort, "%d", port);

    /* look up server host */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rs = getaddrinfo(hostname, sPort, &hints, &ai)) != 0)
    {
        fprintf(stderr, "getaddrinfo() failed for %s: %s\n", hostname,
                gai_strerror(rs));
        return 1;
    }

    /* create server socket */
    if ((tSock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
        return 1;
    }

    /* bind server socket */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_LOOPBACK;

#if 0

    if (bind(tSock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        fprintf(stderr, "bind() failed: %s\n", strerror(errno));
        return 1;
    }

#endif

    /* connect */
    if (connect(tSock, ai->ai_addr, ai->ai_addrlen) == -1)
    {
        fprintf(stderr, "connect() failed: %s\n", strerror(errno));
        return 1;
    }

    /* free address lookup info */
    freeaddrinfo(ai);

#if 0
    /* get current terminal settings, set raw mode, make sure we
     * register atexit handler to restore terminal settings
     */
    tcgetattr(STDOUT_FILENO, &orig_tios);
    atexit(_cleanup);
    tios = orig_tios;
    cfmakeraw(&tios);
    tcsetattr(STDOUT_FILENO, TCSADRAIN, &tios);

    /* set input echoing on by default */
    do_echo = 1;
#endif

    /* initialize telnet box */
    telnet = telnet_init(telopts, _event_handler, 0, &tSock);
}


char *recieve_telnet(void)
{
    int rs;
    struct pollfd pfd[1];
    static char buffer[512];

    memset(pfd, 0, sizeof(pfd));
    pfd[0].fd = tSock;
    pfd[0].events = POLLIN;

    int timeout = 2000;
    int rv;

    rv = poll(pfd, 1 /*2*/, timeout);

    if (rv == 0)
    {
        fprintf(stderr, "response timeout\n");
        return NULL;
    }

    /* read from server */
    if (pfd[0].revents & POLLIN)
    {
        if ((rs = recv(tSock, buffer, sizeof(buffer), 0)) > 0)
        {
            telnet_recv(telnet, buffer, rs);
        }
        else if (rs == 0)
        {
            exit("telnet error");                                                                                                             // break;
        }
        else
        {
            fprintf(stderr, "recv(client) failed: %s\n",
                    strerror(errno));
            exit("telnet error");
        }
    }

    return buffer;
}


void close_telnet(void)
{
    /* clean up */
    telnet_free(telnet);
    close(tSock);
}

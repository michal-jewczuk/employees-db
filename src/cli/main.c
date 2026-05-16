#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"

int send_hello(int fd) {
    char buf[4096] = {0};

    dbproto_hdr_t *hdr = buf;
    hdr->type = MSG_HELLO_REQ;
    hdr->len = 1;
    dbproto_hello_req *hello = (dbproto_hello_req*)&hdr[1];
    hello->proto = PROTO_VER;

    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);
    hello->proto = htons(hello->proto);

    write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_req));

    read(fd, buf, sizeof(buf));
    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);

    if (hdr->type == MSG_ERROR) {
        printf("Protocol mismatch\n");
        close(fd);
        return STATUS_ERROR;
    }


    printf("Server connected. Protocol v1.\n");
    return STATUS_SUCCESS;
}

int main(int argc, char *argv[]) {
    char *addarg = NULL;
    char *portarg = NULL;
    char *hostarg = NULL;
    unsigned short port = 0;

    int c;
    while ((c = getopt(argc, argv, "p:h:a:")) != -1) {
        switch (c) {
            case 'a':
                addarg = optarg;
                break;
            case 'p':
                portarg = optarg;
                port = atoi(portarg);
                break;
            case 'h':
                hostarg = optarg;
                break;
            case '?':
                printf("Unknown option -%c\n", c);
            default:
                return -1;
        }
    }


    if (port == 0) {
        printf("Bad port: %s\n", port);
        return STATUS_ERROR;
    }
    if (hostarg == NULL) {
        printf("Must specify host with -h\n");
        return STATUS_ERROR;
    }

	struct sockaddr_in serverInfo = {0};
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_addr.s_addr = inet_addr(hostarg);
	serverInfo.sin_port = htons(port);

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		return -1;
	}

	if (connect(fd, (struct sockaddr*)&serverInfo, sizeof(serverInfo)) == -1) {
		perror("connet");
		close(fd);
		return -1;
	}

	if (send_hello(fd) != STATUS_SUCCESS) {
        close(fd);
        return STATUS_ERROR;
    }

	close(fd);
}


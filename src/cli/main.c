#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
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

int send_list_req(int fd) {
    char buf[4096] = {0};

    dbproto_hdr_t *hdr = buf;
    hdr->type = MSG_EMPLOYEE_LIST_REQ;
    hdr->len = 0;

    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);

    write(fd, buf, sizeof(dbproto_hdr_t));

    read(fd, buf, sizeof(buf));
    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);

    if (hdr->type == MSG_ERROR) {
        printf("List request resulted in error\n");
        close(fd);
        return STATUS_ERROR;
    }

    if (hdr->type == MSG_EMPLOYEE_LIST_RESP) {
        printf("Listing employees [%d]:\n", hdr->len);
        dbproto_list_resp *employee = (dbproto_list_resp*)&hdr[1];
        int i = 0;
        for (; i < hdr->len; i++) {
            read(fd, employee, sizeof(dbproto_list_resp)); 
            employee->hours = ntohl(employee->hours);
            printf("[%d] %s %s %d\n", (i + 1), employee->name, employee->address, employee->hours);
        } 
    }

    return STATUS_SUCCESS;
}

int send_create(int fd, char* emplstr) {
    char buf[4096] = {0};
    dbproto_hdr_t *hdr = buf;
    hdr->type = MSG_EMPLOYEE_ADD_REQ;
    hdr->len = 1;

    dbproto_add_req *add = (dbproto_add_req*)&hdr[1];
    printf("Before copy\n");
    strncpy(&add->data, emplstr, sizeof(add->data));

    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);

    printf("Sending data of size: %ld | %ld\n", sizeof(emplstr), sizeof(dbproto_add_req));
    write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_add_req));

    read(fd, buf, sizeof(buf));
    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);

    if (hdr->type != MSG_EMPLOYEE_ADD_RESP) {
        printf("Error adding employee\n");
        close(fd);
        return STATUS_ERROR;
    }


    printf("Employee added\n");
    return STATUS_SUCCESS;
}

int main(int argc, char *argv[]) {
    char *addarg = NULL;
    char *portarg = NULL;
    char *hostarg = NULL;
    bool list = false;
    unsigned short port = 0;

    int c;
    while ((c = getopt(argc, argv, "p:h:a:l")) != -1) {
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
            case 'l':
                list = true;
                break;
            case '?':
                printf("Unknown option -%c\n", c);
            default:
                return -1;
        }
    }


    if (port == 0) {
        printf("Bad port: %d\n", port);
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
		perror("connect");
		close(fd);
		return -1;
	}

	if (send_hello(fd) != STATUS_SUCCESS) {
        return STATUS_ERROR;
    }

    if (addarg != NULL) {
        if (send_create(fd, addarg) != STATUS_SUCCESS) {
            return STATUS_ERROR;
        }
    }

    if (list) {
        send_list_req(fd);
        // TODO handle error or list
    }

	close(fd);
}


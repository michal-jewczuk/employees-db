#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080

#include "common.h"

// how to send prot version and req in one write without changing struct?
void send_hello(int fd) {
    char buf[4096] = {0};
    char hel[4096] = {0};

    dbproto_hdr_t *hdr = buf;
    hdr->type = MSG_HELLO_REQ;
    hdr->len = 1;

    hdr->type = htonl(hdr->type);
    hdr->len = htons(hdr->len);

    write(fd, buf, sizeof(dbproto_hdr_t));
    printf("Server connected.\n");

    dbproto_hello_req *hello = hel;
    hello->proto = 100;
    hello->proto = htons(hello->proto);
    write(fd, hel, sizeof(dbproto_hello_req));
    printf("Protocol version sent.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <ip of the host>\n", argv[0]);
        return 0;
    }
	struct sockaddr_in serverInfo = {0};

	serverInfo.sin_family = AF_INET;
	serverInfo.sin_addr.s_addr = inet_addr(argv[1]);
	serverInfo.sin_port = htons(PORT);

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
	send_hello(fd);

	close(fd);
}


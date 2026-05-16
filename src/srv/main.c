#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <poll.h>

#include "common.h"
#include "file.h"
#include "parse.h"
#include "srvpoll.h"

clientstate_t clientStates[MAX_CLIENTS] = {0};

void print_usage(char *argv[]) {
	printf("Usage: %s -n -f <database file>\n", argv[0]);
	printf("\t -n create new database file\n");
	printf("\t -f (required) path to database file\n");
	printf("\t -p (required) port to listen to\n");

	return;
}

void fsm_reply_hello_err(clientstate_t *client, dbproto_hdr_t* hdr) {
    hdr->type = htonl(MSG_ERROR);
    hdr->len = htons(0);
    write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void fsm_reply_hello(clientstate_t *client, dbproto_hdr_t* hdr) {
    hdr->type = htonl(MSG_HELLO_RESP);
    hdr->len = htons(0);
    write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t *employees, clientstate_t *client) {
    dbproto_hdr_t *hdr = (dbproto_hdr_t*)client->buffer;

    hdr->type = ntohl(hdr->type);
    hdr->len = ntohs(hdr->len);


    if (client->state == STATE_HELLO) {
        printf("client in hello\n");
        if (hdr->type != MSG_HELLO_REQ || hdr->len != 1) {
            printf("Did not get MSG_HELLO in HELLO state\n");
            // TODO send err msg
        }
        dbproto_hello_req* hello = (dbproto_hello_req*)&hdr[1];
        hello->proto = ntohs(hello->proto);
        if (hello->proto != PROTO_VER) {
            printf("Protocol mismatch; expected %d, got %d\n", PROTO_VER, hello->proto);
            fsm_reply_hello_err(client, hdr);
            return;
        }

        fsm_reply_hello(client, hdr);
        client->state = STATE_MSG;
        printf("Client upgraded to STATE_MSG\n");
    }

    if (client->state == STATE_MSG) {
        printf("client in msg\n");
    }
}

int poll_loop(unsigned short port, struct dbheader_t *dbhdr, struct employee_t *employees) {
    int listen_fd, conn_fd, freeSlot;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;
    int opt = 1;

	init_clients(&clientStates);	

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    } 

    if (listen(listen_fd, 10) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d\n", port);

    memset(fds, 0, sizeof(fds));
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    nfds = 1;

    while (1) {
        int ii = 1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clientStates[i].fd != -1) {
                fds[ii].fd = clientStates[i].fd;
                fds[ii].events = POLLIN;
                ii++;
            }
        }

        int n_events = poll(fds, nfds, -1); // -1 no timeout
        if (n_events == -1) {
            perror("poll");
            return EXIT_FAILURE;
        }

        if (fds[0].revents && POLLIN) {
            if ((conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len)) == -1) {
                perror("accept");
                continue;
            }
            printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            freeSlot = find_free_slot(&clientStates);
            if (freeSlot == -1) {
                printf("Server full: closing new connection\n");
                close(conn_fd);
            } else {
                printf("Free slot at: %d\n", freeSlot);
                clientStates[freeSlot].fd = conn_fd;
                //clientStates[freeSlot].state = STATE_CONNECTED;
                clientStates[freeSlot].state = STATE_HELLO;
                nfds++;
                printf("Slot %d has fd %d\n", freeSlot, clientStates[freeSlot].fd);
            }
            n_events--;
        }

        // check each client for read/write activity
        for (int i = 1; i <= nfds && n_events > 0; i++) {
            if (fds[i].revents & POLLIN) {
                n_events--;

                int fd = fds[i].fd;
                int slot = find_slot_by_fd(fd, &clientStates);
                ssize_t bytes_read = read(fd, &clientStates[slot].buffer, sizeof(clientStates[slot].buffer));
                if (bytes_read <= 0) {
                    close(fd);
                    if (slot == -1) {
                        printf("Tried to close fd that does not exist?\n");
                    } else {
                        clientStates[slot].fd = -1;
                        clientStates[slot].state = STATE_DISCONNECTED;
                        printf("Client disconnected\n");
                        nfds--;
                    }
                } else {
                    printf("got bytes: %ld\n", bytes_read);
                    handle_client_fsm(dbhdr, employees, &clientStates[slot]);
                }
            }
        }
    }
	return 0;
}

int main(int argc, char *argv[]) {
	bool newfile = false;
	char *filepath = NULL;
	char *portstring = NULL;
    unsigned short port = 0;
	int c;
	int dbfd = -1;
	struct dbheader_t *dbheader = NULL;
	struct employee_t *employees = NULL;

	while((c = getopt(argc, argv, "nf:p:")) != -1) {
		switch(c) {
			case 'n':
				newfile = true;
				break;
			case 'f':
				filepath = optarg;
				break;
			case 'p':
				portstring = optarg;
                port = atoi(portstring);
                if (port == 0) {
                    printf("Bad port: %s\n", portstring);
                }
				break;
			case '?':
				printf("Unknown option -%c\n",  c);
				break;
			default:
				return -1;
		}
	}

	if (filepath == NULL) {
		printf("Filepath is a required argument!\n");
		print_usage(argv);
		return 0;
	}

    if (port == 0) {
        printf("POrt not set\n");
        print_usage(argv);
        return 0;
    }

	if (newfile) {
		dbfd = create_db_file(filepath);
		if (dbfd == STATUS_ERROR) {
			printf("Unable to create db file\n");
			return -1;
		}
		if(create_db_header(&dbheader) == STATUS_ERROR) {
			printf("Failed to create db header\n");
			return -1;
		}
	} else {
		dbfd = open_db_file(filepath);
		if (dbfd == STATUS_ERROR) {
			printf("Unable to open db file\n");
			return -1;
		}
		if (validate_db_header(dbfd, &dbheader) == STATUS_ERROR) {
			printf("Invalid db header!\n");
			return -1;
		}
	}

    poll_loop(port, dbheader, employees);

	free(dbheader);
	free(employees);
	close(dbfd);
	return 0;
}


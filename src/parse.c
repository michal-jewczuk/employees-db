#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "parse.h"
#include "common.h"

int create_db_header(int fd, struct dbheader_t **headerOut) {
	struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t)); 
	if (header == NULL) {
		printf("Malloc failed to create db header\n");
		return STATUS_ERROR;
	}

	header->magic = HEADER_MAGIC;
	header->version = 0x1;
	header->count = 0;
	header->filesize = sizeof(struct dbheader_t);
	*headerOut = header;

	return STATUS_SUCCESS;
}

int validate_db_header(int fd, struct dbheader_t **headerOut) {
	if (fd < 0) {
		printf("Called with invalid FD\n");
		return STATUS_ERROR;
	}

	struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t)); 
	if (header == NULL) {
		printf("Malloc failed to create db header\n");
		return STATUS_ERROR;
	}

	if(read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
		perror("read");
		free(header);
		return STATUS_ERROR;
	}

	header->magic = ntohl(header->magic);
	header->version = ntohs(header->version);
	header->count = ntohs(header->count);
	header->filesize = ntohl(header->filesize);

	if (header->version != 1) {
		printf("Invalid db header version\n");
		free(header);
		return STATUS_ERROR;
	}

	if (header->magic != HEADER_MAGIC) {
		printf("Invalid db header file format\n");
		free(header);
		return STATUS_ERROR;
	}

	struct stat dbstat = {0};
	int fs = fstat(fd, &dbstat);
	if (fs == -1 || dbstat.st_size != header->filesize) {
		printf("Corrupted database\n");
		free(header);
		return STATUS_ERROR;
	}

	*headerOut = header;

	return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *header, struct employee_t *employee) {
	if (fd < 0) {
		printf("Called with invalid FD\n");
		return STATUS_ERROR;
	}

	header->magic = htonl(header->magic);
	header->version = htons(header->version);
	header->count = htons(header->count);
	header->filesize = htonl(header->filesize);

	lseek(fd, 0, SEEK_SET);
	if(write(fd, header, sizeof(struct dbheader_t)) == -1) {
		perror("write");
		return STATUS_ERROR;
	}

	return STATUS_SUCCESS;
}


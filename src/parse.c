#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "parse.h"
#include "common.h"

int create_db_header(struct dbheader_t **headerOut) {
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

int output_file(int fd, struct dbheader_t *header, struct employee_t *employees) {
	if (fd < 0) {
		printf("Called with invalid FD\n");
		return STATUS_ERROR;
	}

	int emp_count = header->count;

	header->magic = htonl(header->magic);
	header->version = htons(header->version);
	header->count = htons(header->count);
	header->filesize = htonl(header->filesize);

	lseek(fd, 0, SEEK_SET);
	if(write(fd, header, sizeof(struct dbheader_t)) == -1) {
		perror("write");
		return STATUS_ERROR;
	}

	for (int i = 0; i < emp_count; i++) {
		employees[i].hours = htons(employees[i].hours);
		if(write(fd, &employees[i], sizeof(struct employee_t)) == -1) {
			perror("write");
			return STATUS_ERROR;
		}
	}

	return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *header, struct employee_t **employeesOut) {
	if (fd < 0) {
		printf("Called with invalid FD\n");
		return STATUS_ERROR;
	}
	
	int count = header->count;
	struct employee_t *employees = calloc(count, sizeof(struct employee_t));
	if (employees == NULL) {
		printf("Error allocating memory for employees\n");
		return STATUS_ERROR;
	}

	if (read(fd, employees, count * sizeof(struct employee_t)) != count * sizeof(struct employee_t)) {
		perror("read");
		free(employees);
		return STATUS_ERROR;
	}

	for (int i = 0; i < count; i++) {
		employees[i].hours = htonl(employees[i].hours);
	}

	*employeesOut = employees;
	return STATUS_SUCCESS;
}

int add_employee(struct dbheader_t *header, struct employee_t **employees, char *addstring) {
	if (header == NULL || employees == NULL || *employees == NULL || addstring == NULL) {
		printf("Method got invalid argument\n");
		return STATUS_ERROR;
	}

	char *name = strtok(addstring, ",");
	if (name == NULL) {
		printf("Error parsing employee from input\n");
		return STATUS_ERROR;
	}

	char *address = strtok(NULL, ",");
	if (address == NULL) {
		printf("Error parsing employee from input\n");
		return STATUS_ERROR;
	}

	char *hours = strtok(NULL, ",");
	if (hours == NULL) {
		printf("Error parsing employee from input\n");
		return STATUS_ERROR;
	}

	struct employee_t *e = *employees;
	e = realloc(e, header->filesize);
	if (e == NULL) {
		printf("Error allocating memory for new employee\n");
		return STATUS_ERROR;
	}

	strncpy(e[header->count].name, name, NAME_S - 1);
	strncpy(e[header->count].address, address, ADDR_S - 1);
	e[header->count].hours = atoi(hours);

	header->count++;
	header->filesize = header->filesize + sizeof(struct employee_t);

	*employees = e;
	return STATUS_SUCCESS;
}

void list_employees(struct dbheader_t *header, struct employee_t *employees) {
	if (header == NULL || employees == NULL) {
		return;
	}

	for (int i = 0; i < header->count; i++) {
		printf("[%d] %s,%s,%d\n", i + 1, employees[i].name, employees[i].address, employees[i].hours);
	}
}


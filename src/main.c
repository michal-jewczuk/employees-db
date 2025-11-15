#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
	printf("Usage: %s -n -f <database file>\n", argv[0]);
	printf("\t -n create new database file\n");
	printf("\t -f (required) path to database file\n");
	printf("\t -l list all emloyees\n");
	printf("\t -a <name,address,hours> add new user to database\n");

	return;
}

int main(int argc, char *argv[]) {
	bool newfile = false;
	bool listemployees = false;
	char *filepath = NULL;
	char *addstring = NULL;
	int c;
	int dbfd = -1;
	struct dbheader_t *dbheader = NULL;
	struct employee_t *employees = NULL;

	while((c = getopt(argc, argv, "nf:a:l")) != -1) {
		switch(c) {
			case 'n':
				newfile = true;
				break;
			case 'f':
				filepath = optarg;
				break;
			case 'a':
				addstring = optarg;
				break;
			case 'l':
				listemployees = true;
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

	if (read_employees(dbfd, dbheader, &employees) == STATUS_ERROR) {
		printf("Error reading employees\n");
		close(dbfd);
		return -1;
	}

	if (addstring) {
		add_employee(dbheader, &employees, addstring);
	}

	if (listemployees) {
		list_employees(dbheader, employees);
	}

	if (output_file(dbfd, dbheader, employees) == STATUS_ERROR) {
		printf("Could not write db file\n");
		close(dbfd);
		return -1;
	}	

	free(dbheader);
	free(employees);
	close(dbfd);
	return 0;
}


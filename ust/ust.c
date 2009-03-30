#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "ustcomm.h"

void parse_opts(int argc, char **argv)
{
	int flags, opt;
	int nsecs, tfnd;

	nsecs = 0;
	tfnd = 0;
	flags = 0;
	while ((opt = getopt(argc, argv, "nt:")) != -1) {
		switch (opt) {
		case 'n':
			flags = 1;
			break;
		case 't':
			nsecs = atoi(optarg);
			tfnd = 1;
			break;
		default:	/* '?' */
			fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n",
				argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	printf("flags=%d; tfnd=%d; optind=%d\n", flags, tfnd, optind);

	if (optind >= argc) {
		fprintf(stderr, "Expected argument after options\n");
		exit(EXIT_FAILURE);
	}

	printf("name argument = %s\n", argv[optind]);

	/* Other code omitted */

	exit(EXIT_SUCCESS);

}

int main(int argc, char *argv[])
{
	pid_t pid = atoi(argv[1]);

	char *msg = argv[2];

	struct ustcomm_connection conn;

	ustcomm_connect_app(pid, &conn);
	ustcomm_send_request(&conn, msg, NULL);

	return 0;
}

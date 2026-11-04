// Command-line helper for exercising the ASGN1 ioctl commands.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "asgn1_ioctl.h"

/*
 * Purpose: Prints supported helper commands.
 * Parameters: program is argv[0].
 * Returns: none.
 */
static void usage(const char *program)
{
	fprintf(stderr, "Usage: %s <stats|version|reset|mode 0|1> [device]\n",
		program);
}

/*
 * Purpose: Runs one ioctl request and prints returned data or an error.
 * Parameters: argc and argv are the process command-line arguments.
 * Returns: EXIT_SUCCESS on a completed operation, otherwise EXIT_FAILURE.
 */
int main(int argc, char **argv)
{
	const char *device = argc >= 3 && strcmp(argv[1], "mode") != 0
				 ? argv[2] : (argc >= 4 ? argv[3] : "/dev/asgn1");
	int fd;
	int result = EXIT_SUCCESS;

	if (argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "stats") == 0 && argc == 3) {
		struct asgn1_stats stats;

		if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
			perror("ASGN1_GET_STATS");
			result = EXIT_FAILURE;
		} else {
			printf("open_count=%d\nwrite_count=%d\nread_count=%d\n"
			       "buffer_len=%d\nlast_write_size=%d\n",
			       stats.open_count, stats.write_count, stats.read_count,
			       stats.buffer_len, stats.last_write_size);
		}
	} else if (strcmp(argv[1], "version") == 0 && argc == 3) {
		char version[ASGN1_VERSION_SIZE] = { 0 };

		if (ioctl(fd, ASGN1_GET_VERSION, version) < 0) {
			perror("ASGN1_GET_VERSION");
			result = EXIT_FAILURE;
		} else {
			printf("version=%s\n", version);
		}
	} else if (strcmp(argv[1], "reset") == 0 && argc == 3) {
		if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
			perror("ASGN1_RESET_BUFFER");
			result = EXIT_FAILURE;
		}
	} else if (strcmp(argv[1], "mode") == 0 && argc == 4) {
		char *end;
		long mode = strtol(argv[2], &end, 10);
		int mode_value = (int)mode;

		if (*end != '\0' || (mode != 0 && mode != 1)) {
			fprintf(stderr, "mode must be 0 or 1\n");
			result = EXIT_FAILURE;
		} else if (ioctl(fd, ASGN1_SET_MODE, &mode_value) < 0) {
			perror("ASGN1_SET_MODE");
			result = EXIT_FAILURE;
		}
	} else if (strcmp(argv[1], "badcmd") == 0 && argc == 3) {
		errno = 0;
		if (ioctl(fd, _IO(ASGN1_IOC_MAGIC, 99)) != -1 || errno != ENOTTY) {
			fprintf(stderr, "unknown ioctl did not return ENOTTY\n");
			result = EXIT_FAILURE;
		} else {
			puts("unknown ioctl: ENOTTY");
		}
	} else {
		usage(argv[0]);
		result = EXIT_FAILURE;
	}

	if (close(fd) < 0) {
		perror("close");
		result = EXIT_FAILURE;
	}
	return result;
}

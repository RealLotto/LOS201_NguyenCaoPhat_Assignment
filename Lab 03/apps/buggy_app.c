#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void process_data(const int *data, int len)
{
    int sum = 0;
    for (int i = 0; i <= len; ++i) {
        sum += data[i];
    }
    printf("sum=%d\n", sum);
}

static void dereference_null(void)
{
    volatile int *value = NULL;
    puts("Triggering null pointer dereference");
    *value = 1;
}

static char *create_message(const char *text)
{
    char *buffer = malloc(strlen(text) + 1);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(buffer, text);
    free(buffer);
    return buffer;
}

static int calculate(int a, int b)
{
    volatile int result = a * b;
    printf("calculate(%d, %d)=%d (INT_MAX=%d)\n", a, b, result, INT_MAX);
    return result;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s --case {null|oob|uaf|overflow}\n"
            "Each case is isolated for repeatable GDB debugging.\n",
            program);
}

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[2], "null") == 0) {
        dereference_null();
    } else if (strcmp(argv[2], "oob") == 0) {
        const int values[] = {10, 20, 30, 40, 50};
        process_data(values, 5);
    } else if (strcmp(argv[2], "uaf") == 0) {
        char *message = create_message("Hello Embedded Linux");
        printf("message=%s\n", message);
    } else if (strcmp(argv[2], "overflow") == 0) {
        (void)calculate(100000, 100000);
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

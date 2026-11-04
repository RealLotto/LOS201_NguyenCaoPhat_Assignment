#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_RECORDS 100
#define MSG_SIZE 256

typedef struct {
    int id;
    char *name;
    int *scores;
} Record;

static Record *create_record(int id, const char *name, int num_scores)
{
    Record *record = malloc(sizeof(*record));
    if (record == NULL)
        return NULL;

    record->id = id;
    record->name = malloc(strlen(name) + 1);
    record->scores = malloc(sizeof(*record->scores) * (size_t)num_scores);
    if (record->name == NULL || record->scores == NULL) {
        free(record->name);
        free(record->scores);
        free(record);
        return NULL;
    }

    strcpy(record->name, name);
    for (int i = 0; i < num_scores; ++i)
        record->scores[i] = id * 10 + i;
    return record;
}

static void delete_record(Record *record, int fixed)
{
    if (fixed) {
        free(record->name);
        free(record->scores);
    }
    free(record);
}

static int process_messages(int count, int fixed)
{
    for (int i = 0; i < count; ++i) {
        char *buffer = malloc(MSG_SIZE);
        if (buffer == NULL)
            return -1;
        snprintf(buffer, MSG_SIZE, "Message #%d: data payload here", i);
        if (i % 10 == 0)
            printf("Processed: %s\n", buffer);
        if (fixed)
            free(buffer);
    }
    return 0;
}

int main(int argc, char **argv)
{
    int fixed = 0;
    int hold_seconds = 0;
#ifdef LEAKY_APP_FIXED
    fixed = 1;
#endif
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--fixed") == 0) {
            fixed = 1;
        } else if (strcmp(argv[i], "--leak") == 0) {
            fixed = 0;
        } else if (strcmp(argv[i], "--hold") == 0) {
            hold_seconds = 6;
        } else {
            fprintf(stderr, "Usage: %s [--leak|--fixed] [--hold]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    Record *records[NUM_RECORDS] = {0};
    for (int i = 0; i < NUM_RECORDS; ++i) {
        records[i] = create_record(i, "EmbeddedLinuxStudent", 5);
        if (records[i] == NULL) {
            fprintf(stderr, "Could not allocate record %d\n", i);
            for (int j = 0; j < i; ++j)
                delete_record(records[j], 1);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < NUM_RECORDS; ++i)
        delete_record(records[i], fixed);

    if (process_messages(50, fixed) != 0) {
        fputs("Could not allocate message buffer\n", stderr);
        return EXIT_FAILURE;
    }

    printf("Leak mode: %s\n", fixed ? "fixed" : "intentional leaks");
    if (hold_seconds > 0) {
        puts("Holding allocations for /proc inspection");
        sleep((unsigned int)hold_seconds);
    }
    return EXIT_SUCCESS;
}

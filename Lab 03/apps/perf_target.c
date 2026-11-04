#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 100000
#define ITERATIONS 20

__attribute__((noinline))
static double heavy_compute(const double *values, int count)
{
    double result = 0.0;
    for (int i = 0; i < count; ++i)
        result += sqrt(values[i]) * log(values[i] + 1.0);
    return result;
}

__attribute__((noinline))
static int linear_search(const int *values, int count, int target)
{
    for (int i = 0; i < count; ++i) {
        if (values[i] == target)
            return i;
    }
    return -1;
}

static int io_intensive(int count)
{
    FILE *file = fopen("/tmp/perf_test.txt", "w");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    for (int i = 0; i < count; ++i)
        fprintf(file, "Line %d: measurement data\n", i);
    return fclose(file);
}

int main(void)
{
    double *dvalues = malloc(sizeof(*dvalues) * ARRAY_SIZE);
    int *ivalues = malloc(sizeof(*ivalues) * ARRAY_SIZE);
    if (dvalues == NULL || ivalues == NULL) {
        fputs("Could not allocate profiling arrays\n", stderr);
        free(dvalues);
        free(ivalues);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < ARRAY_SIZE; ++i) {
        dvalues[i] = (double)(i + 1);
        ivalues[i] = i;
    }

    puts("CPU workload");
    double total = 0.0;
    for (int i = 0; i < ITERATIONS; ++i)
        total += heavy_compute(dvalues, ARRAY_SIZE);
    printf("Result: %.3f\n", total);

    puts("Linear search workload");
    for (int i = 0; i < 1000; ++i)
        (void)linear_search(ivalues, ARRAY_SIZE, ARRAY_SIZE - 1);

    puts("File I/O workload");
    if (io_intensive(10000) != 0) {
        free(dvalues);
        free(ivalues);
        return EXIT_FAILURE;
    }

    free(dvalues);
    free(ivalues);
    puts("Done");
    return EXIT_SUCCESS;
}

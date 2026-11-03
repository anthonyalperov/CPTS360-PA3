
#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
// Additional headers as needed
#include <time.h>
#include <unistd.h>

#define PROC_STATUS "/proc/kmlab/status"

void register_process(unsigned int pid)
{
    FILE *file = fopen(PROC_STATUS, "w");

    if (file == NULL)
    {
        perror("Failed to open /proc/kmlab/status");
        exit(EXIT_FAILURE);
    }

    if (fprintf(file, "%u\n", pid) < 0)
    {
        perror("Failed to register PID");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (fclose(file) == EOF)
    {
        perror("Failed to finish PID registration");
        exit(EXIT_FAILURE);
    }

    printf("Registered PID: %u\n", pid);
}

int main(int argc, char* argv[])
{
    int __expire = 10;
    time_t start_time = time(NULL);

    if (argc == 2) {
        __expire = atoi(argv[1]);
    }

    if (__expire < 1)
    {
        fprintf(stderr, "Duration must be positive\n");
        return EXIT_FAILURE;
    }

    register_process(getpid());

    // Perform CPU-intensive calculations
    volatile unsigned long long result = 0;

    // Terminate user application if the time has expired
    while (1) {
        for (unsigned long i = 0; i < 1000000; i++)
        {
            result += ((long long)i * i) % 97;
        }

        if ((int)(time(NULL) - start_time) > __expire) {
            break;
        }
    }

    FILE *file = fopen(PROC_STATUS, "r");

    if (file == NULL)
    {
        perror("Failed to read /proc/kmlab/status");
        return EXIT_FAILURE;
    }

    printf("\n");
    printf("Registered processes and CPU times:\n");

    char line[256];

    while (fgets(line, sizeof(line), file) != NULL)
    {
        printf("%s", line);
    }

    fclose(file);

    return 0;
}

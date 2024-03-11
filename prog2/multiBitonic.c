/**
 *  \file multiBitonic.c (implementation file)
 *
 *  \brief Assignment 1.2: Multi-threaded bitonic sort.
 *
 *  This file contains the definition of the multi-threaded bitonic sort algorithm.
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "const.h"
#include "shared.c"

/**
 *  \brief Prints the usage of the program.
 *
 *  \param cmd_name name of the command that started the program
 */
void printUsage(char *cmd_name) {
    fprintf(stderr, "Usage: %s REQUIRED OPTIONS\n"
                    "REQUIRED:\n"
                    "-f --- input file with numbers\n"
                    "OPTIONS:\n"
                    "-h --- print this help\n"
                    "-n --- number of worker threads (default is %d, minimum is 1)\n", cmd_name, N_WORKERS);
}

/**
 *  \brief Gets the time elapsed since the last call to this function.
 *
 *  \return time elapsed in seconds
 */
static double get_delta_time(void) {
    static struct timespec t0, t1;

    // t0 is assigned the value of t1 from the previous call. if there is no previous call, t0 = t1 = 0
    t0 = t1;

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double) (t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double) (t1.tv_nsec - t0.tv_nsec);
}

/**
 *  \brief Merges two halves of an integer array in the desired order.
 *
 *  \param arr array to be merged
 *  \param low_index index of the first element of the array
 *  \param count number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 */
void bitonic_merge(int *arr, int low_index, int count, int direction) {
    if (count <= 1) return;
    int half = count / 2;
    // move the numbers to the correct half
    for (int i = low_index; i < low_index + half; i++) {
        if (direction == (arr[i] > arr[i + half])) {
            int temp = arr[i];
            arr[i] = arr[i + half];
            arr[i + half] = temp;
        }
    }
    // merge left half
    bitonic_merge(arr, low_index, half, direction);
    // merge right half
    bitonic_merge(arr, low_index + half, half, direction);
}

/**
 *  \brief Worker thread function that executes merge tasks.
 *
 *  It dequeues a task from the FIFO queue and executes it.
 *  When there are no more tasks, the thread exits.
 *
 *  \param arg pointer to the shared area
 */
static void *bitonic_worker(void *arg) {
    shared_t *shared = (shared_t *) arg;
    queue_t *queue = shared->queue;

    while (1) {
        worker_task_t task = dequeue(queue);
        if (task.arr == NULL) break; // exit the thread
        bitonic_merge(task.arr, task.low_index, task.count, task.direction);
        // decrease the number of merge tasks to be executed in the current level
        decrement_level_count(queue);
    }
    return (void *) EXIT_SUCCESS;
}

/**
 *  \brief Distributor thread function that distributes merge tasks and executes the bitonic sort.
 *
 *  It enqueues a task to the FIFO queue for each level of the bitonic sort.
 *  It waits for the worker threads to finish the level before enqueuing the next level.
 *
 *  \param arg pointer to the shared area
 */
static void *bitonic_distributor(void *arg) {
    shared_t *shared = (shared_t *) arg;
    char *file_path = shared->file_path;
    int direction = shared->direction;
    int n_workers = shared->n_workers;
    queue_t *queue = shared->queue;

    // open the file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[DIST] Could not open file %s\n", file_path);
        return (void *) EXIT_FAILURE;
    }
    // read the size of the array
    int size;
    if (fread(&size, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "[DIST] Could not read the size of the array\n");
        fclose(file);
        return (void *) EXIT_FAILURE;
    }
    // size must be power of 2
    if ((size & (size - 1)) != 0) {
        fprintf(stderr, "[DIST] The size of the array must be a power of 2\n");
        fclose(file);
        return (void *) EXIT_FAILURE;
    }
    shared->size = size;
    fprintf(stdout, "[DIST] Array size: %d\n", size);
    // allocate memory for the array
    int *arr = (int *) malloc(size * sizeof(int));
    if (arr == NULL) {
        fprintf(stderr, "[DIST] Could not allocate memory for the array\n");
        fclose(file);
        return (void *) EXIT_FAILURE;
    }
    shared->arr = arr;
    // load array into memory
    int num, ni = 0;
    while (fread(&num, sizeof(int), 1, file) == 1 && ni < size) {
        arr[ni++] = num;
    }
    // close the file
    fclose(file);

    // START TIME
    get_delta_time();

    // distribute merge tasks for each level of the bitonic sort
    if (size > 1) {
        for (int count = 2; count <= size; count *= 2) {
            // set the number of merge tasks to be executed in the next level
            int level_count = size / count;
            set_level_count(queue, level_count);
            fprintf(stdout, "[DIST] Lvl merges: %d", level_count);
            fflush(stdout);
            // enqueue merge tasks for the next level
            for (int low_index = 0; low_index < size; low_index += count) {
                // sub_direction is the direction of the sub-merge
                int sub_direction = (low_index / count) % 2 == 0 != 0 == direction;
                worker_task_t task = {arr, low_index, count, sub_direction};
                enqueue(queue, task);
            }
            // wait for the current level to finish
            wait_level_end(queue);
            fprintf(stdout, " --- done\n");
        }
    }

    // END TIME
    fprintf(stdout, "[DIST] Time elapsed: %.9f seconds\n", get_delta_time());

    // send termination tasks to worker threads
    while (n_workers-- > 0) {
        worker_task_t task = {.arr =  NULL};
        enqueue(queue, task);
    }
    return (void *) EXIT_SUCCESS;
}

/**
 *  \brief Main function of the program.
 *
 *  It processes the command line options, initializes the shared area, creates the distributor and worker threads,
 *  waits for the threads to finish, checks if the array is sorted and frees the allocated memory.
 *
 *  \param argc number of command line arguments
 *  \param argv array of command line arguments
 *
 *  \return EXIT_SUCCESS if the array is sorted, EXIT_FAILURE otherwise
 */
int main(int argc, char *argv[]) {
    // program arguments
    char *cmd_name = argv[0];
    char *file_path = NULL;
    int n_workers = N_WORKERS;

    // process command line options
    int opt;
    do {
        switch ((opt = getopt(argc, argv, "f:n:"))) {
            case 'f':
                file_path = optarg;
                if (file_path == NULL) {
                    fprintf(stderr, "[MAIN] Invalid file name\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'n':
                n_workers = atoi(optarg);
                if (n_workers < 1) {
                    fprintf(stderr, "[MAIN] Invalid number of worker threads\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                printUsage(cmd_name);
                return EXIT_SUCCESS;
            case '?':
                fprintf(stderr, "[MAIN] Invalid option -%c\n", optopt);
                printUsage(cmd_name);
                return EXIT_FAILURE;
            case -1:
                break;
        }
    } while (opt != -1);
    if (file_path == NULL) {
        fprintf(stderr, "[MAIN] Input file not specified\n");
        printUsage(cmd_name);
        return EXIT_FAILURE;
    }

    // initialize the shared area
    shared_t *shared = (shared_t *) malloc(sizeof(shared_t));
    if (shared == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the shared area\n");
        return EXIT_FAILURE;
    }
    queue_t *queue = (queue_t *) malloc(sizeof(queue_t));
    if (queue == NULL) {
        fprintf(stderr, "[MAIN] Could not allocate memory for the FIFO queue\n");
        free(shared);
        return EXIT_FAILURE;
    }
    init_queue(queue);
    shared->file_path = file_path;
    shared->direction = 0; // descending order
    shared->n_workers = n_workers;
    shared->queue = queue;

    // create distributor and worker threads
    pthread_t distributor;
    if (pthread_create(&distributor, NULL, bitonic_distributor, shared) != 0) {
        fprintf(stderr, "[MAIN] Could not create distributor thread\n");
        free(shared);
        return EXIT_FAILURE;
    } else {
        fprintf(stdout, "[MAIN] Distributor thread has been created\n");
    }
    pthread_t workers[n_workers];
    for (int i = 0; i < n_workers; i++) {
        if (pthread_create(&workers[i], NULL, bitonic_worker, shared) != 0) {
            fprintf(stderr, "[MAIN] Could not create worker thread %d\n", i + 1);
            free(shared);
            return EXIT_FAILURE;
        } else {
            fprintf(stdout, "[MAIN] Worker threads have been created (%d/%d)\r", i + 1, n_workers);
        }
    }
    fprintf(stdout, "\n");

    // wait for threads to finish
    void *ptr_retcode_void;
    int *ptr_retcode_int;
    pthread_join(distributor, &ptr_retcode_void);
    ptr_retcode_int = (int *) ptr_retcode_void;
    if (ptr_retcode_int != EXIT_SUCCESS) {
        fprintf(stderr, "[MAIN] Distributor thread has failed with return code %d\n", *ptr_retcode_int);
        free(shared);
        return EXIT_FAILURE;
    } else {
        fprintf(stdout, "[MAIN] Distributor thread has finished\n");
    }
    for (int i = 0; i < n_workers; i++) {
        pthread_join(workers[i], &ptr_retcode_void);
        ptr_retcode_int = (int *) ptr_retcode_void;
        if (ptr_retcode_int != EXIT_SUCCESS) {
            fprintf(stderr, "[MAIN] Worker thread %d has failed with return code %d\n", i + 1,
                    *ptr_retcode_int);
            free(shared);
            return EXIT_FAILURE;
        } else {
            fprintf(stdout, "[MAIN] Worker threads have finished (%d/%d)\r", i + 1, n_workers);
        }
    }
    fprintf(stdout, "\n");

    // check if array is sorted
    int *arr = shared->arr;
    int size = shared->size;
    for (int i = 0; i < size - 1; i++) {
        if (arr[i] < arr[i + 1]) {
            fprintf(stderr, "[MAIN] Error in position %d between element %d and %d\n",
                    i, arr[i], arr[i + 1]);
            return EXIT_FAILURE;
        }
    }
    printf("[MAIN] The array is sorted, everything is OK! :)\n");

    free(shared->arr);
    free(shared->queue);
    free(shared);
    return EXIT_SUCCESS;
}

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
 *  \brief Sorts an integer array using the bitonic sort algorithm.
 *
 *  \param arr array to be sorted
 *  \param size number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 *  \param queue pointer to the FIFO queue
 */
void bitonic_sort(int *arr, int size, int direction, queue_t *queue) {
    if (size <= 1) return;
    for (int count = 2; count <= size; count *= 2) {
        // set the number of merge tasks to be executed in the next level
        int level_count = size / count;
        set_level_count(queue, level_count);
        fprintf(stdout, "Number of merges needed in this level: %d", level_count);
        fflush(stdout);
        // enqueue merge tasks for the next level
        for (int low_index = 0; low_index < size; low_index += count) {
            // sub_direction is the direction of the sub-merge
            int sub_direction = (low_index / count) % 2 == 0 != 0 == direction;
            merge_task_t task = {arr, low_index, count, sub_direction};
            enqueue(queue, task);
        }
        // wait for the current level to finish
        wait_level_end(queue);
        fprintf(stdout, " --- done\n");
    }
}

/**
 *  \brief Worker thread function that executes merge tasks.
 *
 *  It dequeues a task from the FIFO queue and executes it.
 *  When there are no more tasks, the thread exits.
 *
 *  \param arg pointer to the FIFO queue
 */
static void *bitonic_merge_worker(void *arg) {
    queue_t *queue = (queue_t *) arg;
    while (1) {
        merge_task_t task = dequeue(queue);
        if (task.arr == NULL) break; // exit the thread
        bitonic_merge(task.arr, task.low_index, task.count, task.direction);
        // decrease the number of merge tasks to be executed in the current level
        decrement_level_count(queue);
    }
    return NULL;
}

void printUsage(char *cmd_name) {
    fprintf(stderr, "Usage: %s REQUIRED OPTIONS\n"
                    "REQUIRED:\n"
                    "-f --- input file with numbers\n"
                    "OPTIONS:\n"
                    "-h --- print this help\n"
                    "-n --- number of worker threads (default is %d, minimum is 1)\n", cmd_name, N_WORKERS);
}

int main(int argc, char *argv[]) {
    // program arguments
    char *cmd_name = argv[0];
    char *file_name = NULL;
    int n_workers = N_WORKERS;

    int opt;
    do {
        switch ((opt = getopt(argc, argv, "f:n:"))) {
            case 'f':
                file_name = optarg;
                if (file_name == NULL) {
                    fprintf(stderr, "Invalid file name\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'n':
                n_workers = atoi(optarg);
                if (n_workers < 1) {
                    fprintf(stderr, "Invalid number of worker threads\n");
                    printUsage(cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                printUsage(cmd_name);
                return EXIT_SUCCESS;
            case '?':
                fprintf(stderr, "Invalid option -%c\n", optopt);
                printUsage(cmd_name);
                return EXIT_FAILURE;
            case -1:
                break;
        }
    } while (opt != -1);
    if (file_name == NULL) {
        fprintf(stderr, "Input file not specified\n");
        printUsage(cmd_name);
        return EXIT_FAILURE;
    }

    // open the file
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file %s\n", file_name);
        return EXIT_FAILURE;
    }

    // read the size of the array
    int size;
    if (fread(&size, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Could not read the size of the array\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    // size must be power of 2
    if ((size & (size - 1)) != 0) {
        fprintf(stderr, "The size of the array must be a power of 2\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Array size: %d\n", size);

    // allocate memory for the array
    int *arr = (int *) malloc(size * sizeof(int));
    if (arr == NULL) {
        fprintf(stderr, "Could not allocate memory for the array\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    // load array into memory
    int num, ni = 0;
    while (fread(&num, sizeof(int), 1, file) == 1 && ni < size) {
        arr[ni++] = num;
    }

    // initialize the FIFO queue
    queue_t *queue = (queue_t *) malloc(sizeof(queue_t));
    if (queue == NULL) {
        fprintf(stderr, "Could not allocate memory for the queue\n");
        free(arr);
        fclose(file);
        return EXIT_FAILURE;
    }
    init_queue(queue);

    // create threads
    pthread_t workers[n_workers];
    for (int ti = 0; ti < n_workers; ti++) {
        if (pthread_create(&workers[ti], NULL, bitonic_merge_worker, queue) != 0) {
            fprintf(stderr, "Could not create thread %d/%d\n", ti + 1, n_workers);
            free(queue);
            free(arr);
            fclose(file);
            return EXIT_FAILURE;
        }
        fprintf(stdout, "Thread %d/%d was created\n", ti + 1, n_workers);
    }

    // bitonic sort on the array in descending order
    bitonic_sort(arr, size, 0, queue);

    // send termination tasks to threads
    for (int ti = 0; ti < n_workers; ti++) {
        merge_task_t task = {.arr =  NULL};
        enqueue(queue, task);
    }

    // wait for threads to finish
    for (int ti = 0; ti < n_workers; ti++) {
        pthread_join(workers[ti], NULL);
        fprintf(stdout, "Thread %d/%d has finished\n", ti + 1, n_workers);
    }

    // print results
    if (size <= 32) {
        fprintf(stdout, "Sorted array:\n");
        for (int k = 0; k < size; k++) {
            fprintf(stdout, "%d: %d\n", k, arr[k]);
        }
    } else {
        fprintf(stdout, "First 16:\n");
        for (int k = 0; k < 16; k++) {
            fprintf(stdout, "%d ", arr[k]);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "Last 16:\n");
        for (int k = size - 16; k < size; k++) {
            fprintf(stdout, "%d ", arr[k]);
        }
        fprintf(stdout, "\n");
    }
    free(queue);
    free(arr);
    fclose(file);
    return EXIT_SUCCESS;
}

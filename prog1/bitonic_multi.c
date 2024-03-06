#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define NUM_THREADS 8
#define MAX_QUEUE_SIZE 1024


/**
 * \brief Structure that represents a task to be executed by a thread.
 */
typedef struct {
    int *arr;
    int low_index;
    int count;
    int direction;
} bitonic_merge_t;


/**
 * \brief Structure that represents a FIFO.
 */
typedef struct {
    bitonic_merge_t tasks[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
    int level_count; // number of tasks in the current merge level
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_cond_t level_done; // signal that the current merge level is done
} queue_t;


/**
 * \brief Initialize the FIFO queue.
 *
 * \param queue pointer to the queue
 */
void init_queue(queue_t *queue) {
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    queue->level_count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    pthread_cond_init(&queue->level_done, NULL);
}


/**
 * \brief Enqueue a task in the FIFO queue.
 *
 * \param queue pointer to the queue
 * \param task task to be enqueued
 */
void enqueue(queue_t *queue, bitonic_merge_t task) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->size >= MAX_QUEUE_SIZE) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    queue->tasks[queue->rear] = task;
    queue->size++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
}


/**
 * \brief Dequeue a task from the FIFO queue.
 *
 * \param queue pointer to the queue
 * \return the dequeued task
 */
bitonic_merge_t dequeue(queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->size <= 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    bitonic_merge_t task = queue->tasks[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->size--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return task;
}


/**
 * \brief Sets the desired number of merge tasks to be executed in the next level.
 *
 * Should be called by the producer thread before enqueuing merge tasks for the next level.
 *
 * \param queue pointer to the queue
 * \param count number of merge tasks
 */
void set_level_count(queue_t *queue, int count) {
    pthread_mutex_lock(&queue->mutex);
    queue->level_count = count;
    pthread_mutex_unlock(&queue->mutex);
}


/**
 * \brief Decreases the number of merge tasks to be executed in the current level.
 *
 * Should be called by a thread after it finishes a merge task.
 *
 * \param queue pointer to the queue
 */
void decrease_level_count(queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    queue->level_count--;
    if (queue->level_count == 0) {
        pthread_cond_signal(&queue->level_done);
    }
    pthread_mutex_unlock(&queue->mutex);
}


/**
 * \brief Waits for the number of merge tasks to be executed in the current level to reach zero.
 *
 * Should be called by the producer thread before enqueuing merge tasks for the next level.
 *
 * \param queue pointer to the queue
 */
void wait_level_end(queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->level_count > 0) {
        pthread_cond_wait(&queue->level_done, &queue->mutex);
    }
    pthread_mutex_unlock(&queue->mutex);
}


/**
 *  \brief Merges two halves of an integer array in the desired order.
 *
 *  \param arr array to be merged
 *  \param low_index index of the first element of the array
 *  \param count number of elements in the array
 *  \param direction 0 for descending order, 1 for ascending order
 */
void bitonic_merge(int *arr, int low_index, int count, int direction) { // NOLINT(*-no-recursion)
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
            bitonic_merge_t task = {arr, low_index, count, sub_direction};
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
        bitonic_merge_t task = dequeue(queue);
        if (task.arr == NULL) break; // exit the thread
        bitonic_merge(task.arr, task.low_index, task.count, task.direction);
        // decrease the number of merge tasks to be executed in the current level
        decrease_level_count(queue);
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    FILE *file;

    for (int fi = 1; fi < argc; fi++) {
        file = fopen(argv[fi], "rb");
        if (file == NULL) {
            fprintf(stderr, "Could not open file %s\n", argv[fi]);
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
        pthread_t threads[NUM_THREADS];
        for (int ti = 0; ti < NUM_THREADS; ti++) {
            if (pthread_create(&threads[ti], NULL, bitonic_merge_worker, queue) != 0) {
                fprintf(stderr, "Could not create thread %d/%d\n", ti + 1, NUM_THREADS);
                free(queue);
                free(arr);
                fclose(file);
                return EXIT_FAILURE;
            }
            fprintf(stdout, "Thread %d/%d was created\n", ti + 1, NUM_THREADS);
        }

        // bitonic sort on the array in descending order
        bitonic_sort(arr, size, 0, queue);

        // send termination tasks to threads
        for (int ti = 0; ti < NUM_THREADS; ti++) {
            bitonic_merge_t task = {.arr =  NULL};
            enqueue(queue, task);
        }

        // wait for threads to finish
        for (int ti = 0; ti < NUM_THREADS; ti++) {
            pthread_join(threads[ti], NULL);
            fprintf(stdout, "Thread %d/%d has finished\n", ti + 1, NUM_THREADS);
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
    }
    return EXIT_SUCCESS;
}
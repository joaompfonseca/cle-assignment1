/**
 *  \file shared.h (interface file)
 *
 *  \brief Assignment 1.2: multithreaded bitonic sort.
 *
 *  This file contains the definition of a FIFO queue and the operations that it supports.
 *  The queue is used to store merge tasks to be executed by the worker threads.
 *
 *  Producer thread operations:
 *  - enqueue: add a task to the queue
 *  - set_level_count: set the number of tasks that need to be executed in the level
 *  - wait_level_end: wait for the current level to finish
 *
 *  Consumer thread operations:
 *  - dequeue: remove a task from the queue
 *  - decrement_level_count: decrements the number of tasks to be executed in the level
 *
 *  \author João Fonseca - March 2024
 *  \author Rafael Gonçalves - March 2024
 */

#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include "const.h"

/** \brief Structure that represents a merge task to be executed by a worker thread */
typedef struct {
    int *arr;
    int low_index;
    int count;
    int direction;
    int sort_or_merge;
} worker_task_t;

/** \brief Structure that represents a FIFO queue */
typedef struct {
    worker_task_t tasks[QUEUE_SIZE];
    int front;
    int rear;
    int size;
    int level_count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_cond_t level_done;
} queue_t;

/** \brief Structure that represents the shared area */
typedef struct {
    char *file_path;
    int *arr;
    int size;
    int direction;
    int n_workers;
    queue_t *queue;
} shared_t;

/**
 * \brief Initializes the FIFO queue.
 *
 * Should be called by the main thread before using the queue.
 *
 * \param queue pointer to the queue
 */
void init_queue(queue_t *queue);

/**
 * \brief Enqueue a merge task in the FIFO queue.
 *
 * Should be called by the main thread to add a merge task to the queue.
 *
 * \param queue pointer to the queue
 * \param task merge task to be enqueued
 */
void enqueue(queue_t *queue, worker_task_t task);

/**
 * \brief Dequeue a merge task from the FIFO queue.
 *
 * Should be called by a worker thread to get a merge task to execute.
 *
 * \param queue pointer to the queue
 *
 * \return the dequeued merge task
 */
worker_task_t dequeue(queue_t *queue);

/**
 * \brief Sets the desired number of merge tasks to be executed in the next level.
 *
 * Should be called by the main thread before enqueuing merge tasks for the next level.
 *
 * \param queue pointer to the queue
 * \param count number of merge tasks
 */
void set_level_count(queue_t *queue, int count);

/**
 * \brief Decrements the number of merge tasks to be executed in the current level.
 *
 * Should be called by a worker thread after it finishes a merge task.
 *
 * \param queue pointer to the queue
 */
void decrement_level_count(queue_t *queue);

/**
 * \brief Waits for the number of merge tasks to be executed in the current level to reach zero.
 *
 * Should be called by the main thread before enqueuing merge tasks for the next level.
 *
 * \param queue pointer to the queue
 */
void wait_level_end(queue_t *queue);

#endif /* SHARED_H */

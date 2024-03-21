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
    int type;
} worker_task_t;

/** \brief Structure that represents the configuration of the program */
typedef struct {
    char* file_path;
    int *arr;
    int size;
    int direction;
    int n_workers;
} config_t;

/** \brief Structure that represents the tasks */
typedef struct {
    worker_task_t *list;
    int size;
    int index;
    int done;
    pthread_cond_t tasks_ready;
    pthread_cond_t tasks_done;
} tasks_t;

/** \brief Structure that represents the shared area */
typedef struct {
    pthread_mutex_t mutex;
    config_t config;
    tasks_t tasks;
} shared_t;

/**
 * \brief Initializes the FIFO queue.
 *
 * Should be called by the main thread before using the queue.
 *
 * \param queue pointer to the queue
 */
void init_shared(shared_t *shared, config_t *config, tasks_t *tasks);

/**
 * \brief Enqueue a merge task in the FIFO queue.
 *
 * Should be called by the main thread to add a merge task to the queue.
 *
 * \param queue pointer to the queue
 * \param task merge task to be enqueued
 */
void set_tasks(shared_t *shared, worker_task_t *list, int size);

/**
 * \brief Dequeue a merge task from the FIFO queue.
 *
 * Should be called by a worker thread to get a merge task to execute.
 *
 * \param queue pointer to the queue
 *
 * \return the dequeued merge task
 */
worker_task_t get_task(shared_t *shared, int index);

/**
 * \brief Sets the desired number of merge tasks to be executed in the next level.
 *
 * Should be called by the main thread before enqueuing merge tasks for the next level.
 *
 * \param queue pointer to the queue
 * \param count number of merge tasks
 */
void task_done(shared_t *shared, int index);

#endif /* SHARED_H */

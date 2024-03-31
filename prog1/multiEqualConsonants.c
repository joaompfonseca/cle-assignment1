#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <getopt.h>
#include "wordUtils.h"

#define MAX_CHUNK_SIZE 4096
#define N_WORKERS 4 // default number of workers
#define MAX_CHAR_LENGTH 5 // max number of bytes of a UTF-8 character + null terminator
#define CONSONANTS "bcdfghjklmnpqrstvwxyz"
#define CLOCK_MONOTONIC 1 // for clock_gettime

struct SharedFileData {
    char *fileName;
    int nWords;
    int nWordsWMultCons;
    FILE *fp;
};

struct ChunkData {
    int fileIndex;
    bool finished;
    int nWords;
    int chunkSize;
    int nWordsWMultCons;
    bool inWord;
    char *chunk;
};

struct Monitor {
    int currentFile;
    int nFiles;
    struct SharedFileData *filesResults;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

struct SharedFileData *sharedFileData;
struct Monitor monitor;
int nFiles;

static double get_delta_time(void) {
    static struct timespec t0, t1;
    
    // t0 is assigned the value of t1 from the previous call. if there is no previous call, t0 = t1 = 0
    t0 = t1;

    if (clock_gettime (CLOCK_MONOTONIC, &t1) != 0) {
        perror ("clock_gettime");
        exit(1);
    }
    return (double) (t1.tv_sec - t0.tv_sec) + 1.0e-9 * (double) (t1.tv_nsec - t0.tv_nsec);
}

void printResults(int _nFiles) {
    for (int i = 0; i < _nFiles; i++) {
        printf("File name: %s\n", sharedFileData[i].fileName);
        printf("Total number of words: %d\n", sharedFileData[i].nWords);
        printf("Total number of words with at least two instances of the same consonant: %d\n\n", sharedFileData[i].nWordsWMultCons);
    }
}

char extractCharFromFile(FILE *textFile, char *UTF8Char, uint8_t *charSize, uint8_t *removePos) {
    char c;

    // Determine number of bytes of the UTF-8 character
    c = (char) fgetc(textFile); // Byte #1
    if (c == EOF) {
        memset(UTF8Char, 0, MAX_CHAR_LENGTH);
        return EOF;
    }
    else {
        *charSize = lengthCharUtf8(c);
        if (*charSize == 0) {
            // in the middle of a multi-byte UTF-8 character
            *removePos += 1;
            for (int i = 0; i < 2; i++) {
                fseek(textFile, -2, SEEK_CUR);
                c = (char) fgetc(textFile); // Byte #1
                *charSize = lengthCharUtf8(c);
                if (*charSize != 0) {
                    break;
                }
                *removePos += 1;
            }

            // still invalid UTF-8 character
            if (*charSize == 0) {
                printf("Invalid UTF-8 character\n");
                memset(UTF8Char, 0, MAX_CHAR_LENGTH);
                exit(EXIT_FAILURE);
            }
        }

        // Add the first byte to the UTF-8 character
        UTF8Char[0] = c;

        // Read the remaining bytes
        for (int i = 1; i < *charSize; i++) {
            c = (char) fgetc(textFile); // Byte #2, #3, #4
            if (c == EOF) {
                printf("Error reading char: EOF\n");
                memset(UTF8Char, 0, MAX_CHAR_LENGTH);
            }
            else {
                UTF8Char[i] = c;
            }
        }
        // Add null terminator
        UTF8Char[*charSize] = '\0';
        normalizeCharUtf8(UTF8Char);

        return 1;
    }
}

char extractCharFromChunk(char *chunk, char *UTF8Char, int *ptr) {
    char c;

    // Determine number of bytes of the UTF-8 character
    c = chunk[*ptr]; // Byte #1

    if (c == '\0') {
        memset(UTF8Char, 0, MAX_CHAR_LENGTH);
        return -1;
    }
    else {
        int charLength = lengthCharUtf8(c);

        if (charLength == 0) {
            printf("Invalid UTF-8 character\n");
            memset(UTF8Char, 0, MAX_CHAR_LENGTH);
        }

        else {
            // Add the first byte to the UTF-8 character
            UTF8Char[0] = c;
            (*ptr)++;

            // Read the remaining bytes
            for (int i = 1; i < charLength; i++) {
                c = chunk[*ptr]; // Byte #2, #3, #4
                if (c == EOF) {
                    printf("Error reading char: EOF\n");
                    memset(UTF8Char, 0, MAX_CHAR_LENGTH);
                }
                else {
                    UTF8Char[i] = c;
                }
                (*ptr)++;
            }

            // Add null terminator
            UTF8Char[charLength] = '\0';
            normalizeCharUtf8(UTF8Char);
        }

        return 1;
    }
}

void retrieveData(uint8_t workerId, struct ChunkData *chunkData) {
    if (pthread_mutex_lock(&monitor.mutex) != 0) {
        perror("Error: could not lock mutex");
        pthread_exit(NULL);
    }

    // get current file index
    int fileIndex = monitor.currentFile;

    if (monitor.currentFile < monitor.nFiles) {
        // open file
        if (sharedFileData[fileIndex].fp == NULL) {
            if ((sharedFileData[fileIndex].fp = fopen(sharedFileData[fileIndex].fileName, "rb")) == NULL) {
                perror("Error opening file");
                pthread_exit(NULL);
            }
        }

        // read chunk
        chunkData->chunk = (char *)malloc((MAX_CHUNK_SIZE + 1) * sizeof(char)); // +1 for null terminator
        chunkData->chunkSize = fread(chunkData->chunk, 1, MAX_CHUNK_SIZE, sharedFileData[fileIndex].fp);
        chunkData->fileIndex = fileIndex;
        chunkData->finished = false;

        // if chunk size is less than MAX_CHUNK_SIZE, then it is the last chunk
        if (chunkData->chunkSize < MAX_CHUNK_SIZE) {
            fclose(sharedFileData[fileIndex].fp);
            monitor.currentFile++;
        }
        else {
            // read until a word is complete
            char *UTF8Char = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));
            uint8_t charSize;
            uint8_t removePos = 0;

            while (extractCharFromFile(sharedFileData[fileIndex].fp, UTF8Char, &charSize, &removePos) != EOF) {
                if (isCharNotAllowedInWordUtf8(UTF8Char)) {
                    chunkData->chunkSize -= removePos;
                    chunkData->chunk[chunkData->chunkSize] = '\0';
                    break;
                }

                // realloc chunk if necessary
                if (chunkData->chunkSize + charSize > MAX_CHUNK_SIZE) {
                    chunkData->chunk = (char *)realloc(chunkData->chunk, (chunkData->chunkSize + charSize + 1) * sizeof(char));
                }

                // add char to chunk
                for (int i = 0; i < charSize; i++) {
                    chunkData->chunk[chunkData->chunkSize] = UTF8Char[i];
                    chunkData->chunkSize++;
                }
            }

            chunkData->chunk[chunkData->chunkSize] = '\0';
        }
    }

    // unlock mutex
    if (pthread_mutex_unlock(&monitor.mutex) != 0) {
        perror("Error: could not unlock mutex");
        pthread_exit(NULL);
    }
}

// put consOcc and detMultCons as arguments
void processChar(char* word, char *UTF8Char, bool *inWord, int *nWords, int *nWordsWMultCons, int consOcc[], bool *detMultCons) {
    if (*inWord && isCharNotAllowedInWordUtf8(UTF8Char)) {
        // if (strcmp(word, "dizer") == 0) {
        //     printf("%s ", word);
        // }

        *inWord = false;
        memset(consOcc, 0, 26 * sizeof(int));
        // if (*detMultCons) {
        //     printf("%s ", word);
        // }

        memset(word, 0, MAX_CHAR_LENGTH);
    }
    else if (!(*inWord) && isCharStartOfWordUtf8(UTF8Char)) {
        *inWord = true;
        *detMultCons = false;
        (*nWords)++;
        strcpy(word, UTF8Char);
    }
    else if (*inWord) {
        strcat(word, UTF8Char);
    }

    if (strchr(CONSONANTS, UTF8Char[0]) != NULL) {
        consOcc[UTF8Char[0] - 'a']++;

        if (!(*detMultCons) && consOcc[UTF8Char[0] - 'a'] > 1) {
            (*nWordsWMultCons)++;
            *detMultCons = true;
        }
    }
}

void saveResults(struct ChunkData *chunkData) {
    if (pthread_mutex_lock(&monitor.mutex) != 0) {
        perror("Error: could not lock mutex");
        pthread_exit(NULL);
    }

    sharedFileData[chunkData->fileIndex].nWords += chunkData->nWords;
    sharedFileData[chunkData->fileIndex].nWordsWMultCons += chunkData->nWordsWMultCons;

    if (pthread_mutex_unlock(&monitor.mutex) != 0) {
        perror("Error: could not unlock mutex");
        pthread_exit(NULL);
    }
}

void *worker(void *id) {
    uint8_t workerId = *((uint8_t *)id);

    struct ChunkData chunkData;
    int ptr;
    char *currentChar = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));
    int consOcc[26];
    bool detMultCons;

    while (true) {
        chunkData.nWords = 0;
        chunkData.nWordsWMultCons = 0;
        chunkData.finished = true;
        chunkData.inWord = false;

        retrieveData(workerId, &chunkData);

        if (chunkData.finished) {
            break;
        }

        ptr = 0;
        detMultCons = false;
        memset(consOcc, 0, 26 * sizeof(int));

        char* word = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));

        while (extractCharFromChunk(chunkData.chunk, currentChar, &ptr) != -1) {
            processChar(word, currentChar, &chunkData.inWord, &chunkData.nWords, &chunkData.nWordsWMultCons, consOcc, &detMultCons);
        }

        // update shared data
        saveResults(&chunkData);

        memset(chunkData.chunk, 0, MAX_CHUNK_SIZE);
    }

    return (void*) EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    // program arguments
    char *cmd_name = argv[0];
    int nThreads = N_WORKERS;
    int nFiles;
    char **fileNames;

    // process command line options
    int opt;
    do {
        opt = getopt(argc, argv, "n:");
        switch (opt) {
            case 'n':
                nThreads = atoi(optarg);
                if (nThreads < 1) {
                    fprintf(stderr, "[MAIN] Invalid number of worker threads\n");
                    fprintf(stderr, "Usage: %s [-n n_workers] file1.txt file2.txt ...\n", cmd_name);
                    return EXIT_FAILURE;
                }
                break;
            case -1:
                if (optind < argc) {
                    // process remaining arguments
                    nFiles = argc - optind;
                    printf("Number of files: %d\n", nFiles);
                    fileNames = (char **)malloc((nFiles + 1) * sizeof(char *));
                    for (int i = optind; i < argc; i++) {
                        fileNames[i - optind] = argv[i];
                    }
                }
                else {
                    fprintf(stderr, "Usage: %s [-n n_workers] file1.txt file2.txt ...\n", cmd_name);
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-n n_workers] file1.txt file2.txt ...\n", cmd_name);
                exit(EXIT_FAILURE);
        }
    } while (opt != -1);

    printf("Number of workers: %d\n\n", nThreads);

    initializeCharMeaning();
    pthread_t threads[nThreads];

    get_delta_time();

    sharedFileData = (struct SharedFileData *)malloc((nFiles + 1) * sizeof(struct SharedFileData));
    for (int i = 0; i < nFiles; i++) {
        sharedFileData[i].fileName = fileNames[i];
        sharedFileData[i].nWords = 0;
        sharedFileData[i].nWordsWMultCons = 0;
        sharedFileData[i].fp = NULL;
    }

    monitor = (struct Monitor){
        0, // currentFile
        nFiles, // nFiles
        sharedFileData, // filesResults
        PTHREAD_MUTEX_INITIALIZER, // mutex
        PTHREAD_COND_INITIALIZER // cond
    };

    // create nThreads threads
    for (int i = 0; i < nThreads; i++) {
        pthread_create(&threads[i], NULL, worker, &i);
    }

    // join nThreads threads
    for (int i = 0; i < nThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    printResults(nFiles);

    printf("Elapsed time: %f\n", get_delta_time());
    return EXIT_SUCCESS;
}

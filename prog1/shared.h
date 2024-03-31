#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_CHUNK_SIZE 4096

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

extern void allocateSharedData(int _nFiles, char **fileNames);
extern void retrieveData(uint8_t workerId, struct ChunkData *chunkData);
extern void saveResults(struct ChunkData *chunkData);
extern void printResults(int _nFiles);
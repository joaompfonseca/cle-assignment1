#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "wordUtils.h"

#define MAX_CHAR_LENGTH 5
#define MAX_WORD_LENGTH 129
#define CONSONANTS "bcdfghjklmnpqrstvwxyz"
#define CLOCK_MONOTONIC 1

char extractChar(FILE *textFile, char *UTF8Char) {
    char c;

    // Determine number of bytes of the UTF-8 character
    c = (char) fgetc(textFile); // Byte #1
    if (c == EOF) {
        memset(UTF8Char, 0, MAX_CHAR_LENGTH);
        return EOF;
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

            // Read the remaining bytes
            for (int i = 1; i < charLength; i++) {
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
            UTF8Char[charLength] = '\0';
            normalizeCharUtf8(UTF8Char);
        }

        return 1;
    }
}

void processChar(char *UTF8Char, bool *inWord, int *nWords, int *nWordsWMultCons) {
    static int consOcc[26];
    static bool detMultCons;

    if (*inWord && isCharNotAllowedInWordUtf8(UTF8Char)) {
        *inWord = false;
        memset(consOcc, 0, 26 * sizeof(int));
    }
    else if (!(*inWord) && isCharStartOfWordUtf8(UTF8Char)) {
        *inWord = true;
        detMultCons = false;
        (*nWords)++;
    }

    if (strchr(CONSONANTS, UTF8Char[0]) != NULL) {
        consOcc[UTF8Char[0] - 'a']++;

        if (!detMultCons && consOcc[UTF8Char[0] - 'a'] > 1) {
            (*nWordsWMultCons)++;
            detMultCons = true;
        }
    }
}

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

int main(int argc, char *argv[]) {
    FILE *file;
    int nWords, nWordsWMultCons;
    bool inWord;

    initializeCharMeaning();

    get_delta_time();
    for (int i = 1; i < argc; i++) {
        file = fopen(argv[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file %s\n", argv[i]);
            return EXIT_FAILURE;
        }
        // Buffer to store a character in UTF-8
        char *UTF8Char = (char *) malloc(MAX_CHAR_LENGTH * sizeof(char));

        if (UTF8Char == NULL) {
            fprintf(stderr, "Error allocating memory for UTF8Char\n");
            return EXIT_FAILURE;
        }

        nWords = 0;
        nWordsWMultCons = 0;
        inWord = false;

        // Main loop to read the text file
        while (extractChar(file, UTF8Char) != EOF) {
            processChar(UTF8Char, &inWord, &nWords, &nWordsWMultCons);
        }

        fclose(file);
        printf("File %s has %d nWords and %d nWords with at least two equal consonants\n", argv[i], nWords, nWordsWMultCons);
    }
    
    printf("Elapsed time: %f\n", get_delta_time());
    return EXIT_SUCCESS;
}
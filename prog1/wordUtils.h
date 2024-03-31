#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// For Portuguese words, we don't have to consider some special characters
#define START_CHARS "0123456789abcdefghijklmnopqrstuvwxyzàáâãäåæçèéêëìíîïðñòóôõöøùúûüýÿ_"
#define SINGLE_BYTE_DELIMITERS " \t\n\r-\"[]().,:;?!–"
#define MAX_CHAR_LENGTH 5 // max number of bytes of a UTF-8 character + null terminator
#define CONSONANTS "bcdfghjklmnpqrstvwxyz"

extern int charMeaning[256];
extern int lengthCharUtf8(char firstByte);
extern void normalizeCharUtf8(char *charUtf8);
extern void initializeCharMeaning();
extern int isCharStartOfWordUtf8(const char *charUtf8);
extern int isCharNotAllowedInWordUtf8(const char *charUtf8);
extern char extractCharFromFile(FILE *textFile, char *UTF8Char, uint8_t *charSize, uint8_t *removePos);
extern char extractCharFromChunk(char *chunk, char *UTF8Char, int *ptr);
extern void processChar(char *word, char *currentChar, bool *inWord, int *nWords, int *nWordsWMultCons, int consOcc[], bool *detMultCons);

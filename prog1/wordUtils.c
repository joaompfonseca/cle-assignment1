#include "wordUtils.h"

int charMeaning[256];

int lengthCharUtf8(char firstByte) {
    if ((firstByte & 0x80) == 0) {
        return 1;
    } else if ((firstByte & 0xE0) == 0xC0) {
        return 2;
    } else if ((firstByte & 0xF0) == 0xE0) {
        return 3;
    } else if ((firstByte & 0xF8) == 0xF0) {
        return 4;
    }
    return 0;
}

void normalizeCharUtf8(char *charUtf8) {
    // Convert to lowercase
    if (charUtf8[0] >= (char) 0x41 && charUtf8[0] <= (char) 0x5A) { // A-Z
        charUtf8[0] += 0x20; // a-z
    } else if (charUtf8[0] == (char) 0xC3 && charUtf8[1] >= (char) 0x80 && charUtf8[1] <= (char) 0x96) { // À-Ö 
        charUtf8[1] += 0x20; // à-ö
    }

    if (charUtf8[0] == (char) 0xC3 && (charUtf8[1] == (char) 0xA7 || charUtf8[1] == (char) 0x87)) {
        charUtf8[0] = 0x63;
        charUtf8[1] = 0x00;
    }
}

void initializeCharMeaning() {
    memset(charMeaning, 0, 256 * sizeof(int));

    for (int i = 0; i < strlen(START_CHARS); i++) {
        charMeaning[(unsigned char) START_CHARS[i]] = 1;
    }

    for (int i = 0; i < strlen(SINGLE_BYTE_DELIMITERS); i++) {
        charMeaning[(unsigned char) SINGLE_BYTE_DELIMITERS[i]] = 2;

    }
}

int isCharStartOfWordUtf8(const char *charUtf8) {
    return charMeaning[(unsigned char) charUtf8[0]] == 1;
}

int isCharNotAllowedInWordUtf8(const char *charUtf8) {

    if (
        // multi byte delimiter
        (charUtf8[0] == (char) 0xE2 && charUtf8[1] == (char) 0x80 && charUtf8[2] == (char) 0x9C) // “
        || (charUtf8[0] == (char) 0xE2 && charUtf8[1] == (char) 0x80 && charUtf8[2] == (char) 0x9D) // ”
        || (charUtf8[0] == (char) 0xE2 && charUtf8[1] == (char) 0x80 && charUtf8[2] == (char) 0x93) // –
        || (charUtf8[0] == (char) 0xE2 && charUtf8[1] == (char) 0x80 && charUtf8[2] == (char) 0xA6) // …
    ) {
        return 1;
    }
    // single byte delimiter
    return charUtf8[1] == (char) 0x00 && charMeaning[(unsigned char) charUtf8[0]] == 2;
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

void processChar(char* word, char *UTF8Char, bool *inWord, int *nWords, int *nWordsWMultCons, int consOcc[], bool *detMultCons) {
    if (*inWord && isCharNotAllowedInWordUtf8(UTF8Char)) {
        *inWord = false;
        memset(consOcc, 0, 26 * sizeof(int));
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
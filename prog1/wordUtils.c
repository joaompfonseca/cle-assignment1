#include "wordUtils.h"
#include <string.h>

// For Portuguese words, we don't have to consider some special characters
#define START_CHARS "0123456789abcdefghijklmnopqrstuvwxyzàáâãäåæçèéêëìíîïðñòóôõöøùúûüýÿ_"
#define SINGLE_BYTE_DELIMITERS " \t\n\r-\"[]().,:;?!–"

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

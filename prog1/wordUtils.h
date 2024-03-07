extern int charMeaning[256];

extern int lengthCharUtf8(char firstByte);

extern void normalizeCharUtf8(char *charUtf8);

extern void initializeCharMeaning();

extern int isCharStartOfWordUtf8(const char *charUtf8);

extern int isCharNotAllowedInWordUtf8(const char *charUtf8);

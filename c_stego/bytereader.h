#ifndef _br
#define _br

FILE *fileptr;
char *buffer;
long filelen;
long currentByte;
int endReached;

void rewindFile();
int readFile(char *filename);
void prevByte(int step);
char nextByte();
#endif

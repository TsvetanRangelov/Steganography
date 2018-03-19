#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *fileptr;
FILE *dumpptr;
char *buffer;
long filelen;
long currentByte;
int endReached;

int readFile(char *filename){
	fileptr = fopen(filename, "rb");
	fseek(fileptr, 0, SEEK_END);
	filelen = ftell(fileptr);
	rewind(fileptr);
	buffer = (char *) malloc((filelen+1)*sizeof(char));
	if(buffer ==NULL){
		return 1;
	}
	fread(buffer, filelen, 1, fileptr);
	currentByte=0l;
	endReached = 0;
	fclose(fileptr);
	return 0;
}

void prevByte(int step){
	currentByte-=step;
}

void rewindFile(){
	endReached = 0;
	currentByte = 0l;
}
char nextByte(){
	currentByte++;
	if(currentByte==filelen){
		endReached = 1;
	}
	return buffer[currentByte-1];
}

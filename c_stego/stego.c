#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "qdbmp.h"
#include "bytereader.h"

//Max power for a ULONG, helper const = 2^63
const UINT UINTPOW = 9223372036854775808;

//Flag for whether or not to rewind and encode a file again
int toRewind = 0;


//Chess pattern, helper for xor
const UINT CHESSPATTERN = 12273903644374837845;

//Returns the GrayCode encoding for a picture
UCHAR binaryToGray(UCHAR c){
	return c ^ (c >> 1);
}


//Prints a char/pixel as its bits
void printbits(char v) {
	int i;
	for(i = 7; i >= 0; i--) putchar('0' + ((v >> i) & 1));
	putchar(' ');
}

char *strrev(char *str)
{
	int i = strlen(str) - 1, j = 0;

	char ch;
	while (i > j)
	{
		ch = str[i];
		str[i] = str[j];
		str[j] = ch;
		i--;
		j++;
	}
	return str;
}

//Helper power function for UINT to prevent casting
UINT pow(UINT base, UINT power){
	if(power==0) return 1;
	return base*pow(base,power-1);
}

//Counts the number of transitions between two rows
int row_noise(int num){
	int noise=0, i;
	int prev = num%2;
	for(i=0;i<8;++i){
		num/=2;
		noise+= prev ^ (num%2);
		prev = num%2;
	}
	return noise;
}


//Counts the number of transitions between ones and zeros
//in a row
int count_ones(int num){
	int i, ones=0;
	for(i=0;i<8;++i){
		ones+=num%2;
		num/=2;
	}
	return ones;
}



//Returns the noise in the given plane
int calc_noise(UINT plane){
	int rows[8];
	int i, j, noise=0;
	for(i=0;i<8;++i){
		rows[i]=plane%pow(2,8);
		plane/=pow(2,8);
		if(i>=1){
			noise+=count_ones(rows[i]^rows[i-1]);
		}
		noise+=row_noise(rows[i]);
	}

	return noise;
}




//Takes two bmp files and outputs difference to a third one
void bmp_diff(BMP *pic1, BMP *pic2, BMP *out){
	UCHAR pixels1[3], pixels2[3];
	UINT width = BMP_GetWidth(pic1);
	UINT height = BMP_GetHeight(pic1);
	UINT i, j;
	for(i=0;i<width;++i){
		for(j=0;j<height;++j){
			BMP_GetPixelRGB(pic1, i, j, &pixels1[0], &pixels1[1], &pixels1[2]);
			BMP_GetPixelRGB(pic2, i, j, &pixels2[0], &pixels2[1], &pixels2[2]);
			BMP_SetPixelRGB(out, i, j, pixels1[0]-pixels2[0], pixels1[1]-pixels2[1], pixels1[2]-pixels2[2]); 
		}
	}
}


//Takes a set of planes and a location and writes the planes there
void writePlanesToSector(BMP *bmp, UINT x, UINT y, UINT **planes){
	UCHAR pixels[3];
	UINT i,j,k,l;
	for(i=0;i<8;++i){
		for(j=0;j<8;++j){
			pixels[0]=0;
			pixels[1]=0;
			pixels[2]=0;
			for(k=0;k<3;++k){
				for(l=0;l<8;++l){
					pixels[k]+=planes[l%8][k+l/8]/UINTPOW*pow(2,l);
					planes[l][k]=planes[l%8][k+l/8]<<(unsigned long)1;
				}
			}
			BMP_SetPixelRGB(bmp, x+i, y+j, pixels[0], pixels[1], pixels[2]); 
		}
	}
}

//Reads a small 8x8 sector to be used as most efficient subdivision
UINT **readSS(BMP *bmp, UINT x, UINT y){
	UINT **planes;
	planes = (UINT **)calloc(8,sizeof(UINT *));
	UCHAR pixels[3];
	UINT i,j,k,l;
	for(i=0;i<8;++i){
		planes[i]= (UINT *)calloc(3, sizeof(UINT));
	}

	for(i=0;i<8;++i){
		for(j=0;j<8;++j){
			BMP_GetPixelRGB(bmp, x+i, y+j, &pixels[0], &pixels[1], &pixels[2]);
			for(k=0;k<3;++k){
				for(l=0;l<8;++l){

					//Read pixel and save into planes
					planes[l][k]=(planes[l][k]<<(unsigned long)1)+pixels[k]%2;
					pixels[k]=pixels[k]>>1;
				}
			}
		}
	}
	return planes;
}


//calculates the noise in a given sector
void ss_sector_noise(UINT **sector, int *h_noise){
	UINT i, j;
	for(i=0;i<8;++i){
		for(j=0;j<3;++j){
			h_noise[calc_noise(sector[i][j])]+=7;
		}
	}
}


//Encodes a message in a plane
UINT put_message_in_plane(int threshold){
	int i, j;
	UINT newplane=0;
	for(j=0;j<7;++j){
		char b=nextByte();
		if(endReached){
			b =3; // End of text character, unusable on non-binary files
			if(toRewind) rewindFile(); //Flag is set during execution
		}
		newplane=(newplane<<(unsigned long)8);
		newplane+=b;
	}
	if(calc_noise(newplane)<=threshold){
		return newplane ^ CHESSPATTERN;
	}
	return newplane;
}
//Encodes message information into noisy parts of the sector
UINT **encode_data(UINT **sector, int threshold){
	UINT i, j;
	for(i=0;i<8;++i){
		for(j=0;j<3;++j){
			if(endReached) return sector;	
			else
				if(calc_noise(sector[i][j])>threshold){
					sector[i][j]=put_message_in_plane(threshold);
					if(calc_noise(sector[i][j]) <= threshold)	prevByte(7);
				}
		}
	}
	return sector;
}



//Encrypt a sector of variable length
void encrypt_sector(BMP *bmp, UINT x, UINT y, UINT sectorSize, int threshold){
	UINT ***sector;
	UINT numSS = sectorSize*sectorSize/64;
	if((sector = (UINT ***)malloc(numSS*sizeof(UINT **)))==NULL){
		exit(EXIT_FAILURE);
		fprintf(stdout, "%s","Not Enough memory");
	}
	UINT i;
	//reading sector into 8x8 subs for efficiency
	for(i=0;i<numSS;++i){
		sector[i] = readSS(bmp, x+8*(i%(sectorSize/8)), y+8*(i/(sectorSize/8)));
	}
	//encode data from the input into the given sector
	for(i=0;i<numSS;++i){
		sector[i] = encode_data(sector[i], threshold);
	}
	//writing sector into 8x8 subs for efficiency
	for(i=0;i<numSS;++i){
		writePlanesToSector(bmp, x+8*(i%(sectorSize/8)), y+8*(i/(sectorSize/8)), sector[i]);
	}
}


//General picture wrapper that breaks down the picture into suitable
//sizes for computation
void encrypt_picture(BMP *bmp, UINT sectorSize, int threshold){
	UINT width = BMP_GetWidth(bmp);
	UINT height = BMP_GetHeight(bmp);
	UINT i, j;
	for(i=0;i<width/sectorSize;++i){
		for(j=0;j<height/sectorSize;++j){
			encrypt_sector(bmp, i*sectorSize, j*sectorSize, sectorSize, threshold);
		}
	}
}

//General sector noise calculator
void sector_noise(BMP *bmp, UINT x, UINT y, UINT sectorSize, int *h_noise){
	UINT ***sector;
	UINT numSS = sectorSize*sectorSize/64;
	if((sector = (UINT ***)malloc(numSS*sizeof(UINT **)))==NULL){
		exit(EXIT_FAILURE);
		fprintf(stdout, "%s","Not Enough memory");
	}
	UINT i;

	//reading sector into 8x8 subs for efficiency
	for(i=0;i<numSS;++i){
		sector[i] = readSS(bmp, x+8*(i%(sectorSize/8)), y+8*(i/(sectorSize/8)));
		ss_sector_noise(sector[i], h_noise);
	}
}

void histogram_noise(BMP *bmp, UINT sectorSize, int *h_noise){
	UINT width = BMP_GetWidth(bmp);
	UINT height = BMP_GetHeight(bmp);
	UINT i, j;
	for(i=0;i<width/sectorSize;++i){
		for(j=0;j<height/sectorSize;++j){
			sector_noise(bmp, i*sectorSize, j*sectorSize, sectorSize, h_noise);
		}
	}
}

// Find the threshold needed to store the file
int find_threshold(BMP *bmp, UINT sectorSize){
	if(toRewind) return 56;
	int *h_noise = (int *)calloc(112,sizeof(int));
	histogram_noise(bmp, 8, h_noise);
	int i, sum=0;
	for(i=111;i>0;--i){
		sum+=h_noise[i];
		if(sum>filelen){
			if(i>56) return 56;			
			return i;
		}
	}
	return -1;
}


//Encodes a message in a plane
int put_decoded(UINT plane, char *filename){
	int i, j;
	if(plane > UINTPOW){
		plane ^= CHESSPATTERN;
	}
	char *b = strrev((char *)&plane);
	for(i=0;i<7;++i){
		if(b[i]==3){
			printf("%.*s", i, b);
			return 1;
		}
	}
	printf("%.7s", b);
	return 0;
}

//Encodes message information into noisy parts of the sector
int decode_data(UINT **sector, int threshold, char *filename){
	UINT i, j;
	for(i=0;i<8;++i){
		for(j=0;j<3;++j){
			if(calc_noise(sector[i][j])>threshold){
				if(put_decoded(sector[i][j],filename)==1) return 1;
			}
		}
	}
	return 0;
}

//Decrypts a given sector and saves the dump into filename
int decrypt_sector(BMP *bmp, UINT x, UINT y, UINT sectorSize, int threshold, char *filename){
	UINT ***sector;
	UINT numSS = sectorSize*sectorSize/64;
	if((sector = (UINT ***)malloc(numSS*sizeof(UINT **)))==NULL){
		fprintf(stdout, "%s","Not Enough memory");
	}
	UINT i;
	//reading sector into 8x8 subs for efficiency
	for(i=0;i<numSS;++i){
		sector[i] = readSS(bmp, x+8*(i%(sectorSize/8)), y+8*(i/(sectorSize/8)));
		if(decode_data(sector[i], threshold, filename)==1) return 1;
	}
	return 0;
}


//Decode the file and dump the output in filename
int decode(BMP *bmp, UINT sectorSize, int threshold, char *filename){
	UINT width = BMP_GetWidth(bmp);
	UINT height = BMP_GetHeight(bmp);
	UINT i, j;
	for(i=0;i<width/sectorSize;++i){
		for(j=0;j<height/sectorSize;++j){
			if(decrypt_sector(bmp, i*sectorSize, j*sectorSize, sectorSize, threshold, filename)==1) return 1;
		}
	}
	return 0;	
}

void nthplane(BMP *bmp, int n){
	UCHAR pixels[3];
	UINT width = BMP_GetWidth(bmp);
	UINT height = BMP_GetHeight(bmp);
	UINT i, j;
	for(i=0;i<width;++i){
		for(j=0;j<height;++j){
			BMP_GetPixelRGB(bmp, i, j, &pixels[0], &pixels[1], &pixels[2]);
			pixels[0]=binaryToGray(pixels[0]);
			pixels[1]=binaryToGray(pixels[1]);
			pixels[2]=binaryToGray(pixels[2]);
			BMP_SetPixelRGB(bmp, i, j, ((pixels[0]/pow(2,n))%2)*255, ((pixels[1]/pow(2,n))%2)*255, ((pixels[2]/pow(2,n))%2)*255); 
		}
	}

}


int main(int argc, char *argv[]){
	srand(time(NULL));
	//There are 3 arguments given, comparing differences.
	if(!strcmp(argv[1],"diff")){
		BMP* pic1 = BMP_ReadFile(argv[2]);
		BMP* pic2 = BMP_ReadFile(argv[3]);
		BMP* out = BMP_Create(BMP_GetWidth(pic1), BMP_GetHeight(pic1), 24);
		bmp_diff(pic1,pic2,out);
		if(BMP_GetError() != BMP_OK){
			printf("%s\n%s", "There is trouble reading the file", BMP_GetErrorDescription());
		}
		BMP_CHECK_ERROR(stdout, -1);
		BMP_WriteFile(out, argv[4]);
		BMP_Free(pic1);
		BMP_Free(pic2);
		BMP_Free(out);
	}
	else if(!strcmp(argv[1], "nth")){
		BMP* bmp = BMP_ReadFile(argv[2]);
		nthplane(bmp, atoi(argv[4]));
		if(BMP_GetError() != BMP_OK){
			printf("%s\n%s", "There is trouble reading the file", BMP_GetErrorDescription());
		}
		BMP_CHECK_ERROR(stdout, -1);
		BMP_WriteFile(bmp, argv[3]);
		BMP_Free(bmp);
		
	}
	//Encrypting the file as per standard stego
	else if(!strcmp(argv[1],"enc")){
		if(readFile(argv[3])){
			exit(EXIT_SUCCESS);
		}
		int sectorSize = atoi(argv[5]);
		if((argc > 6) && !strcmp(argv[6],"rew")) toRewind = 1;
		BMP* bmp = BMP_ReadFile(argv[2]);
		int threshold = find_threshold(bmp, sectorSize);
		if(threshold == -1){
			exit(EXIT_FAILURE);
		}
		while(!endReached && !toRewind){
			bmp = BMP_ReadFile(argv[2]);
			rewindFile();
			encrypt_picture(bmp, sectorSize, threshold);
			--threshold;
		}
		if(toRewind){
			threshold = 56;
			encrypt_picture(bmp, sectorSize, threshold);
			--threshold;
		}
		printf("%d", threshold+1);

		if(BMP_GetError() != BMP_OK)
			printf("%s\n%s", "There is trouble reading the file", BMP_GetErrorDescription());
		BMP_CHECK_ERROR(stdout, -1);
		BMP_WriteFile(bmp, argv[4]);
		BMP_Free(bmp);
	}
	else if(!strcmp(argv[1], "dec")){
		BMP *bmp = BMP_ReadFile(argv[2]);
		int threshold = atoi(argv[3]);
		int sectorSize = atoi(argv[4]);
		if(decode(bmp, sectorSize, threshold, "byte")==0)
			printf("%s\n", "There is trouble decoding the file. End not reached");
		if(BMP_GetError() != BMP_OK)
			printf("%s\n%s", "There is trouble reading the file", BMP_GetErrorDescription());
		BMP_CHECK_ERROR(stdout, -1);
		BMP_Free(bmp);
		
	}
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qdbmp.h"
#include <time.h>


//Max power for a ULONG, helper const = 2^63
const UINT UINTPOW = 9223372036854775808;


//Returns the GrayCode encoding for a picture
char binaryToGray(char c){
  return c ^ (c >> 1);
}


//Prints a char/pixel as its bits
void printbits(char v) {
  int i;
  for(i = 7; i >= 0; i--) putchar('0' + ((v >> i) & 1));
  putchar(' ');
}


//Helper power function for UINT to prevent casting
UINT pow(UINT base, UINT power){
  if(power==0) return 1;
  return base*pow(base,power-1);
}


//Returns the noise in the given plane
int calc_noise(UINT plane){
  int rows[8];
  int i, j, noise=0;
  for(i=0;i<8;++i){
    rows[i]=plane%pow(2,8);
    plane/=pow(2,8);
    if(i>1){
      noise+=count_ones(rows[i]^rows[i-1]);
    }
    noise+=row_noise(rows[i]);
  }

  return noise;
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
  for(i=0;i<sizeof(int);++i){
    ones+=num%2;
    num/=2;
  }
  return ones;
}


//Takes two bmp files and outputs difference to a third one
void bmp_diff(BMP *pic1, BMP *pic2, BMP *out){
  UCHAR pixels1[3], pixels2[3];
  UINT width = BMP_GetWidth(pic1);
  UINT height = BMP_GetHeight(pic2);
  UINT i, j;
  for(i=0;i<width;++i){
    for(j=0;j<height;++j){
      BMP_GetPixelRGB(pic1, i, j, &pixels1[0], &pixels1[1], &pixels1[2]);
      BMP_GetPixelRGB(pic2, i, j, &pixels2[0], &pixels2[1], &pixels2[2]);
      BMP_SetPixelRGB(out, i, j, pixels1[0]-pixels2[0], pixels1[1]-pixels2[1], pixels1[2]-pixels2[2]); 
    }
  }
}



void encrypt_sector(BMP *bmp, UINT x, UINT y, UINT sectorSize){
  UCHAR sector[sectorSize][sectorSize][3];
  UINT planes[8][3];      //There are 8 bitplanes regardless of sector size
  //Implementation should be changed for bigger sector
  //sizes
  UINT chessPattern;
  UCHAR pixels[3];
  UINT i,j,k,l;



  //Read the picture and store it into bitplanes
  for(i=0;i<sectorSize;++i){
    for(j=0;j<sectorSize;++j){
      chessPattern=(chessPattern<<(unsigned long)1)+(j+1)%2;
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
  

  //Encrypts the data, currently with a random data pattern
  //!!!Should be refactored
  for(i=0;i<8;++i){
    for(j=0;j<3;++j){
      if(calc_noise(planes[i][j])>40){
        planes[i][j]= rand() ^ chessPattern;
      }
    }
  }


  //Translate from bitplanes to pixels and rewrite the picture
  for(i=0;i<sectorSize;++i){
    for(j=0;j<sectorSize;++j){
      pixels[0]=0;
      pixels[1]=0;
      pixels[2]=0;
      for(k=0;k<3;++k){
        for(l=0;l<8;++l){
          pixels[k]+=planes[l][k]/UINTPOW*pow(2,l);
          planes[l][k]=planes[l][k]<<(unsigned long)1;
        }
      }
      BMP_SetPixelRGB(bmp, x+i, y+j, pixels[0], pixels[1], pixels[2]); 
    }
  }
}

//General picture wrapper that breaks down the picture into suitable
//sizes for computation
void encrypt_picture(BMP *bmp, UINT sectorSize){
  UINT width = BMP_GetWidth(bmp);
  UINT height = BMP_GetHeight(bmp);
  UINT i, j;
  for(i=0;i<width/sectorSize;++i){
    for(j=0;j<height/sectorSize;++j){
      encrypt_sector(bmp, i*sectorSize, j*sectorSize, sectorSize);
    }
  }
}


int main(int argc, char *argv[]){
  BMP* bmp = BMP_ReadFile("airplane.bmp");
  srand(time(NULL));
  if(BMP_GetError() != BMP_OK){
    printf("%s\n%s", "There is trouble reading the file", BMP_GetErrorDescription());
  }
  BMP_CHECK_ERROR(stdout, -1);

  printf("Height: %d, Width: %d, Depth in bits: %d\n", BMP_GetHeight(bmp), BMP_GetWidth(bmp), BMP_GetDepth(bmp));
  
  //There are 3 arguments given, comparing differences.
  if(argc==4){
    BMP* pic1 = BMP_ReadFile(argv[1]);
    BMP* pic2 = BMP_ReadFile(argv[2]);
    BMP* out = BMP_Create(BMP_GetWidth(pic1), BMP_GetHeight(pic1), 24);
    bmp_diff(pic1,pic2,out);
    BMP_WriteFile(out, argv[3]);
    BMP_Free(pic1);
    BMP_Free(pic2);
    BMP_Free(out);
  }
  //Encrypting the file as per standard stego
  else{  
    encrypt_picture(bmp, 8);
    BMP_WriteFile(bmp, "airplane.bmp");
    BMP_CHECK_ERROR(stdout, -1);
    BMP_Free(bmp);
  }
  return 0;
}

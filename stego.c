#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qdbmp.h"
#include <time.h>

const UINT UINTPOW = 9223372036854775808;


char binaryToGray(char c){
  return c ^ (c >> 1);
}

void printbits(char v) {
    int i;
    for(i = 7; i >= 0; i--) putchar('0' + ((v >> i) & 1));
    putchar(' ');
}

UINT pow(UINT base, UINT power){
  if(power==0) return 1;
  return base*pow(base,power-1);
}


UINT encode_plane(UINT plane, int sectorSize){
  return plane;
}

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

int count_ones(int num){
  int i, ones=0;
  for(i=0;i<sizeof(int);++i){
    ones+=num%2;
    num/=2;
  }
  return ones;
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


int main(){
  BMP* bmp = BMP_ReadFile("airplane.bmp");
  srand(time(NULL));
  if(BMP_GetError() != BMP_OK){
    printf("%s\n%s", "There is trouble reading the file", BMP_GetErrorDescription());
  }
  BMP_CHECK_ERROR(stdout, -1);

  printf("Height: %d, Width: %d, Depth in bits: %d\n", BMP_GetHeight(bmp), BMP_GetWidth(bmp), BMP_GetDepth(bmp));
  

  encrypt_picture(bmp, 8);
  
  
  BMP_WriteFile(bmp, "airplane.bmp");
  BMP_CHECK_ERROR(stdout, -1);
  BMP_Free(bmp);
  return 0;
}

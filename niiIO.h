#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

class HeaderData{
 public:
  int headerSize;
  short *dims;
  int dataType;
  float offset;
  bool byteSwap;

  HeaderData(){
    headerSize=0;
    dims = new short[8];
    dataType=0;
    offset=0;
    byteSwap=false;
  }
	
  void print(){
    printf("byteSwap = %s\n", (byteSwap)?"true":"false");
    printf("headerSize = %d\n", headerSize);
    printf("dim = ");
    for(int i=0; i<8; i++)
      printf("%d ", dims[i]);
    printf("\ndataType=%d\n", dataType);
    printf("offset = %0.1f\n", offset);
  }

};
	





class niiIO{
 private:
  char *header;
  HeaderData *hd;
  int dataSize;
  int dataTypeSize;

  char *swapBytes(char *data, int size){
    char *tmp = new char[size];
    for(int i=0; i<size; i++)
      tmp[i] = data[size-1-i];
    return tmp;
  }
  


  bool testHeaderForByteSwap(FILE *file){
    int first;
    char *tmp = new char[4];
    fseek(file, 0, SEEK_SET);
    fread(tmp, 1, 4, file);
    first = *(int *)tmp;
    if(first < 338 || first > 358){
      first = *(int *)swapBytes(tmp, 4);
      if(first > 338 && first < 358)
	return true;
      else{
	printf("Unable to read file.\n");
	exit(0);
      }
    }
    else
      return false;
  }


  void readHeader(const char *filename){
    hd = new HeaderData();

    FILE *file = fopen(filename, "rb");
    header = new char[348];
    fread(header, 348, 1, file);

    hd->byteSwap = testHeaderForByteSwap(file);

    char *tmp4 = new char[4];
    char *tmp2 = new char[2];

    fseek(file, 0, SEEK_SET);
    fread(tmp4, 1, 4, file);
    hd->headerSize = (hd->byteSwap) ? *(int *)swapBytes(tmp4, 4) : *(int *)tmp4;

    fseek(file, 40, SEEK_SET);
    for(int i=0; i<8; i++){
      fread(tmp2, 1, 2, file);
      hd->dims[i] = (hd->byteSwap) ? *(short *)swapBytes(tmp2, 2) : *(short *)tmp2;
    }
    fseek(file, 70, SEEK_SET);
    fread(tmp2, 1, 2, file);
    hd->dataType = (hd->byteSwap) ? *(short *)swapBytes(tmp2, 2) : *(short *)tmp2;

    fseek(file, 108, SEEK_SET);
    fread(tmp4, 1, 4, file);
    hd->offset = (hd->byteSwap) ? *(float *)swapBytes(tmp4, 4) : *(float *)tmp4;

		
    fclose(file);
  }
		



  void readData(FILE *file, HeaderData *hd){
    data = new float[width*height*depth];
    char *tmp = new char[dataTypeSize];
		
    fseek(file, hd->offset, SEEK_SET);
    for(int i=0; i<dataSize; i++){
      fread(tmp, 1, dataTypeSize, file);
      if(dataTypeSize==1)
	data[i] = (float)((hd->byteSwap) ? *(char *)swapBytes(tmp, dataTypeSize) : *(char *)tmp);
      else if(dataTypeSize==2)
	data[i] = (float)((hd->byteSwap) ? *(short *)swapBytes(tmp, dataTypeSize) : *(short *)tmp);
      else
	data[i] = (hd->byteSwap) ? *(float *)swapBytes(tmp, dataTypeSize) : *(float *)tmp;
    }
  }


  
 public:
  float *data;
  int width, height, depth;


  niiIO(const char *filename){

    readHeader(filename);
    dataTypeSize = (int)floor(sqrt(hd->dataType));
		
    //hd->print();

    width = hd->dims[1];
    height = hd->dims[2];
    depth = hd->dims[3];
    dataSize = width*height*depth;
  
    readData(fopen(filename, "rb"), hd);
  }

  ~niiIO(){
    delete[] header;
    header=NULL;
    delete hd;
    hd=NULL;
    delete[] data;
    data=NULL;
  }


  void writeOriginalData(const char *filename){
    FILE *outfile = fopen(filename, "wb");
    fwrite(header, 1, 348, outfile);
  
    char *garbage = new char[1];
    char *tmp = new char[dataTypeSize];
    while(ftell(outfile) < hd->offset)
      fwrite(garbage, 1, 1, outfile);
		

    for(int i=0; i<dataSize; i++){
      tmp = reinterpret_cast<char *>(&data[i]);
      if(hd->byteSwap)
	fwrite(swapBytes(tmp, dataTypeSize), 1, dataTypeSize, outfile);
      else 
	fwrite(tmp, 1, dataTypeSize, outfile);
    }

    fclose(outfile);
  }



  void writeData(float *newData, int newDataSize, const char *filename){
    FILE *outfile = fopen(filename, "wb");
    fwrite(header, 1, 348, outfile);
  
    char *garbage = new char[1];
    char *tmp = new char[dataTypeSize];

    short sh;
    char ch;
    float fl;

    while(ftell(outfile) < hd->offset)
      fwrite(garbage, 1, 1, outfile);
    for(int i=0; i<newDataSize; i++){
      if(dataTypeSize==1){
	ch=(char)newData[i];
	tmp = reinterpret_cast<char *>(&ch);
      }
      else if(dataTypeSize==2){
	sh = (short)newData[i];
	tmp = reinterpret_cast<char *>(&sh);
      }
      else{
	fl = newData[i];
	tmp = reinterpret_cast<char *>(&fl);
      }
      if(hd->byteSwap)
	fwrite(swapBytes(tmp, dataTypeSize), 1, dataTypeSize, outfile);
      else
	fwrite(tmp, 1, dataTypeSize, outfile);
    }
		
    fclose(outfile);
  }



};

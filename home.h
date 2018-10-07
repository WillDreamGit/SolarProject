#ifndef __HOME__
#define __HOME__

#include <time.h>

typedef struct {
   char ProdId[20];
   char Current[10];
   char Irradiance[10];
}SBC_t; 


typedef struct {
	char valid;
	tm FirstConnectTime;
	SBC_t SbcRecord;
}myDB_t;

#endif

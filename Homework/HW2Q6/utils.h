/************************************************************************
* File:  utils.h
*
* Purpose:
*
* Notes:
*
************************************************************************/
#ifndef	__utils_h
#define	__utils_h


//#define is_bigendian() ( (*(char*)&bsti) == 0 )
// ( (*(char*)&bsti) == 0 )
bool is_bigendian();

uint64_t htonll (uint64_t InAddr) ;
void swapbytes(void *_object, size_t size);



void die(const char *msg);
double timestamp();
extern char Version[];

double getTime(int);
double getTime1();
double getCurTimeD();
double getCurTime(struct timespec *ts);
double getTimestampD(); 
double getTimestamp(struct timespec *ts);

int convertD2TS(double *timeDouble,  struct timespec *timeTSpec);
bool isTS1GTTS2(struct timespec *TS1, struct timespec *TS2);
double convertTS2D(struct timespec *t);

int delay(int64_t ns);
int gettimeofday_benchmark();

long getMicroseconds(struct timeval *t);
double convertTimeval(struct timeval *t);

long getTimeSpan(struct timeval *start_time, struct timeval *end_time);
void setUnblockOption(int sock, char unblock);
void sockBlockingOn(int sock);
void sockBlockingOff(int sock);
//
//This busyWaits until time delayTime (based on clock_gettime with CLOCK_MONOTONIC)
int busyWait (double delayTime);

#endif



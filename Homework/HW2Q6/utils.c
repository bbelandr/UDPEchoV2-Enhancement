/*********************************************************
* Module Name:  Tools common routines 
*
* File Name:    utils..c
*
*
*********************************************************/
#include "UDPEcho.h"
#include "utils.h"
//#include "portable_endian.h"
//#include <endian.h> 
/* needed by the htonll and ntohll routines */

char Version[] = "2.0";

double getTime(int);
double getTime1();

int delay(int64_t ns);
int gettimeofday_benchmark();
  // 1:getTimeofDay; 
  // 2:clock_gettime with CLOCK_REALTIME
  // 3:clock_gettime with CLOCK_PROCESS_CPU_TIME


void die(const char *msg) {
  if (errno == 0) {
    /* Just the specified message--no error code */
    puts(msg);
  } else {
    /* Message WITH error code/name */
    perror(msg);
  }
  printf("Die message: %s \n", msg);
  
  /* DIE */
  exit(EXIT_FAILURE);
}

long getMicroseconds(struct timeval *t) {
  return (t->tv_sec) * 1000000 + (t->tv_usec);
}

//Returns timeval in seconds with usecond precision
double convertTimeval(struct timeval *t) {
  return ( t->tv_sec + ( (double) t->tv_usec)/1000000 );
}

//Returns time difference in microseconds 
long getTimeSpan(struct timeval *start_time, struct timeval *end_time) {
  long usec2 = getMicroseconds(end_time);
  long usec1 = getMicroseconds(start_time);
  return (usec2 - usec1);
}

double timestamp() 
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0) { 
    die("the sky is falling!"); 
  }
  return ((double)tv.tv_sec + ((double)tv.tv_usec / 1000000));
}

void setUnblockOption(int sock, char unblock) {
  int opts = fcntl(sock, F_GETFL);
  if (unblock == 1)
    opts |= O_NONBLOCK;
  else
    opts &= ~O_NONBLOCK;
  fcntl(sock, F_SETFL, opts);
}

void sockBlockingOn(int sock) { setUnblockOption(sock, 0); }
void sockBlockingOff(int sock) { setUnblockOption(sock, 1); }


const int bsti = 1;  // Byte swap test integer
bool is_bigendian()
{
bool rc = false;

  rc =  (*(char*)&bsti) == 0;

  return rc;
}


/*************************************************************
*
* Function: void swapbytes(void *_object, size_t size)
* 
* Summary: In-place swapping of bytes to match endianness of hardware
*
* Inputs:
*   *object : memory to swap in-place
*   size   : length in bytes
*           
*
* outputs:  
*     updates caller's object data
*
* notes: 
*    
*   Timeval struct defines the two components as long ints
*         The following nicely printers: 
*         printf("%ld.%06ld\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
*
***************************************************************/
void swapbytes(void *_object, size_t size)
{
  unsigned char *start, *end;
//TODO:  replace with preprocessor macro
  if(!is_bigendian())
  {
    for ( start = (unsigned char *)_object, end = start + size - 1; start < end; ++start, --end )
    {
      unsigned char swap = *start;
      *start = *end;
      *end = swap;
    }
   }
}

/*************************************************************
*
* Function: uint64_t htonll (uint64_t InAddr) 
* 
* Summary:  equivalent to htonl but operates on long long which
*           is assumed to be uint64_t 
*
* Inputs:
*   uint64_t InAddr :  callers 64 bit data
*           
*
* outputs:  
*   returns the InAddr in network byte (Big Endian) format
*
* notes: Returns all 1's on error
*    
***************************************************************/
uint64_t htonll (uint64_t InAddr) 
{
  uint64_t rvalue = InAddr;
//  swapbytes((void *)(&rvalue),size(uint64_t));

//TODO:  replace with preprocessor macro
  if(!is_bigendian())
  {
    rvalue = htobe64(rvalue);
  }
  return rvalue;

}

/***********************************************************
* Function: double getCurTimeD() 
*
* Explanation:  This returns the wall time using 
*               CLOCK_REALTIME as the clock source.
*               Should do the same as timestamp() or
*                wallClockTime(), but possibly with
*               more precision.
*
* inputs: none
*
* outputs:
*    returns the wall time as a double representing 
*     the wall clock time  in seconds  with nanosecond precision
*
* notes: 
*     TAG WALLCLOCK
*
***********************************************************/
double getCurTimeD() 
{
  double timestamp = -1.0;
  struct timespec ts;
  int rc = NOERROR;

  rc = clock_gettime(CLOCK_REALTIME, &ts);

  if (rc==NOERROR) { 
      timestamp = ( (double)ts.tv_sec +  (double) (((double)ts.tv_nsec)/1000000000) );
  } else {
    perror("getCurTimeD:  HARD error on clock_gettime\n");
  }

  return(timestamp);
}


/***********************************************************
* Function: double getCurTime(struct timespec *ts) 
*
* Explanation:  This returns the wall time using 
*               CLOCK_REALTIME as the clock source.
*
* inputs: 
*       struct timespec *ts : callers timespec that is to be filed in.
*
* outputs:
*    returns the wall time in seconds  with nanosecond precision
*
* notes: 
*     TAG WALLCLOCK 
*
***********************************************************/
double getCurTime(struct timespec *ts) 
{
  double timestamp = -1.0;
  int rc = NOERROR;

  //likely to use CLOCK_REALTIME 
  rc = clock_gettime(CLOCK_REALTIME, ts);

  if (rc==NOERROR) { 
      timestamp = ( (double)ts->tv_sec +  (double) (((double)ts->tv_nsec)/1000000000) );
  } else {
    printf("getCurTime:  HARD error on clock_gettime,  errno:%d \n", errno);
    perror("getCurTimeD:  HARD error on clock_gettime\n");
  }

  return(timestamp);

}


/***********************************************************
* Function:  double getTimestamp(struct timespec *ts) 
*
* Explanation:  This returns  a timestamp using 
*             the setting of the variable defaultTimestampClockSource
*
* inputs: 
*      struct timespec *ts : callers timespec that will be filled in
*
* outputs:
*        returns a timestamp in seconds  with nanosecond precision
*        a return of -1 indicates an  ERROR.
*
* notes: 
*
*     TAG TIMESTAMP  NANOSECONDS
***********************************************************/
double getTimestamp(struct timespec *ts) 
{
  double timestamp = -1.0;
  int rc = NOERROR;

  rc = clock_gettime(CLOCK_MONOTONIC_RAW, ts);
  if (rc==NOERROR) { 
      timestamp = (double)ts->tv_sec + ((double)ts->tv_nsec)/1000000000;
  } else {
    perror("getTimestamp:  HARD error on clock_gettime\n");
    rc = ERROR;
  }

  if (rc == ERROR)
    timestamp = DOUBLE_ERROR;

  return(timestamp);

}


/***********************************************************
* Function:  double getTimestampD() 
*
* Explanation:  This returns  a timestamp using 
*               the MONOTONIC_RAW clock source
*               Same as getTimestamp but without the ts param               
*
* inputs: 
*
* outputs:
*     returns a double representing a timestamp in seconds  with nanosecond precision
*
* notes: 
*
*     TAG TIMESTAMP  NANOSECONDS
***********************************************************/
double getTimestampD() 
{
  double timestamp = -1.0;
  struct timespec ts;
  int rc = NOERROR;

  rc = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
//TODO:  to try to improve precision do math in uint64_t then convert to double
  if (rc==NOERROR) { 
      timestamp = (double)ts.tv_sec + ((double)ts.tv_nsec)/1000000000;
  } else {
    perror("getTimestampD():  HARD error on clock_gettime\n");
    rc = ERROR;
  }

  if (rc == ERROR)
    timestamp = DOUBLE_ERROR;

  return(timestamp);

}


double getTime(int clockType) {

  clockid_t clockLinuxType = CLOCK_REALTIME;
  struct timeval myTime;
  struct timespec curTime;
  int rc;

  switch(clockType) {
    case 1:
	rc= gettimeofday (&myTime, (struct timezone *) NULL);
        if (rc==0) 
	  return (((((double) myTime.tv_sec) * 1000000.0) 
             + (double) myTime.tv_usec) / 1000000.0); 
        else{
         printf("getTime: Error on gettimeofday:%d, errno:%d (clockType:%d) \n",
           rc,errno,clockType);
          return(0.0);
        }

    break;

    case 2:
      clockLinuxType = CLOCK_REALTIME;
      //Use clock_gettime
      rc =clock_gettime(clockLinuxType, &curTime);
      if (rc==0) { 
        //return (1000000000 * (double)curTime.tv_sec + (double)curTime.tv_nsec);
        return ((double)curTime.tv_sec + ((double)curTime.tv_nsec)/1000000000);
      }
      else{
        printf("getTime: Error on clock_gettime:%d, errno:%d (clockType:%d) \n",
          rc,errno,clockType);
        return(0.0);
      }
    break;

    case 3:
      clockLinuxType = CLOCK_PROCESS_CPUTIME_ID;
      //Use clock_gettime
      rc =clock_gettime(clockLinuxType, &curTime);
      if (rc==0) { 
        //return (1000000000 * (double)curTime.tv_sec + (double)curTime.tv_nsec);
        return ((double)curTime.tv_sec + ((double)curTime.tv_nsec)/1000000000);
      }
      else{
        printf("getTime: Error on clock_gettime:%d, errno:%d (clockType:%d) \n",
          rc,errno,clockType);
        return(0.0);
      }
    break;

    default:
      printf("Error on clockType: %d \n",clockType);
      return(0.0);
  }

}

double getTime1()
{
	struct timeval curTime;
	(void) gettimeofday (&curTime, (struct timezone *) NULL);
	return (((((double) curTime.tv_sec) * 1000000.0) 
             + (double) curTime.tv_usec) / 1000000.0); 
}


int gettimeofday_benchmark()
{
  int i;
  struct timespec tv_start, tv_end;
  struct timeval tv_tmp;
  int count = 1 * 1000 * 1000 * 50;
  clockid_t clockid;
        
  int rv = clock_getcpuclockid(0, &clockid);

  if (rv) {
    perror("clock_getcpuclockid");
    return 1;
  }

  clock_gettime(clockid, &tv_start);

  for(i = 0; i < count; i++)
      gettimeofday(&tv_tmp, NULL);

  clock_gettime(clockid, &tv_end);

  long long diff = (long long)(tv_end.tv_sec - tv_start.tv_sec)*(1*1000*1000*1000);
  diff += (tv_end.tv_nsec - tv_start.tv_nsec);

  printf("%d cycles in %lld ns = %f ns/cycle\n", count, diff, (double)diff / (double)count);

  return 0;

}


int
delay(int64_t ns)
{
    struct timespec req, rem;

    req.tv_sec = 0;

    while (ns >= 1000000000L) {
        ns -= 1000000000L;
        req.tv_sec += 1;
    }

    req.tv_nsec = ns;

    while (nanosleep(&req, &rem) == -1)
        if (EINTR == errno)
            memcpy(&req, &rem, sizeof(rem));
        else
            return -1;
    return 0;
}

/*************************************************************
* Function: int busyWait (double delayTime)
* 
* Summary: Delays (busy waits) until the clock_getttime CLOCK_MONOTONIC_RAW is reached 
*          Unlike other delay routines, this is passed a specific counter value 
*
*          WARNING: this routine is deceiving! The time to wait is
*                   actually a future value of CLOCK_MONOTONIC_RAW
*                   The caller must know this!!
*
* Inputs: 
*    double delayTime: caller's desired unblock time based on CLOCK_MONOTONIC_RAW
*
* outputs:  
*   returns  ERROR or  NOERROR;
*   
* Notes:   
* 
* Example calling code  
*
*  iterationDelay = ((double)delay)/1000000000.0;
*  reqDelay.tv_sec = (uint32_t)floor(iterationDelay);
*  if (reqDelay.tv_sec >= 1)
*    reqDelay.tv_nsec = (uint32_t)( 1000000000 * (iterationDelay - (double)reqDelay.tv_sec));
*  else
*    reqDelay.tv_nsec = (uint32_t) (1000000000 * iterationDelay);
*
*    if (iterationDelay > 0.000000010)
*      rc = busyWait(iterationDelay);
*    else
*      rc = NOERROR;
*
************************************************/
int busyWait (double delayTime)
{
  int rc = NOERROR;
  struct timespec TS1, TS2;

  rc =  convertD2TS(&delayTime,&TS2);
  while (1) {
    rc = clock_gettime(CLOCK_MONOTONIC_RAW, &TS1);
    //break when this is true 
    if (isTS1GTTS2(&TS1, &TS2)) 
      break;
  }
  rc = NOERROR;
  return rc;
}


/***********************************************************
* Function:  double convertTS2D(struct timespec *t) 
*
* Explanation:  This converts the timestamp from 
*             a timespec into a double
*
* inputs: 
*  struct timespec *callersTime - the caller's ptr to the timestamp
*
* outputs:
*        returns the timestamp as a double in units of seconds with nsec precision
*
* notes: 
*
***********************************************************/
double convertTS2D(struct timespec *t) {
  return ( t->tv_sec + ( (double) t->tv_nsec)/1000000000 );
}


/***********************************************************
* Function: int convertD2TS(double *timeDouble,  struct timespec *ts) 
*
* Explanation:  This converts the timestamp from a double to a timespec
*
* inputs: 
*    double *timeDouble: ptr to callers time in double format
*    struct timespec *timeTSpec : ptr to callers timespec that 
*             is to be filled in
*
*  struct timespec *callersTime - the caller's ptr to the timestamp
*
* outputs:
*        returns ERROR or NOERROR
*
* notes: 
*
***********************************************************/
int convertD2TS(double *timeDouble,  struct timespec *ts) 
{
  uint64_t delaySecs = 0;
  uint64_t delayNsecs = 0;
  int rc = NOERROR;

  if (*timeDouble >= 1.0)
    delaySecs =  (uint64_t)floor(*timeDouble);

  delayNsecs = (uint64_t)( 1000000000 * (*timeDouble - (double) delaySecs)); 

#ifdef TRACEME 
  printf("convertD2TS:  time:%f  delaySecs:%"PRIu64" delayNsecs:%"PRIu64" \n",
		  *timeDouble, delaySecs, delayNsecs);
#endif
  
  ts->tv_sec = delaySecs;
  ts->tv_nsec = delayNsecs;


  return rc;

}

/*************************************************************
* Function: bool isTS1GTTS2(struct timespec *TS1, struct timespec *TS2) 
* 
* Summary: returns true if the first timestamp is greater than the second  TS
*
* Inputs: 
*     struct timespec *TS1: first timestamp
*     struct timespec *TS2: second timestapm
*
* outputs:  
*   returns  true if TS1>TS2.  returns false if TS1<=TS2.
************************************************/
bool isTS1GTTS2(struct timespec *TS1, struct timespec *TS2) 
{
  bool rc = false;
  if (TS1->tv_sec > TS2->tv_sec || ((TS1->tv_sec == TS2->tv_sec) && (TS1->tv_nsec > TS2->tv_nsec))  ) {
    rc = true;
  }
  return rc;
}



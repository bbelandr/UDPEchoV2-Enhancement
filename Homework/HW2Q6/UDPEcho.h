/*********************************************************
*
* Module Name: UDP Echo client/server header file
*
* File Name:    UDPEcho.h	
*
* Summary:
*  This file contains common stuff for the client and server
*
* Revisions:
* 
* $A1: 4/2/25: Added updatedMessageHeader 
*
* Last Update: 4/2/2025
*
*********************************************************/
#ifndef	__UDPEcho_h
#define	__UDPEcho_h
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>  /*brings in C99 types of uint32_t uint64_t  ...*/
#include <inttypes.h>
#include <limits.h>  /*brings in limits such as LONG_MIN LLONG_MAX ... */
#include <math.h>    /* floor, ... */


#include <string.h>     
#include <errno.h>

#include <stdbool.h>

#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <netinet/in.h> /* for in_addr */
#include <arpa/inet.h> /* for inet_addr ... */

#include <unistd.h>     /* for close() */
#include <fcntl.h>
#include <netdb.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>


// Handle error with user msg
void DieWithUserMessage(const char *msg, const char *detail);

// Handle error with sys msg
void DieWithSystemMessage(const char *msg);

enum sizeConstants {
  MAXSTRINGLENGTH = 128,
  BUFSIZE = 512,
};

//Same as UINT_MAX 
//#define MAX_UINT32  4294967295U
#define MAX_UINT32  UINT32_MAX

//same as ULLONG_MAX on any machine  (32 or 64 bit machine)
//  = 0xffffffffffffffff 
//#define MAX_UINT64  18446744073709551615UL
#define MAX_UINT64  UINT64_MAX

//possible reasonCodes for exiting  

//defines max mesg size - needs to be a bit larger than possible max message
#define MAX_DATA_BUFFER 50028
//defines min mesg size - we'll assume can be as small as 4 bytes
#define MIN_DATA_BUFFER 4

//The following are the min/max amount of USER data supported in a single sendto/receive (i.e., UDP only!!)
//It does NOT include the overhead data needed / used by our message header (or any TCP/IP/Frame headers)

//Min must hold at lesat the messageHeader
#define MESSAGEMIN 12
#define MESSAGE_DEFAULT_SIZE 24
//Note: for UDP, this will cause frag. although if using localhost the mtu is usually >60Kbytes
#define MESSAGEMAX 50000

#define MAX_MSG_HDR 128


#define ECHOMAX 10000     /* Longest string to echo */
#define ERROR_LIMIT 5


typedef struct {
  uint32_t sequenceNum;
  uint32_t timeSentSeconds;
  uint32_t timeSentNanoSeconds;
} messageHeaderDefault;


//For the EMWA - 
#define ALPHA 0.125

//Choices of opModes 
#define opModeRTT 0
#define opModeOWD 1
//$A1
typedef struct {
  uint32_t sequenceNum;
  uint32_t timeSentSeconds;
  uint32_t timeSentNanoSeconds;
  uint16_t opMode;
} __attribute__((packed)) updatedMessageHeader;


#ifndef LINUX
#define INADDR_NONE 0xffffffff
#endif


//The possible tool modes of operation  
#define PING_MODE 0      //server ack's each arrival (subject to its AckStrategy param)
#define LIMITED_RTT 1    //server ack's each arrival (subject to its AckStrategy param)
                         // client only maintains a single RTT sample active at a time
#define ONE_WAY_MODE  2  // server does not issue an ack


//Definition, FALSE is 0,  TRUE is anything other
#define TRUE 1
#define FALSE 0

#define VALID 1
#define NOTVALID 0

//Defines max size temp buffer that any object might create
#define MAX_TMP_BUFFER 1024

/*
  Consistent with C++, use EXIT_SUCCESS, SUCCESS is a 0, otherwise  EXIT_FAILURE  is not 0

  If the method returns a valid rc, EXIT_FAILURE is <0
   Should use bool when the method has a context appropriate for returning T/F.   
*/
#define SUCCESS 0
#define NOERROR 0

#define ERROR   -1
#define FAILURE -1
#define FAILED -1


#define CHAR_ERROR 0xFF
#define DOUBLE_ERROR  -1.0


#endif


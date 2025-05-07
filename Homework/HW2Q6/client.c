/*********************************************************
* Module Name: UDPEchoV2   client  
*
* File Name:    client.c
*
* Summary:
*  This file contains the client portion of a client/server
*    UDP-based performance tool.
*  
* Params:
*  char *  server = argv[1];
*  uint32_t  serverPort = atoi(argv[2]);
*  double  iterationDelay atof(argv[3]);
*  uint32_t messageSize = atoi(argv[4]);
*             NOTE:  The messageSize is inclusive of the size of the application message header.  
*  uin32_t nIterations = atoi(argv[5]);
*  uint16_t opMode = (uint16_t)atoi(argv[6]);
*  double  sendRate = atof(argv[7])
*  char *outputFile = atoi(argv[8]);
*
*  Usage :   client
*             <Server IP>
*             <Server Port>
*             [<Iteration Delay (secs.nano)>]
*             [<Message Size (bytes)>]
*             [<# of iterations>]
*             <opMode>
*             <sendRate>
*             <outputFile>
*
* outputs:  
*    The per iteration information printed to stdout:
*      printf("%f %4.9f %4.9f %d %d\n", 
*             wallTime, RTTSample, smoothedRTT, 
*             receivedCount,  numberRTTSamples);
*
*       wallTime:  The time
*       RTTSample:  the sample based on the current message arrival
*       smoothedRTT: computes a smoothed RTT average using a weighted filter
*       numberRTTSamples: The number of samples received
*
* $A1: 4/1/2025  minor cleanup 
*               Updates for HW2 Q6. 
*        -Add two new params:  opMode and outputFile
*        -Use new updatedMessageHeader 
*
* $A2: 4/3/25 :  Finished the updates
*     
* Last update: 4/3/2025
*
*********************************************************/
#include "UDPEcho.h"
#include "AddressHelper.h"
#include "utils.h"

void myUsage();
void clientCNTCCode();
void CatchAlarm(int ignored);

extern char Version[];

//uncomment to see debug trace
//#define TRACEME 1


//Define this globally so our async handlers can access

char *server = NULL;                   /* IP address of server */
int sock = -1;                         /* Socket descriptor */
double startTime = 0.0;
double endTime = 0.0;

//$A2
FILE *outputFID = NULL;
char *outputFile = NULL;
bool doSampleOutput=false;
uint16_t opMode = opModeRTT;
uint16_t RxedOpMode = opModeRTT;

uint32_t totalPacketsRxed = 0;
uint32_t totalPacketsSent = 0;
uint64_t totalBytesSent = 0;
double timeOfFirstTxedMsg = -1.0;
double timeOfLastTxedMsg = -1.0;

//Stats and counters - these are global as the asynchronous handlers need access
uint32_t numberOfTrials=0; /*counts number of attempts */
uint32_t numberLost=0;
uint32_t numberOutOfOrder=0;
uint32_t numberTOs=0;
//incremented by the rx seq number - nest rx seq number 
uint32_t totalGaps=0;

uint32_t TxErrorCount=0;
uint32_t RxErrorCount=0;
uint32_t largestSeqRecv = 0;
uint32_t receivedCount = 0;


double RTTSum = 0.0;
uint32_t numberRTTSamples=0;

//Maintains current wall clock time
double wallTime = 0.0;

//Prior to sendto, records the current wall clock time
double lastMsgTxWallTime = 0.0;
struct timespec msgTxTime;    



static const unsigned int TIMEOUT_SECS = 2; // Seconds between retransmits

void myUsage()
{
  printf("UDPEchoV2:client(v%s): <Server IP> <Server Port> <Iteration Delay(secs.nanos)> <Message Size (bytes)>] <# of iterations> <opMode> <sendRate> <outFile> \n", Version);
  printf(" ---> nIterations: 0 is forever \n");
  printf(" ---> opMode: 0:RTT Mode,  1: OWD Mode \n");
}


/*************************************************************
*
* Function: Main program for  UDPEcho client
*           
****************************************************************/

int main(int argc, char *argv[]) 
{
  int32_t rc = NOERROR;
   uint16_t txMarker = 0x5555;

  //uint32_t delay=0;

  double   iterationDelay = 0.0;
  double   sendRate = 0.0;
  int32_t  messageSize=0;

  int32_t  RxedMsgSize=0;
  int32_t nIterations=  -1;
  char *service = NULL;


  struct addrinfo addrCriteria;         // Criteria for address
  struct addrinfo *servAddr = NULL;;    // Holder for returned list of server addrs

  int rtnVal = 0;
  int sock = -1;
  struct sigaction handler; // Signal handler
  ssize_t numBytes = 0;
  struct sockaddr_storage fromAddr; // Source address of server
  socklen_t fromAddrLen = 0;
  char *TxBuffer = NULL;
  char *RxBuffer = NULL;
  //Used to help pack and unpack the network buffer
  //A2
  uint16_t *TxShortPtr  = NULL;
  uint16_t *RxShortPtr  = NULL;
  uint32_t *TxIntPtr  = NULL;
  uint32_t *RxIntPtr  = NULL;
  bool loopForever=false;
  bool loopFlag=true;
  uint32_t sequenceNumber=0;
  struct timespec reqDelay;
  struct timespec remDelay;

  //Used for the RTT sample
  double  Tstart = 0.0;
  double  Tstop = 0.0;
  //most recent RTT sample
  double RTTSample = 0.0;
  //smoothed avg
  double smoothedRTT = 0.0;
  double alpha = ALPHA;
  //double alpha = 0.10;
  updatedMessageHeader *TxHeaderPtr=NULL;
  updatedMessageHeader *RxHeaderPtr=NULL;
//$A1
//  messageHeaderDefault *TxHeaderPtr=NULL;
//  messageHeaderDefault *RxHeaderPtr=NULL;
  uint32_t count = 0;

  int32_t msgHeaderSize = sizeof(updatedMessageHeader);
  //int32_t msgHeaderSize = sizeof(messageHeaderDefault);

  //used for the busy wait
  //Used to track intervals for each Tx
  struct timespec TSstartTS; //accurate TS clock
  double TSstartD=0.0;
  double nextWakeUpTimeD=0.0;


//The 8th param is optional
  if (argc < 7)    /* need at least server name and port */
  {
    printf("UDPEchoV2: %d params entered, requires 8 \n",argc);
    myUsage();
    exit(1);
  }

  wallTime = getCurTimeD();
  startTime = wallTime;



  //set defaults
 // delay=1000000;
  messageSize = 56;
  nIterations=0;

  server = argv[1];     // First arg: server address/name
  service = argv[2];

//Delay in microseconds
//  delay = atoi(argv[3]);
  iterationDelay = atof(argv[3]);

//messageSize in bytes
  messageSize= atoi(argv[4]);
  if (messageSize > MAX_DATA_BUFFER)
    messageSize = MAX_DATA_BUFFER;

  nIterations = atoi(argv[5]);
  if (nIterations == 0)
    loopForever=true;

//A2
  opMode = (uint16_t)atoi(argv[6]);

  if (argc == 8)  
  {
    sendRate = (double)atof(argv[7]);
  } 


  if (argc == 9)  
  {
    outputFile = argv[8];
    doSampleOutput=true;
  }
  else
  {
    doSampleOutput=false;
  }

  if (outputFile != NULL) {
    printf("client: opMode:%d  outputFile:%s \n", opMode, outputFile);
  } else {
    printf("client: opMode:%d  outputFile NOT entered! \n", opMode);
  }


  signal (SIGINT, clientCNTCCode);


  //If sendRate is not passed, we compute
  //it based on the params iterationDelay and messageSize
  if (sendRate == 0.0) 
  {
    //Neither msgSize or iteration delay can be 0
    if ( (messageSize == 0) || (iterationDelay == 0.0) )
    {
      printf("client: HARD ERROR:   sendRate:%12.0f messageSize:%d iterationDelay:%4.12f  \n", sendRate, messageSize, iterationDelay);
      exit(1);
    }
    sendRate =  ( (double)messageSize*8.0) / iterationDelay; 
    printf("client: (WAS 0): sendRate:%12.0f bps messageSize:%d bytes iterationDelay:%4.12f secs  \n", sendRate, messageSize, iterationDelay);
    // If it is passed, either the iterationDelay or messageSize
    //  must be 0.   We set the zero based on the sendrate and other param
  }
  else 
  {
    //Either msgSize is 0 or iterationDelay ...
    if (messageSize == 0) {
      messageSize = (int32_t) (iterationDelay  / sendRate); 
    }
    if (iterationDelay  == 0.0) {
      iterationDelay =  ( (double) messageSize * 8.0) / sendRate;
    }
    printf("client: (NOT 0): sendRate:%12.0f messageSize:%d iterationDelay:%f  \n", sendRate, messageSize,  iterationDelay);
  }
  


  reqDelay.tv_sec = (uint32_t)floor(iterationDelay);
  if (reqDelay.tv_sec >= 1)
    reqDelay.tv_nsec = (uint32_t)( 1000000000 * (iterationDelay - (double)reqDelay.tv_sec));
  else
    reqDelay.tv_nsec = (uint32_t) (1000000000 * iterationDelay);


//#ifdef TRACEME
  printf("client: server:%s  service:%s sendRate:%12.0f bps messageSize:%d bytes iterationDelay:%4.12f secs  msgHeaderSize:%d  \n", 
        server,service, sendRate, messageSize,  iterationDelay, msgHeaderSize);
  printf("client:   reqDelay:.tv_sec: %ld secs  tv_nsec:%ld nanosecs \n", reqDelay.tv_sec, reqDelay.tv_nsec);
//#endif


  //for nanosleep
  remDelay.tv_sec = 0;
  remDelay.tv_nsec = 0;


//First seq num is 1
  sequenceNumber++;


  //Init memory for first send
  TxBuffer = malloc((size_t)messageSize);
  if (TxBuffer == NULL) {
    printf("client: HARD ERROR malloc of Tx  %d bytes failed \n", messageSize);
    exit(1);
  }
  memset(TxBuffer, 0, messageSize);
  //This pointer is used when packing the header into the network buffer
  TxIntPtr  = (uint32_t *) TxBuffer;

  //A1
  //messageHeaderDefault TxHeader;
  updatedMessageHeader TxHeader;
  TxHeaderPtr=&TxHeader;


//typedef struct {
//  uint32_t sequenceNum;
//  uint32_t timeSentSeconds;
//  uint32_t timeSentNanoSeconds;
//  uint16_t opMode; 
//} __attribute__((packed)) updatedMessageHeader;

  //Init memory for receive 
  RxBuffer = malloc((size_t)messageSize);
  if (RxBuffer == NULL) {
    printf("client: HARD ERROR malloc of Rx %d bytes failed \n", messageSize);
    exit(1);
  }
  memset(RxBuffer, 0, messageSize);

  //$A1
  //messageHeaderDefault RxHeader;
  updatedMessageHeader RxHeader;
  RxHeaderPtr=&RxHeader;

  //These pointers are  used when packing the header fieldsss  into the network buffer
  RxIntPtr  = (uint32_t *) RxBuffer;


  // Tell the system what kind(s) of address info we want
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  addrCriteria.ai_socktype = SOCK_DGRAM;          // Only datagram sockets
  addrCriteria.ai_protocol = IPPROTO_UDP;         // Only UDP protocol

  // Get address(es)
  rtnVal = getaddrinfo(server, service, &addrCriteria, &servAddr);
  if (rtnVal != 0) {
    printf("client: failed getaddrinfo, %s \n", gai_strerror(rtnVal));
  }

#if 0

       int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res);

 struct addrinfo {
               int              ai_flags;
               int              ai_family;
               int              ai_socktype;
               int              ai_protocol;
               socklen_t        ai_addrlen;
               struct sockaddr *ai_addr;
               char            *ai_canonname;
               struct addrinfo *ai_next;
           };

#endif

  //servAddr holds list of addrinfo's
  // Display returned addresses
  count=0;
  // Could use this to either obtain this hosts
  // IP or to confirm the IP of the server's respone in RTT mode
  //struct sockaddr_storage clntAddr; // Client address

  struct addrinfo *addr = NULL;
  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    count++;
    PrintSocketAddress(addr->ai_addr, stdout);
    fputc('\n', stdout);
  }
  printf("client: found %d addresses based on name %s \n",count, server);
  struct sockaddr *myIPV6Addr = NULL;
  count = findAF_INET6SocketAddress(servAddr,&myIPV6Addr);
  if (count == 0) {
    printf("Did not find any V6 addresses \n");
  } else{
    printf("client: found %d V6 addresses based on name %s \n",count, server);
    printf("getaddrinfo:  found addresse \n");
    PrintSocketAddress(myIPV6Addr, stdout);
    fputc('\n', stdout);
  }

  struct sockaddr *myIPV4Addr = NULL;
  count = findAF_INETSocketAddress(servAddr,&myIPV4Addr);
  if (count > 0) {
    printf("client: found %d V4 addresses based on name %s \n",count, server);
    PrintSocketAddress(myIPV4Addr, stdout);
    fputc('\n', stdout);
  } else {
    printf("getaddrinfo:  Failed to find V4 addr ???  \n");
  }

  // Create a reliable, stream socket using UDP
  sock = socket(servAddr->ai_family, servAddr->ai_socktype,
      servAddr->ai_protocol); // Socket descriptor for client
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Set signal handler for alarm signal
  handler.sa_handler = CatchAlarm;
  if (sigfillset(&handler.sa_mask) < 0) // Block everything in handler
    DieWithSystemMessage("sigfillset() failed");
  handler.sa_flags = 0;

  if (sigaction(SIGALRM, &handler, 0) < 0)
    DieWithSystemMessage("sigaction() failed for SIGALRM");


  if (doSampleOutput )
  {
    printf("client:  open output file %s \n", outputFile);
    outputFID = fopen(outputFile,"w");
  }


  //Must be an accurate timestamp
  TSstartD = getTimestamp(&TSstartTS);
  nextWakeUpTimeD=TSstartD;
  while (loopFlag)
  {

    lastMsgTxWallTime = getCurTime(&msgTxTime);
    wallTime = lastMsgTxWallTime;

    //Check to make sure we have not looped the seq counter
    if (sequenceNumber == (MAX_UINT32 - 1) )
    {
      //We could wrap counters - better to move to 64 bit counters...
      printf("client: HARD ERROR: Exceeded the sequence number range.... next seqNu:%d \n", sequenceNumber);
      loopFlag = false;
      //exit(1);
    }

    //Update the TxHeader
    TxHeaderPtr->sequenceNum = sequenceNumber++;
    TxHeaderPtr->timeSentSeconds = msgTxTime.tv_sec;
    TxHeaderPtr->timeSentNanoSeconds = msgTxTime.tv_nsec;
    TxHeaderPtr->opMode = opMode;

    //pack the header into the network buffer
    TxIntPtr  = (uint32_t *) TxBuffer;
    *TxIntPtr++  = htonl(TxHeaderPtr->sequenceNum);
    *TxIntPtr++  = htonl(TxHeaderPtr->timeSentSeconds);
    *TxIntPtr++  = htonl(TxHeaderPtr->timeSentNanoSeconds);
    TxShortPtr  = (uint16_t *) TxIntPtr;
    *TxShortPtr++  =  htons(TxHeaderPtr->opMode);
    txMarker = 0x5555;
    //Add a marker  
    *TxShortPtr++  = htons(txMarker);

    rc = NOERROR;
    numberOfTrials++;
    if ( (!loopForever) &&  (numberOfTrials > nIterations) )
    {
         loopFlag=false;
	 //A1
         //clientCNTCCode();
         continue;
         //break;
    }
    Tstart= getTimestampD();

#ifdef TRACEME
    printf("client: send seqNum:%d  opMode:%d \n",  TxHeaderPtr->sequenceNum, TxHeaderPtr->opMode);
#endif

    numBytes = sendto(sock, TxBuffer, messageSize, 0,
      servAddr->ai_addr, servAddr->ai_addrlen);
    if (numBytes < 0) {
        TxErrorCount++;
//#ifdef TRACEME
        perror("client: sendto error \n");
//#endif
        continue;
    }
    else if (numBytes != messageSize){
//#ifdef TRACEME
      printf("client: sendto return %d not equal to messageSize:%d \n", (int32_t) numBytes,messageSize);
//#endif
        continue;
    }

    totalPacketsSent++;
    totalBytesSent+=numBytes;
    timeOfLastTxedMsg = wallTime;
    if (timeOfFirstTxedMsg == -1.0)
    {
      timeOfFirstTxedMsg = wallTime;
    }

    //If opModeRTT -  
    if (opMode == opModeRTT)
    {

      // Receive a response

      // Set length of from address structure (in-out parameter)
      fromAddrLen = sizeof(fromAddr);
      alarm(TIMEOUT_SECS); // Set the timeout

      //returns -1 on error else bytes received
      rc =  recvfrom(sock, RxBuffer, messageSize, 0, (struct sockaddr *) &fromAddr, &fromAddrLen);
      if (rc == ERROR) 
      {
        if (errno == EINTR) {     // Alarm went off
//#ifdef TRACEME
          printf("client: recvfrom error EINTR, numberTOs:%d \n",numberTOs);
//#endif
          rc = NOERROR;
          continue;
        } else {
          RxErrorCount++;
//#ifdef TRACEME
          perror("client: recvfrom other error \n");
//#endif
        }
      } else 
      {
        //succeeded!
        totalPacketsRxed++;
        numBytes=rc;
        RxedMsgSize = rc;
        alarm(0);
        if (RxedMsgSize != messageSize)
        {
          printf("client: HARD ERROR, opModeRTT but did not receive a valid msg?  RxedMsgSize:%d  messageSize:%d \n", RxedMsgSize, messageSize);
        }
 //Obtain RTT sample
        Tstop =  getTimestampD();
        RTTSample= Tstop - Tstart;
        RTTSum += RTTSample;
        numberRTTSamples++;
        //Init the filter
        if (numberRTTSamples == 1) {
          smoothedRTT = RTTSample;
        } else {
          smoothedRTT = alpha*RTTSample + (1-alpha)*smoothedRTT;
        }
        rc = NOERROR;
        receivedCount++;
        wallTime = getCurTimeD();

        RxIntPtr  = (uint32_t *) RxBuffer;
        RxHeaderPtr->sequenceNum =  ntohl(*RxIntPtr++);
        RxHeaderPtr->timeSentSeconds =  ntohl(*RxIntPtr++);
        RxHeaderPtr->timeSentNanoSeconds  =  ntohl(*RxIntPtr++);

        RxShortPtr  = (uint16_t *)RxIntPtr;
        RxHeaderPtr->opMode = ntohs(*RxShortPtr++);
        RxedOpMode = RxHeaderPtr->opMode;

#ifdef TRACEME
        printf("%f %d %d %4.9f %4.9f %d %d\n", wallTime, (int32_t)RxedOpMode, RxedMsgSize, RTTSample, smoothedRTT, receivedCount,  numberRTTSamples);
#endif

        if (doSampleOutput)
        {
          fprintf(outputFID, "%f %d %d %4.9f %4.9f %d %d\n", wallTime, (int32_t)RxedOpMode, RxedMsgSize, RTTSample, smoothedRTT, receivedCount,  numberRTTSamples);
        }



#ifdef TRACEME
        printf("client: succeeded to recv %d bytes from server \n", (int) numBytes);
        printf("Rxed: %d %d %d %d \n", 
             RxHeaderPtr->sequenceNum, (int32_t)RxHeaderPtr->opMode,
             RxHeaderPtr->timeSentSeconds, RxHeaderPtr->timeSentNanoSeconds);
#endif
      }  //else succeeded to recvfrom

    }

    //delay requested amount

    //This busyWaits until time delayTime (based on clock_gettime with CLOCK_MONOTONIC)
    nextWakeUpTimeD += iterationDelay;
    rc = busyWait(nextWakeUpTimeD);

    //rc = nanosleep((const struct timespec*)&reqDelay, &remDelay);

  }  //while loopFlag true

  //Send a message to the server so it can exit...
  // Will be a reduced size: 16 octets
  int32_t reducedControlMsgSize=16;

  lastMsgTxWallTime = getCurTime(&msgTxTime);
  wallTime = lastMsgTxWallTime;

  TxHeaderPtr->sequenceNum = MAX_UINT32;
  TxHeaderPtr->timeSentSeconds = msgTxTime.tv_sec;
  TxHeaderPtr->timeSentNanoSeconds = msgTxTime.tv_nsec;
  TxHeaderPtr->opMode = opMode;

  //pack the header into the network buffer
  TxIntPtr  = (uint32_t *) TxBuffer;
  *TxIntPtr++  = htonl(TxHeaderPtr->sequenceNum);
  *TxIntPtr++  = htonl(TxHeaderPtr->timeSentSeconds);
  *TxIntPtr++  = htonl(TxHeaderPtr->timeSentNanoSeconds);
  TxShortPtr  = (uint16_t *) TxIntPtr;
  *TxShortPtr++  =  htons(TxHeaderPtr->opMode);
  //Add a marker  

  txMarker = 0x0102;

  *TxShortPtr++  = htons(txMarker);


//#ifdef TRACEME
  printf("client: SENDING the terminate signal to the server, reducedSizeMsg:%d with marker:%0x  \n", reducedControlMsgSize, txMarker);
//#endif

  //Sleep for 1 second ... this allows congestion to dissipate reducing the chance the terminate signal gets dropped
  useconds_t sleepTime = 1000000;
  (void) usleep(sleepTime);

  numBytes = sendto(sock, TxBuffer, reducedControlMsgSize, 0,
      servAddr->ai_addr, servAddr->ai_addrlen);
  if (numBytes < reducedControlMsgSize) {
    printf("client: HARD ERROR:   terminate sendto wrong size?  %d \n", (int32_t) numBytes);
  }
  close(sock);

  printf("client:  Now call CNTC Code to compute stats ... \n");
  clientCNTCCode();

  exit(0);
}

// Handler for SIGALRM
void CatchAlarm(int ignored) {
  numberTOs++;
}


void clientCNTCCode() 
{
  double avgRTT  = 0.0;
  double avgLossRate  = 0.0;
  uint32_t totalLost = 0;
  double duration = 0.0;
  double avgSendrate = 0.0;

  wallTime = getCurTimeD();
  endTime = wallTime;
  duration = timeOfLastTxedMsg - timeOfFirstTxedMsg;


  if (duration > 0.0) 
  {
    avgSendrate = ( (double)totalBytesSent * 8.0) / duration;
  }

  if (opMode == opModeRTT)
  {
    if (numberRTTSamples > 0) {
       avgRTT = RTTSum /  (double)numberRTTSamples;
    } 


    totalLost=numberTOs;
    if (totalPacketsSent  >  0) {
      avgLossRate = ((double)totalLost) / (double)totalPacketsSent;
    }

  }

  printf("wallTime duration avgRTT avgSendrate avgLossRate numberRTTSamples totalLost totalPacketsSent \n");
  printf("%12.6f %6.6f %4.9f %12.0f %2.4f %d %d %d \n",
          wallTime, duration, avgRTT, avgSendrate, avgLossRate, numberRTTSamples,totalLost,totalPacketsSent);

  if (doSampleOutput )
  {
    fprintf(outputFID,"%12.6f %6.6f %4.9f %12.0f %2.4f %d %d %d \n",
          wallTime, duration, avgRTT, avgSendrate, avgLossRate, numberRTTSamples,totalLost,totalPacketsSent);
    fclose(outputFID);
  }

  exit(0);
}






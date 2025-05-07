/*********************************************************
* Module Name:  UDPEchoV2 server
*
* File Name:    server.c	
*
* Summary:
*  This file contains the client portion of a client/server
*    UDP-based performance tool.
*  
* Usage:
*     server <service> 
*
* Output:
*  Per iteration output: 
*       printf("%f %d %d %d %d.%d %3.9f %3.9f\n", wallTime, (int32_t) numBytesRcvd,
*             largestSeqRecv,
*             msgHeaderPtr->sequenceNum,
*             msgHeaderPtr->timeSentSeconds,
*             msgHeaderPtr->timeSentNanoSeconds, OWDSample, smoothedOWD);
*
*  End of program summary info: 
*
*  printf("UDPEchoV2:Server:Summary:  %12.6f %6.6f %4.9f %2.4f %d %d %d %6.0f %d %d %d\n",
*        wallTime, duration, avgOWD, avgLossRate, numberOfTrials, receivedCount, largestSeqRecv, totalLost,
*        RxErrorCount, TxErrorCount, numberOutOfOrder);
*
*  
* A1: 3/12/2025:  Prepping to add support for opMode 1    CBR behavior....NO ECHO!
*                 Fixed iteration count off by 1,  cleaned up output a bit
*                 
* A2: 4/2/25:  Add outputFile param and support for opMode 1
*              Switched to updatedMessageHeader format
*
* A3: 4/4/25   faster samples - see  CREATESAMPLESARRAY
*              monitor/collect mean burst loss outcomes  - avgGap is avg loss run length over all loss events 
*              Append single summary result to serverResults.dat 
*              Responds to client's terminate signal
*
* Last updated: 4/4/2025
*
*********************************************************/
#include "UDPEcho.h"
#include "AddressHelper.h"
#include "utils.h"

void CatchAlarm(int ignored);
void CNTCCode();

int sock = -1;                         /* Socket descriptor */
int bStop = 1;;

double timeOfFirstRxedMsg = -1.0;
double timeOfLastRxedMsg = -1.0;

double startTime = 0.0;
double endTime = 0.0;
double  wallTime = 0.0;
uint32_t largestSeqRecv = 0;
uint64_t receivedCount = 0;
uint32_t RxErrorCount = 0;
uint32_t TxErrorCount = 0;
uint32_t  numberOutOfOrder=0;

uint32_t lastSeqNumber=0;
uint32_t curSeqNumber=0;
int32_t numberOfGaps=0;
int32_t sumOfAllGaps=0;
int32_t sizeCurGap=0;
int32_t thisGap=0;

//If defined, we record the size of each gap event
//    A gap event is a loss event involving >0
//    consecurtively lost packets 
#define CREATEGAPARRAY 1 

#ifdef CREATEGAPARRAY
#define MAX_GAPS 128000
uint32_t gapArrayIndex = 0;
uint32_t *gapArraySize=NULL;
uint32_t *gapArraySeqNo=NULL;
double *gapArrayTS=NULL;
FILE *gapArrayFID =  NULL;
char *gapArrayFile = "gapArray.dat";
#endif

double OWDSum = 0.0;
uint32_t numberOWDSamples=0;
double maxOWDSample = 0.0;
double minOWDSample = 10000.0;


//If defined, we record samples but in a manner that does not
// cause file I/O until the end of the program
#define CREATESAMPLEARRAYS 1

//After this number, we roll over ....
#define MAX_SAMPLES 1024000

#ifdef CREATESAMPLEARRAYS 
int32_t sampleArrayIndex = 0;
double *OWDSampleArrayTS=NULL;
double *OWDSampleArray=NULL;
uint32_t *seqNoArray=NULL;
FILE *samplesArrayFID = NULL;
char *samplesArrayFile = "serverSamplesArray.dat";
#endif

//$A2
FILE *resultsFID = NULL;
char *resultsOutputFile = "serverResults.dat";

FILE *outputFID = NULL;
char *outputFile = NULL;
bool doSampleOutput = false;
uint16_t RxedOpMode = opModeRTT;
uint16_t rxMarker=0; 

uint64_t totalBytesRxed = 0;
uint32_t numberNegativeOWDSamples=0;

//uncomment to see debug output
//#define TRACE 1

int main(int argc, char *argv[]) 
{

  int32_t rc = NOERROR;

  char *buffer  = NULL;
  //$A2
  updatedMessageHeader msgHeader;
  updatedMessageHeader *msgHeaderPtr=&msgHeader;
  //messageHeaderDefault msgHeader;
  //messageHeaderDefault *msgHeaderPtr=&msgHeader;
  uint32_t *myBufferIntPtr  = NULL;
//$A2
  uint16_t *myBufferShortPtr  = NULL;
  uint32_t msgMinSize = (uint32_t) MESSAGEMIN;



//double OWDSum = 0.0;
//uint32_t numberOWDSamples=0;
  //most recent OWD sample
  double OWDSample = 0.0;
  //smoothed avg
  double smoothedOWD = 0.0;
  double alpha = 0.10;
  double sendTime = 0.0;

  ssize_t numBytesRcvd  = 0;
  uint32_t RxedMsgSize = 0;

  //

  // Test for correct number of arguments
  if (argc < 2) 
    DieWithUserMessage("Parameter(s)", "<Server Port/Service> <outputFile> ");

  char *service = argv[1]; // First arg:  local port/service

//A2
  if (argc == 3)
  {
    outputFile = argv[2];
    doSampleOutput = true;
  } else 
  {
    doSampleOutput = false;
  }

  // Construct the server address structure
  struct addrinfo addrCriteria;                   // Criteria for address
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
  addrCriteria.ai_socktype = SOCK_DGRAM;          // Only datagram socket
  addrCriteria.ai_protocol = IPPROTO_UDP;         // Only UDP socket

  struct addrinfo *servAddr; // List of server addresses
  int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
  if (rtnVal != 0)
    DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

  // Display returned addresses
  struct addrinfo *addr = NULL;
  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    PrintSocketAddress(addr->ai_addr, stdout);
    fputc('\n', stdout);
  }

#ifdef CREATESAMPLEARRAYS
  sampleArrayIndex = 0;
  seqNoArray = malloc ( MAX_SAMPLES * sizeof(uint32_t) );
  memset(seqNoArray, 0, (MAX_SAMPLES * sizeof(uint32_t)) );
  OWDSampleArrayTS = malloc ( MAX_SAMPLES * sizeof(double) );
  OWDSampleArray = malloc ( MAX_SAMPLES * sizeof(double) );
  memset(OWDSampleArray, 0, (MAX_SAMPLES * sizeof(double)) );
#endif

#ifdef CREATEGAPARRAY
  gapArrayIndex = 0;
  gapArraySize =  malloc ( MAX_GAPS * sizeof(uint32_t) );
  gapArraySeqNo =  malloc ( MAX_GAPS * sizeof(uint32_t) );
  gapArrayTS =  malloc ( MAX_GAPS * sizeof(double) );
#endif

  //Init memory for first send
  buffer = malloc((size_t)MAX_DATA_BUFFER);
  if (buffer == NULL) {
    printf("server: HARD ERROR malloc of  %d bytes failed \n", MAX_DATA_BUFFER);
    exit(1);
  }
  memset(buffer, 0, MAX_DATA_BUFFER);

  signal (SIGINT, CNTCCode);

  // Create socket for incoming connections
  int sock = socket(servAddr->ai_family, servAddr->ai_socktype,
      servAddr->ai_protocol);
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Bind to the local address
  if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
    DieWithSystemMessage("bind() failed");

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);

  if (doSampleOutput )
  {
    printf("server:  open output file %s \n", outputFile);
    outputFID = fopen(outputFile,"w");
  }


  wallTime = getCurTimeD();
  startTime = wallTime;
  for (;;) 
  { 
    struct sockaddr_storage clntAddr; // Client address
    char addrBuffer[INET6_ADDRSTRLEN];
    // Set Length of client address structure (in-out parameter)
    socklen_t clntAddrLen = sizeof(clntAddr);

    // Block until receive message from a client
    // Size of received message
   // ssize_t numBytesRcvd = recvfrom(sock, buffer, MAX_DATA_BUFFER, 0,
    numBytesRcvd = recvfrom(sock, buffer, MAX_DATA_BUFFER, 0,
        (struct sockaddr *) &clntAddr, &clntAddrLen);
    if (numBytesRcvd < 0){
      RxErrorCount++;
      perror("server: Error on recvfrom ");
      continue;
    } else if (numBytesRcvd < msgMinSize) {
      RxErrorCount++;
      printf("server: Error on recvfrom, received (%d) less than MIN (%d) \n ", (int32_t)numBytesRcvd,msgMinSize);
      continue;
    } else 
    { 
      //Else no error on the recv
      RxedMsgSize = numBytesRcvd;
      totalBytesRxed+=RxedMsgSize;
      receivedCount++;
      
      wallTime = getCurTimeD();

      myBufferIntPtr  = (uint32_t *)buffer;
      //unpack to fill in the rx header info
      msgHeaderPtr->sequenceNum = ntohl(*myBufferIntPtr++);
      msgHeaderPtr->timeSentSeconds = ntohl(*myBufferIntPtr++);
      msgHeaderPtr->timeSentNanoSeconds = ntohl(*myBufferIntPtr++);

      //$A2
      myBufferShortPtr  = (uint16_t *)myBufferIntPtr;
      msgHeaderPtr->opMode  = ntohs(*myBufferShortPtr++);
      RxedOpMode = msgHeaderPtr->opMode;

      rxMarker = ntohs(*myBufferShortPtr++);

#ifdef TRACEME
      printf("server: arrival:cur:%d last:%d opMode:%d rxMarker:%0x \n", curSeqNumber,lastSeqNumber, (int32_t)RxedOpMode,rxMarker);
#endif

      //Check if this is the client signal to quit
      if (msgHeaderPtr->sequenceNum  == MAX_UINT32)
      {

        rc =  getFirstV4IPAddress( (struct sockaddr *)&clntAddr, addrBuffer, sizeof(addrBuffer));
        if (rc == ERROR) {
          printf("server: HARD ERROR on getFirstV4IPAddress ??   \n");
        } else {
          printf("server: client TERMINATE signal (size:%d) arrived from client:%s curSeqNumber:%d lastSeqNumber:%d opMode:%d, Marker:0x%04x \n", 
             RxedMsgSize,addrBuffer, msgHeaderPtr->sequenceNum,lastSeqNumber, (int32_t)RxedOpMode, rxMarker);
          //To compute stats ...
          CNTCCode();
        }
      } else
      { 

        //Make sure we record this here and NOT if we detect the TERMINATE 
        timeOfLastRxedMsg = wallTime;
        if (timeOfFirstRxedMsg == -1.0)
        {
          timeOfFirstRxedMsg = wallTime;
        }
        //Current wallclock time - packet send time
        //Only when the client and server run on the same host can we assume perfectly synchronized clocks...
        //The OWDSample fluctuation will include the random time sync errors between the two hosts
        //In some scenarios, it is possible to see a negative OWDSample
        sendTime =  ( (double)msgHeaderPtr->timeSentSeconds +  (((double)msgHeaderPtr->timeSentNanoSeconds)/1000000000.0) );
        OWDSample = wallTime - sendTime;

        if (OWDSample < 0 )
          numberNegativeOWDSamples++;

        if (OWDSample > maxOWDSample)
          maxOWDSample = OWDSample;
        if (OWDSample < minOWDSample)
          minOWDSample = OWDSample;

#ifdef CREATESAMPLEARRAYS
      //Update the array
        if (sampleArrayIndex < MAX_SAMPLES) {
          OWDSampleArrayTS[sampleArrayIndex] = wallTime;
          OWDSampleArray[sampleArrayIndex] = OWDSample;
          seqNoArray[sampleArrayIndex] = msgHeaderPtr->sequenceNum;
          sampleArrayIndex++;
        }
#endif

        OWDSum += OWDSample;
        numberOWDSamples++;

        //Init the filter
        if (numberOWDSamples == 1) {
          smoothedOWD = OWDSample;
        } else {
          smoothedOWD = alpha*OWDSample + (1-alpha)*smoothedOWD;
        }

        if (msgHeaderPtr->sequenceNum > largestSeqRecv)
          largestSeqRecv = msgHeaderPtr->sequenceNum;



        curSeqNumber=msgHeaderPtr->sequenceNum;
        if (curSeqNumber <= lastSeqNumber) 
        {
          printf("server: HARD ERROR dup or out of order ?? :cur:%d last:%d  \n", curSeqNumber,lastSeqNumber);
          exit(1);
        }

        //sizeCurGap 0 means not in a gap
        thisGap = curSeqNumber - lastSeqNumber - 1;

        if ( (thisGap > 0) && (sizeCurGap > 0) )
        {
          //if true, stay in the current active gap
          sizeCurGap+=thisGap;
        }

        if ( (thisGap > 0) && (sizeCurGap == 0) )
        {
          //if true, start this new active gap
          numberOfGaps++;
          sizeCurGap=thisGap;
        }

        if ( (thisGap == 0) && (sizeCurGap > 0) )
        {
          //if true, end the active gap.... 
          sumOfAllGaps+=sizeCurGap;

#ifdef CREATEGAPARRAY
          if (gapArrayIndex < MAX_SAMPLES) {
            gapArraySize[gapArrayIndex] = sizeCurGap;
            gapArraySeqNo[gapArrayIndex] = curSeqNumber;
            gapArrayTS[gapArrayIndex] = wallTime;
            gapArrayIndex++;
          }
#endif
          sizeCurGap=0;
        }

        if (thisGap < 0)
        {
          printf("server: HARD ERROR bad gap:%d??  numberOfGaps:%d \n", thisGap, numberOfGaps);
          exit(1);
        }

        lastSeqNumber = curSeqNumber;


#ifdef TRACE 
        printf("%f %d %d %d %d %d.%d %3.9f %3.9f\n", wallTime, (int32_t)RxedOpMode, RxedMsgSize, largestSeqRecv, 
             msgHeaderPtr->sequenceNum, msgHeaderPtr->timeSentSeconds, msgHeaderPtr->timeSentNanoSeconds, OWDSample, smoothedOWD);
#endif

        if (doSampleOutput )
        {
          fprintf(outputFID,"%f %d %d %d %d %d.%d %3.9f %3.9f\n", wallTime, (int32_t)RxedOpMode, RxedMsgSize, largestSeqRecv, 
             msgHeaderPtr->sequenceNum, msgHeaderPtr->timeSentSeconds, msgHeaderPtr->timeSentNanoSeconds, OWDSample, smoothedOWD);
        }
  
#ifdef TRACE 
        printf("server: Rx %d bytes from ", (int32_t) numBytesRcvd);
        fputs(" client ", stdout);
        PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
        fputc('\n', stdout);
#endif

        if (RxedOpMode == opModeRTT) 
        {

          // Send received datagram back to the client
          ssize_t numBytesSent = sendto(sock, buffer, numBytesRcvd, 0,
            (struct sockaddr *) &clntAddr, sizeof(clntAddr));
          if (numBytesSent < 0) {
            TxErrorCount++;
            perror("server: Error on sendto ");
            continue;
          }
          else if (numBytesSent != numBytesRcvd) {
            TxErrorCount++;
            printf("server: Error on sendto, only sent %d rather than %d ",(int32_t)numBytesSent,(int32_t)numBytesRcvd);
            continue;
          }
        }
      }  //else not the client terminate signal 
    }  //else no error on recv
  } //main loop
}

void CNTCCode() 
{
  double  duration = 0.0;
  double avgLossRate1  = 0.0;
  double avgLossRate2  = 0.0;
  double avgGapSize=0.0;
  double avgLossEventRate=0.0;
  int32_t i=0;

  //Estimate loss based on total observed drops
  uint32_t totalLost2 = 0;

  //Estimate loss based on best guess of number the client sent
  uint32_t totalLost1 = 0;
  uint32_t numberOfTrials;

  double avgOWD = 0.0; 
  double avgThroughput = 0.0;

  //estimate number of trials (only the sender knows this for sure)
  //based on largest seq number seen
  numberOfTrials = largestSeqRecv;

  wallTime = getCurTimeD();
  duration = timeOfLastRxedMsg - timeOfFirstRxedMsg;

  //Compute avgThroughput 
  if (duration > 0.0) {
    avgThroughput = ( (double)totalBytesRxed * 8.0) / duration;
  }

  if (numberOWDSamples > 0)
  {
    avgOWD =   OWDSum / (double)numberOWDSamples;

  } else {
  }

  if (numberOfTrials >= receivedCount)
    totalLost1 = numberOfTrials - receivedCount;
  else 
    totalLost1 = 0;

  totalLost2=sumOfAllGaps;

  if (numberOfTrials > 0)
    avgLossRate1 = (double)totalLost1 / (double)numberOfTrials;

  if (receivedCount >  0) {
    avgLossRate2 = ((double)totalLost2) / (double)( (receivedCount + totalLost2)* 1.0);
    avgLossEventRate =  ((double)numberOfGaps)  / (double)( (double)receivedCount + (double)totalLost2);
  }

  if (numberOfGaps>0) {
    avgGapSize = (double)sumOfAllGaps / (double)numberOfGaps;
  }

  //A1
  //      wallTime, duration, avgOWD,minOWDSample, maxOWDSample, avgThroughput, avgLossRate2, avgGapSize, avgLossEventRate,  numberOfGaps, totalLost2, avgLossRate1, totalLost1, receivedCount);
  printf("duration \tmeanOWD \tminOWD     \tmaxOWD    \tavgTh    \tavgLR2    \tavgGapSz    \tavgLER    \tnumOfGps    \ttotLost2    \tavgLR1    \ttotLost1    \trxCount \tnumberNegativeOWDs  \n");
  printf("%6.2f \t\t%04.9f \t%04.9f \t%04.9f \t%12.0f \t%03.6f \t%03.6f \t%03.6f \t%9d \t%9d \t%3.6f \t%9d  \t%9ld \t%9d \n",
        duration, avgOWD,minOWDSample, maxOWDSample, avgThroughput, avgLossRate2, avgGapSize, avgLossEventRate,  numberOfGaps, totalLost2, avgLossRate1, totalLost1, receivedCount, numberNegativeOWDSamples);

  if (doSampleOutput)
  {
    fprintf(outputFID,"%6.2f \t\t%04.9f \t%04.9f \t%04.9f \t%12.0f \t%03.6f \t%03.6f \t%03.6f \t%9d \t%9d \t%3.6f \t%9d  \t%9ld \t%9d \n",
        duration, avgOWD,minOWDSample, maxOWDSample, avgThroughput, avgLossRate2, avgGapSize, avgLossEventRate,  numberOfGaps, totalLost2, avgLossRate1, totalLost1, receivedCount, numberNegativeOWDSamples);
    fclose(outputFID);
  }

  resultsFID = fopen(resultsOutputFile,"a");
  fprintf(resultsFID,"%6.2f \t\t%04.9f \t%04.9f \t%04.9f \t%12.0f \t%03.6f \t%03.6f \t%03.6f \t%9d \t%9d \t%3.6f \t%9d  \t%9ld \t%9d \n",
        duration, avgOWD,minOWDSample, maxOWDSample, avgThroughput, avgLossRate2, avgGapSize, avgLossEventRate,  numberOfGaps, totalLost2, avgLossRate1, totalLost1, receivedCount, numberNegativeOWDSamples);
  fclose(resultsFID);

#ifdef CREATESAMPLEARRAYS
  printf("server: CREATESAMPLEARRAYS: open samplesArrayFile:%s with %d entries  \n", samplesArrayFile,sampleArrayIndex);
  samplesArrayFID = fopen(samplesArrayFile,"w");
  for(i = 0; i < sampleArrayIndex; i++)
  {
    fprintf(samplesArrayFID,"%12.9f %4.9f  %d \n", OWDSampleArrayTS[i], OWDSampleArray[i], seqNoArray[i] );
  }
  fclose(samplesArrayFID);
#endif

#ifdef CREATEGAPARRAY
  gapArrayFID = fopen(gapArrayFile,"w");
  printf(" --->>  numberOfGaps:%d  gapArrayIndex:%d \n", numberOfGaps, gapArrayIndex);
  for(i = 0; i < gapArrayIndex; i++)
  {
    fprintf(gapArrayFID,"%12.9f %d %d \n", gapArrayTS[i],  gapArraySize[i], gapArraySeqNo[i]);
  }
  fclose(gapArrayFID);

#endif

  exit(0);
}




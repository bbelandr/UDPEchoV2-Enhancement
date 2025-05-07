
********************************************************
*  
*  readme.txt : UDPEchoV2 
*
*   Extends the simpleUDPEcho to ping a message of a given size.  The client also runs for a specified number of
*   times with a specified delay in between iterations.
*
*   client:  
*          UDPEchoV2(v1.1): <Server IP> <Server Port> <Iteration Delay (usecs)> <Message Size (bytes)>] <# of iterations> 
*
*
*  last update:  3/11/2025
*
********************************************************


server
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
Example invocation
./server 6000



client 
*  
* Params:
*  char *  server = argv[1];
*  uint32_t  serverPort = atoi(argv[2]);
*  uint32_t delay atoll(argv[3]);
*  uint32_t messageSize = atoi(argv[4]);
*  uin32_t nIterations = atoi(argv[5]);
*
*  Usage :   client
*             <Server IP>
*             <Server Port>
*             [<Iteration Delay (usecs)>]
*             [<Message Size (bytes)>]
*             [<# of iterations>]
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
The client accepts new program parameters
Iteration Delay :   the delay between iterations passed in units of microseconds...currently 
  not supported by the client.
message Size:  The amount of data to send to the server
NumberOfIterations:  The number of times the client should loop. A value of 0 implies loop forever.


UDPEchoV2:client(v1.1): <Server IP> <Server Port> <Iteration Delay (usecs)> <Message Size (bytes)>] <# of iterations> 

Example invocation
./client localhost 6000 1000 1000 100



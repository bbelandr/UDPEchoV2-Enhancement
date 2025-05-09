/*********************************************************
* Module Name:  UDPEchoV2 server (HARDENED)
*
* File Name:    server.c	
*
* Summary:
*  This file contains the server portion of a client/server
*    UDP-based performance tool, hardened against DDoS attacks.
*  
* Usage:
*     server <service> [outputFile] [maxRate] [whitelist]
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
* A4: 5/8/25   Added DDoS protection:
*             - Rate limiting per client
*             - IP whitelisting
*             - Connection tracking
*             - Size validation
*             - Authentication tokens
*
* Last updated: 5/8/2025
*
*********************************************************/
#include "UDPEcho.h"
#include "AddressHelper.h"
#include "utils.h"
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>

#define MAX_WHITELISTED_IPS 100
#define MAX_CLIENTS 1000
#define DEFAULT_MAX_RATE 1000 // Maximum packets per second per client
#define TOKEN_BUCKET_REFILL_INTERVAL 0.01 // Refill token bucket every 10ms
#define CLIENT_TIMEOUT 300 // Seconds until a client connection times out
#define AUTH_TOKEN_SIZE 16
#define CONNECTION_LIFETIME 120 // 2 minutes max lifetime for a connection
#define BUFFER_CLEANUP_INTERVAL 15 // Cleanup stale connections every 15 seconds

void CatchAlarm(int ignored);
void CNTCCode();
void* connectionCleanupThread(void* arg);
bool isIPWhitelisted(const char* ip);
bool verifyAuthToken(const uint8_t* token, uint32_t seq);
void generateResponseToken(uint8_t* token, uint32_t seq);

// Rate limiting data structures
typedef struct {
    struct sockaddr_storage addr;
    char ip[INET6_ADDRSTRLEN];
    time_t lastSeen;
    double tokenBucket;
    double lastTokenRefill;
    uint32_t packetsReceived;
    uint32_t packetsDropped;
    uint32_t lastSequenceNum;
    bool authenticated;
    uint8_t authToken[AUTH_TOKEN_SIZE];
} ClientInfo;

// Global variables
int sock = -1;                         /* Socket descriptor */
int bStop = 1;
char* whitelist_file = NULL;
char whitelisted_ips[MAX_WHITELISTED_IPS][INET6_ADDRSTRLEN];
int whitelisted_count = 0;
int max_rate = DEFAULT_MAX_RATE;
ClientInfo clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t cleanup_thread;
bool use_whitelist = false;
bool use_authentication = true;
uint8_t server_secret_key[32] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 
                                0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
                                0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

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
uint32_t  packetsDroppedByRateLimit=0;
uint32_t  packetsDroppedByAuth=0;
uint32_t  packetsDroppedByWhitelist=0;

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

// Initialize client tracking data structures
void initClientTracking() {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        memset(&clients[i], 0, sizeof(ClientInfo));
        clients[i].lastSeen = 0;
        clients[i].tokenBucket = max_rate;
        clients[i].lastTokenRefill = getCurTimeD();
        clients[i].packetsReceived = 0;
        clients[i].packetsDropped = 0;
        clients[i].authenticated = false;
    }
}

// Load whitelist from file
bool loadWhitelist(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open whitelist file");
        return false;
    }
    
    char line[INET6_ADDRSTRLEN];
    whitelisted_count = 0;
    
    while (fgets(line, sizeof(line), file) && whitelisted_count < MAX_WHITELISTED_IPS) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            strncpy(whitelisted_ips[whitelisted_count], line, INET6_ADDRSTRLEN);
            whitelisted_count++;
        }
    }
    
    fclose(file);
    printf("Loaded %d whitelisted IPs\n", whitelisted_count);
    return true;
}

// Check if IP is in whitelist
bool isIPWhitelisted(const char* ip) {
    if (!use_whitelist) return true;
    
    int i;
    for (i = 0; i < whitelisted_count; i++) {
        if (strcmp(ip, whitelisted_ips[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Find or create client entry
int findOrCreateClient(const struct sockaddr_storage* addr) {
    char ip[INET6_ADDRSTRLEN];
    int rc = getFirstV4IPAddress((struct sockaddr*)addr, ip, sizeof(ip));
    if (rc == ERROR) {
        return -1;
    }
    
    time_t now = time(NULL);
    int oldest_idx = 0;
    time_t oldest_time = now;
    int empty_idx = -1;
    int i;
    
    pthread_mutex_lock(&clients_mutex);
    
    // Look for existing client or empty slot
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].lastSeen == 0) {
            if (empty_idx == -1) empty_idx = i;
            continue;
        }
        
        if (now - clients[i].lastSeen > CLIENT_TIMEOUT) {
            // This client has timed out
            if (oldest_time > clients[i].lastSeen) {
                oldest_time = clients[i].lastSeen;
                oldest_idx = i;
            }
            continue;
        }
        
        if (strcmp(clients[i].ip, ip) == 0) {
            // Found existing client
            pthread_mutex_unlock(&clients_mutex);
            return i;
        }
    }
    
    // Create new client entry
    int new_idx;
    if (empty_idx != -1) {
        new_idx = empty_idx;
    } else {
        new_idx = oldest_idx;
    }
    
    memcpy(&clients[new_idx].addr, addr, sizeof(struct sockaddr_storage));
    strncpy(clients[new_idx].ip, ip, INET6_ADDRSTRLEN);
    clients[new_idx].lastSeen = now;
    clients[new_idx].tokenBucket = max_rate;
    clients[new_idx].lastTokenRefill = getCurTimeD();
    clients[new_idx].packetsReceived = 0;
    clients[new_idx].packetsDropped = 0;
    clients[new_idx].authenticated = false;
    clients[new_idx].lastSequenceNum = 0;
    
    pthread_mutex_unlock(&clients_mutex);
    return new_idx;
}

// Apply rate limiting
bool checkRateLimit(int client_idx) {
    double now = getCurTimeD();
    
    pthread_mutex_lock(&clients_mutex);
    
    // Refill token bucket
    double elapsed = now - clients[client_idx].lastTokenRefill;
    double tokens_to_add = elapsed * max_rate;
    clients[client_idx].tokenBucket += tokens_to_add;
    if (clients[client_idx].tokenBucket > max_rate) {
        clients[client_idx].tokenBucket = max_rate;
    }
    clients[client_idx].lastTokenRefill = now;
    
    // Check if we have tokens
    if (clients[client_idx].tokenBucket >= 1.0) {
        clients[client_idx].tokenBucket -= 1.0;
        clients[client_idx].lastSeen = time(NULL);
        clients[client_idx].packetsReceived++;
        
        pthread_mutex_unlock(&clients_mutex);
        return true;
    }
    
    clients[client_idx].packetsDropped++;
    pthread_mutex_unlock(&clients_mutex);
    packetsDroppedByRateLimit++;
    return false;
}

// Simple HMAC-like auth token verification
bool verifyAuthToken(const uint8_t* token, uint32_t seq) {
    if (!use_authentication) return true;
    
    // In a real implementation, we would use a proper HMAC
    // For demo purposes, we'll do a simplified validation
    uint8_t expected[AUTH_TOKEN_SIZE];
    uint32_t hash_value = seq ^ 0xdeadbeef;
    
    // XOR with server secret key (simplistic)
    for (int i = 0; i < AUTH_TOKEN_SIZE; i++) {
        expected[i] = (hash_value & 0xff) ^ server_secret_key[i % 32];
        hash_value = hash_value >> 1 | hash_value << 31;  // Rotate
    }
    
    // Compare tokens
    if (memcmp(token, expected, AUTH_TOKEN_SIZE) == 0) {
        return true;
    }
    
    packetsDroppedByAuth++;
    return false;
}

// Generate auth token for responses
void generateResponseToken(uint8_t* token, uint32_t seq) {
    uint32_t hash_value = seq ^ 0xabcdef01;
    
    // XOR with server secret key (simplistic)
    for (int i = 0; i < AUTH_TOKEN_SIZE; i++) {
        token[i] = (hash_value & 0xff) ^ server_secret_key[(i + 16) % 32];
        hash_value = hash_value >> 2 | hash_value << 30;  // Rotate
    }
}

// Cleanup thread to remove stale client entries
void* connectionCleanupThread(void* arg) {
    while (!bStop) {
        sleep(BUFFER_CLEANUP_INTERVAL);
        
        time_t now = time(NULL);
        int cleaned = 0;
        
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].lastSeen != 0 && now - clients[i].lastSeen > CLIENT_TIMEOUT) {
                memset(&clients[i], 0, sizeof(ClientInfo));
                clients[i].lastSeen = 0;
                cleaned++;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        if (cleaned > 0) {
            printf("Cleaned up %d stale client entries\n", cleaned);
        }
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {

  int32_t rc = NOERROR;

  char *buffer  = NULL;
  updatedMessageHeader msgHeader;
  updatedMessageHeader *msgHeaderPtr=&msgHeader;
  uint32_t *myBufferIntPtr  = NULL;
  uint16_t *myBufferShortPtr  = NULL;
  uint32_t msgMinSize = (uint32_t) MESSAGEMIN;
  uint8_t *authTokenPtr = NULL;

  double OWDSample = 0.0;
  double smoothedOWD = 0.0;
  double alpha = 0.10;
  double sendTime = 0.0;

  ssize_t numBytesRcvd  = 0;
  uint32_t RxedMsgSize = 0;

  // Test for correct number of arguments
  if (argc < 2) 
    DieWithUserMessage("Parameter(s)", "<Server Port/Service> [outputFile] [maxRate] [whitelist]");

  char *service = argv[1]; // First arg: local port/service

  if (argc >= 3) {
    outputFile = argv[2];
    doSampleOutput = true;
  } else {
    doSampleOutput = false;
  }
  
  // Parse max rate if provided
  if (argc >= 4) {
    max_rate = atoi(argv[3]);
    if (max_rate <= 0) {
      max_rate = DEFAULT_MAX_RATE;
    }
    printf("Setting max rate to %d packets per second per client\n", max_rate);
  }
  
  // Load whitelist if provided
  if (argc >= 5) {
    whitelist_file = argv[4];
    if (loadWhitelist(whitelist_file)) {
      use_whitelist = true;
      printf("Whitelist enabled with %d IPs\n", whitelisted_count);
    }
  }

  // Initialize client tracking
  initClientTracking();

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
  seqNoArray = malloc(MAX_SAMPLES * sizeof(uint32_t));
  if (!seqNoArray) {
    DieWithSystemMessage("malloc() failed for seqNoArray");
  }
  memset(seqNoArray, 0, (MAX_SAMPLES * sizeof(uint32_t)));
  
  OWDSampleArrayTS = malloc(MAX_SAMPLES * sizeof(double));
  if (!OWDSampleArrayTS) {
    DieWithSystemMessage("malloc() failed for OWDSampleArrayTS");
  }
  
  OWDSampleArray = malloc(MAX_SAMPLES * sizeof(double));
  if (!OWDSampleArray) {
    DieWithSystemMessage("malloc() failed for OWDSampleArray");
  }
  memset(OWDSampleArray, 0, (MAX_SAMPLES * sizeof(double)));
#endif

#ifdef CREATEGAPARRAY
  gapArrayIndex = 0;
  gapArraySize = malloc(MAX_GAPS * sizeof(uint32_t));
  if (!gapArraySize) {
    DieWithSystemMessage("malloc() failed for gapArraySize");
  }
  
  gapArraySeqNo = malloc(MAX_GAPS * sizeof(uint32_t));
  if (!gapArraySeqNo) {
    DieWithSystemMessage("malloc() failed for gapArraySeqNo");
  }
  
  gapArrayTS = malloc(MAX_GAPS * sizeof(double));
  if (!gapArrayTS) {
    DieWithSystemMessage("malloc() failed for gapArrayTS");
  }
#endif

  // Init memory for first send
  buffer = malloc((size_t)MAX_DATA_BUFFER);
  if (buffer == NULL) {
    printf("server: HARD ERROR malloc of %d bytes failed\n", MAX_DATA_BUFFER);
    exit(1);
  }
  memset(buffer, 0, MAX_DATA_BUFFER);

  signal(SIGINT, CNTCCode);

  // Create socket for incoming connections
  sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Bind to the local address
  if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
    DieWithSystemMessage("bind() failed");

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);

  // Set receive timeout to prevent blocking indefinitely
  struct timeval tv;
  tv.tv_sec = 5;  // 5 second timeout
  tv.tv_usec = 0;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    DieWithSystemMessage("setsockopt() failed");
  }

  // Start cleanup thread
  if (pthread_create(&cleanup_thread, NULL, connectionCleanupThread, NULL) != 0) {
    DieWithSystemMessage("Failed to create cleanup thread");
  }

  if (doSampleOutput) {
    printf("server: open output file %s\n", outputFile);
    outputFID = fopen(outputFile, "w");
    if (!outputFID) {
      DieWithSystemMessage("Failed to open output file");
    }
  }

  wallTime = getCurTimeD();
  startTime = wallTime;
  printf("Server started. Press Ctrl+C to exit.\n");
  
  for (;;) { 
    struct sockaddr_storage clntAddr; // Client address
    char addrBuffer[INET6_ADDRSTRLEN];
    // Set Length of client address structure (in-out parameter)
    socklen_t clntAddrLen = sizeof(clntAddr);

    // Block until receive message from a client
    numBytesRcvd = recvfrom(sock, buffer, MAX_DATA_BUFFER, 0,
        (struct sockaddr *) &clntAddr, &clntAddrLen);
        
    if (numBytesRcvd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Timeout occurred, continue to allow cleanup thread to run
        continue;
      }
      RxErrorCount++;
      perror("server: Error on recvfrom");
      continue;
    } else if (numBytesRcvd < msgMinSize) {
      RxErrorCount++;
      printf("server: Error on recvfrom, received (%d) less than MIN (%d)\n", 
             (int32_t)numBytesRcvd, msgMinSize);
      continue;
    }
    
    // Get client IP address for logging
    rc = getFirstV4IPAddress((struct sockaddr*)&clntAddr, addrBuffer, sizeof(addrBuffer));
    if (rc == ERROR) {
      RxErrorCount++;
      printf("server: Error getting client IP address\n");
      continue;
    }
    
    // Check if client is whitelisted
    if (!isIPWhitelisted(addrBuffer)) {
      packetsDroppedByWhitelist++;
      if (packetsDroppedByWhitelist % 100 == 1) {  // Log only occasionally to prevent log flooding
        printf("server: Dropped packet from non-whitelisted IP: %s\n", addrBuffer);
      }
      continue;
    }
    
    // Find or create client record and apply rate limiting
    int client_idx = findOrCreateClient(&clntAddr);
    if (client_idx < 0) {
      RxErrorCount++;
      printf("server: Error tracking client connection\n");
      continue;
    }
    
    // Apply rate limiting
    if (!checkRateLimit(client_idx)) {
      if (clients[client_idx].packetsDropped % 100 == 1) {  // Log only occasionally
        printf("server: Rate limiting dropped packet from %s\n", addrBuffer);
      }
      continue;
    }
    
    // Validate message size more strictly
    if (numBytesRcvd > MAX_DATA_BUFFER) {
      RxErrorCount++;
      printf("server: Packet too large (%d bytes) from %s\n", (int32_t)numBytesRcvd, addrBuffer);
      continue;
    }
    
    // Else no error on the recv
    RxedMsgSize = numBytesRcvd;
    
    // Parse message header
    myBufferIntPtr = (uint32_t *)buffer;
    //unpack to fill in the rx header info
    msgHeaderPtr->sequenceNum = ntohl(*myBufferIntPtr++);
    msgHeaderPtr->timeSentSeconds = ntohl(*myBufferIntPtr++);
    msgHeaderPtr->timeSentNanoSeconds = ntohl(*myBufferIntPtr++);

    myBufferShortPtr = (uint16_t *)myBufferIntPtr;
    msgHeaderPtr->opMode = ntohs(*myBufferShortPtr++);
    RxedOpMode = msgHeaderPtr->opMode;
    rxMarker = ntohs(*myBufferShortPtr++);
    
    // Get pointer to auth token (16 bytes after the header)
    authTokenPtr = (uint8_t*)(myBufferShortPtr + 1);
    
    // Verify token (only for established clients)
    if (clients[client_idx].packetsReceived > 1 && !verifyAuthToken(authTokenPtr, msgHeaderPtr->sequenceNum)) {
      if (packetsDroppedByAuth % 100 == 1) {  // Log only occasionally
        printf("server: Authentication failed for packet from %s\n", addrBuffer);
      }
      continue;
    } else {
      clients[client_idx].authenticated = true;
    }
    
    // Check for sequence number anomalies (potential replay attacks)
    if (clients[client_idx].packetsReceived > 1 && 
        msgHeaderPtr->sequenceNum <= clients[client_idx].lastSequenceNum) {
      RxErrorCount++;
      printf("server: Potential replay attack - out of order packet or duplicate from %s\n", addrBuffer);
      continue;
    }
    clients[client_idx].lastSequenceNum = msgHeaderPtr->sequenceNum;

    // Process remaining packet
    wallTime = getCurTimeD();
    totalBytesRxed += RxedMsgSize;
    receivedCount++;
    
    // Check if this is the client signal to quit
    if (msgHeaderPtr->sequenceNum == MAX_UINT32) {
      printf("server: client TERMINATE signal (size:%d) arrived from client:%s curSeqNumber:%d lastSeqNumber:%d opMode:%d, Marker:0x%04x\n", 
         RxedMsgSize, addrBuffer, msgHeaderPtr->sequenceNum, lastSeqNumber, (int32_t)RxedOpMode, rxMarker);
      //To compute stats ...
      CNTCCode();
    } else {
      //Make sure we record this here and NOT if we detect the TERMINATE 
      timeOfLastRxedMsg = wallTime;
      if (timeOfFirstRxedMsg == -1.0) {
        timeOfFirstRxedMsg = wallTime;
      }
      
      //Current wallclock time - packet send time
      sendTime = ((double)msgHeaderPtr->timeSentSeconds + (((double)msgHeaderPtr->timeSentNanoSeconds)/1000000000.0));
      OWDSample = wallTime - sendTime;

      if (OWDSample < 0)
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

      curSeqNumber = msgHeaderPtr->sequenceNum;
      if (curSeqNumber <= lastSeqNumber) {
        numberOutOfOrder++;
        printf("server: Out of order packet detected: cur:%d last:%d\n", curSeqNumber, lastSeqNumber);
        continue;  // Skip further processing for out-of-order packets
      }

      //sizeCurGap 0 means not in a gap
      thisGap = curSeqNumber - lastSeqNumber - 1;

      if ((thisGap > 0) && (sizeCurGap > 0)) {
        //if true, stay in the current active gap
        sizeCurGap += thisGap;
      }

      if ((thisGap > 0) && (sizeCurGap == 0)) {
        //if true, start this new active gap
        numberOfGaps++;
        sizeCurGap = thisGap;
      }

      if ((thisGap == 0) && (sizeCurGap > 0)) {
        //if true, end the active gap.... 
        sumOfAllGaps += sizeCurGap;

#ifdef CREATEGAPARRAY
        if (gapArrayIndex < MAX_GAPS) {
          gapArraySize[gapArrayIndex] = sizeCurGap;
          gapArraySeqNo[gapArrayIndex] = curSeqNumber;
          gapArrayTS[gapArrayIndex] = wallTime;
          gapArrayIndex++;
        }
#endif
        sizeCurGap = 0;
      }

      if (thisGap < 0) {
        printf("server: Warning: bad gap:%d?? numberOfGaps:%d\n", thisGap, numberOfGaps);
        continue;
      }

      lastSeqNumber = curSeqNumber;

#ifdef TRACE 
      printf("%f %d %d %d %d %d.%d %3.9f %3.9f\n", wallTime, (int32_t)RxedOpMode, RxedMsgSize, largestSeqRecv, 
           msgHeaderPtr->sequenceNum, msgHeaderPtr->timeSentSeconds, msgHeaderPtr->timeSentNanoSeconds, OWDSample, smoothedOWD);
#endif

      if (doSampleOutput) {
        fprintf(outputFID, "%f %d %d %d %d %d.%d %3.9f %3.9f\n", wallTime, (int32_t)RxedOpMode, RxedMsgSize, largestSeqRecv, 
           msgHeaderPtr->sequenceNum, msgHeaderPtr->timeSentSeconds, msgHeaderPtr->timeSentNanoSeconds, OWDSample, smoothedOWD);
      }

#ifdef TRACE 
      printf("server: Rx %d bytes from ", (int32_t) numBytesRcvd);
      fputs(" client ", stdout);
      PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
      fputc('\n', stdout);
#endif

      if (RxedOpMode == opModeRTT) {
        // Generate server auth token for response
        generateResponseToken(authTokenPtr, msgHeaderPtr->sequenceNum);
        
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
    avgThroughput = ((double)totalBytesRxed * 8.0) / duration;
  }

  if (numberOWDSamples > 0)
  {
    avgOWD = OWDSum / (double)numberOWDSamples;
  }

  if (numberOfTrials >= receivedCount)
    totalLost1 = numberOfTrials - receivedCount;
  else 
    totalLost1 = 0;

  totalLost2 = sumOfAllGaps;

  if (numberOfTrials > 0)
    avgLossRate1 = (double)totalLost1 / (double)numberOfTrials;

  if (receivedCount >  0) {
    avgLossRate2 = ((double)totalLost2) / (double)((receivedCount + totalLost2) * 1.0);
    avgLossEventRate = ((double)numberOfGaps) / (double)((double)receivedCount + (double)totalLost2);
  }

  if (numberOfGaps > 0) {
    avgGapSize = (double)sumOfAllGaps / (double)numberOfGaps;
  }

  // Print security stats
  printf("\nSecurity Statistics:\n");
  printf("Packets dropped by rate limit: %u\n", packetsDroppedByRateLimit);
  printf("Packets dropped by whitelist: %u\n", packetsDroppedByWhitelist);
  printf("Packets dropped by authentication: %u\n", packetsDroppedByAuth);
  printf("Out-of-order packets: %u\n\n", numberOutOfOrder);

  printf("duration \tmeanOWD \tminOWD     \tmaxOWD    \tavgTh    \tavgLR2    \tavgGapSz    \tavgLER    \tnumOfGps    \ttotLost2    \tavgLR1    \ttotLost1    \trxCount \tnumberNegativeOWDs  \n");
  printf("%6.2f \t\t%04.9f \t%04.9f \t%04.9f \t%12.0f \t%03.6f \t%03.6f \t%03.6f \t%9d \t%9d \t%3.6f \t%9d  \t%9ld \t%9d \n",
        duration, avgOWD, minOWDSample, maxOWDSample, avgThroughput, avgLossRate2, avgGapSize, avgLossEventRate, numberOfGaps, totalLost2, avgLossRate1, totalLost1, receivedCount, numberNegativeOWDSamples);

  if (doSampleOutput) {
    fprintf(outputFID, "%6.2f \t\t%04.9f \t%04.9f \t%04.9f \t%12.0f \t%03.6f \t%03.6f \t%03.6f \t%9d \t%9d \t%3.6f \t%9d  \t%9ld \t%9d \n",
        duration, avgOWD, minOWDSample, maxOWDSample, avgThroughput, avgLossRate2, avgGapSize, avgLossEventRate, numberOfGaps, totalLost2, avgLossRate1, totalLost1, receivedCount, numberNegativeOWDSamples);
    fclose(outputFID);
  }

  resultsFID = fopen(resultsOutputFile, "a");
  if (resultsFID) {
    fprintf(resultsFID, "%6.2f \t\t%04.9f \t%04.9f \t%04.9f \t%12.0f \t%03.6f \t%03.6f \t%03.6f \t%9d \t%9d \t%3.6f \t%9d  \t%9ld \t%9d \n",
          duration, avgOWD, minOWDSample, maxOWDSample, avgThroughput, avgLossRate2, avgGapSize, avgLossEventRate, numberOfGaps, totalLost2, avgLossRate1, totalLost1, receivedCount, numberNegativeOWDSamples);
    fclose(resultsFID);
  }

#ifdef CREATESAMPLEARRAYS
  printf("server: CREATESAMPLEARRAYS: open samplesArrayFile:%s with %d entries\n", samplesArrayFile, sampleArrayIndex);
  samplesArrayFID = fopen(samplesArrayFile, "w");
  if (samplesArrayFID) {
    for(i = 0; i < sampleArrayIndex; i++) {
      fprintf(samplesArrayFID, "%12.9f %4.9f %d \n", OWDSampleArrayTS[i], OWDSampleArray[i], seqNoArray[i]);
    }
    fclose(samplesArrayFID);
  }
#endif

#ifdef CREATEGAPARRAY
  gapArrayFID = fopen(gapArrayFile, "w");
  if (gapArrayFID) {
    printf(" --->> numberOfGaps:%d gapArrayIndex:%d \n", numberOfGaps, gapArrayIndex);
    for(i = 0; i < gapArrayIndex; i++) {
      fprintf(gapArrayFID, "%12.9f %d %d \n", gapArrayTS[i], gapArraySize[i], gapArraySeqNo[i]);
    }
    fclose(gapArrayFID);
  }
#endif

  // Clean up resources
  if (sock >= 0) {
    close(sock);
  }
  
  if (seqNoArray) free(seqNoArray);
  if (OWDSampleArrayTS) free(OWDSampleArrayTS);
  if (OWDSampleArray) free(OWDSampleArray);
  if (gapArraySize) free(gapArraySize);
  if (gapArraySeqNo) free(gapArraySeqNo);
  if (gapArrayTS) free(gapArrayTS);
  
  // Signal cleanup thread to exit
  bStop = 0;
  pthread_join(cleanup_thread, NULL);
  
  exit(0);
}
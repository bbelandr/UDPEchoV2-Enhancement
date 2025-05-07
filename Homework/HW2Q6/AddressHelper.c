/*********************************************************
*
* Module Name: AddressHelper 
*
* File Name:  AddressHelper.c
*
* Summary:  This holds helpful routines related to addresses 
* Revisions:
*
* Last update: 4/6/2025 
*
*********************************************************/
#include "AddressHelper.h"
#include "utils.h"
#include <errno.h>
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <netinet/in.h> /* for in_addr */
#include <arpa/inet.h> /* for inet_addr ... */

#define SUCCESS 0
#define NOERROR 0

#define ERROR   -1
#define FAILURE -1
#define FAILED -1


//If defined, debug tracing is enabled
#define TRACEME 0

/***********************************************************
* Function: int32_t getFirstV4IPAddress ( const struct sockaddr *address, char *callersBuffer, uint32_t callersBufSize);
*
* Explanation: extracts the first IP v4 address in inet_ntop format copying it into the callers buffer 
*              The   A NL is added making the returned answer a string.
*
*              TODO:  Develop a correct version of this that will return the first IPv4 or IPv6 address 
*                      adding a af param to specify....that code should follow PrintSocketAddresses ....
*
* Inputs:   
*  const struct sockaddr *address  :  even though this is a generic sockaddr struct , this must actually
*                  point to a struct sockaddr_in   
*                  This must be in network byte order.
*
*  char *callersBuffer  :  ptr to the caller's buffer 
*  uint32_t callersBufSize) :   size of the caller's buffer
*
* Output: If no errors occur, returns SUCCESS and cps the ntop output into the callers buffer 
*          On error,  an FAILURE is returned.  
*
**************************************************/
int32_t getFirstV4IPAddress (const struct sockaddr *address, char *callersBuffer, uint32_t callersBufSize)
{
  int rc = SUCCESS;
  struct sockaddr_in *thisIP;
  char *ntopPtrReturn=NULL;


  // For ipv4, a valid address to a struct in_addr will be placed in this location;
  void *numericAddress = NULL; // Pointer to binary address
  in_port_t port; // Port to print

  thisIP= (struct sockaddr_in *)address;


  //extract the reference to the struct in_addr
  numericAddress = &(thisIP)->sin_addr;
  port = ntohs((thisIP)->sin_port);
  rc = NOERROR;
  (void) inet_ntop(address->sa_family, numericAddress, callersBuffer, (size_t)callersBufSize);
  printf("getFirstV4IPAddress: IPV4 address: %s:%d \n", callersBuffer, (int32_t)port);
  return rc;

    //ntopPtrReturn =  inet_ntop(address->sa_family, numericAddress, callersBuffer, (size_t)callersBufSize);
    if (ntopPtrReturn== NULL)
    { 
      rc = ERROR;
      printf("getFirstV4IPAddress:  HARD ERROR on call to ntop ,   errno:%d \n", errno);
    }
    else {
      rc = NOERROR;
//#ifdef TRACEME
      printf("getFirstV4IPAddress: IPV4 address: %s:%d \n", callersBuffer, (int32_t)port);
//#endif
    }

  //return rc;
}

/***********************************************************
* Function: int NumberOfAddress(const struct sockaddr *address) 
*
* Explanation:  Returns the number of addresses in the list 
*
* inputs:   
*   const struct addrinfo  *addrList) 
*
* outputs: returns a FAILURE (-1) or valid number of addresses
*
**************************************************/
int NumberOfAddresses(struct addrinfo *addrList)
{
int rc = FAILURE;
int counter = 0;
struct addrinfo *addr=NULL;

  if (addrList == NULL)
    rc = FAILURE;
  else {
    for (addr = addrList; addr != NULL; addr = addr->ai_next) {
      counter++;
//      PrintSocketAddress(addr->ai_addr, stdout);
//      fputc('\n', stdout);
    }
    rc = counter;
  }
  return rc;
} 


/***********************************************************
* Function: void PrintSocketAddress(const struct sockaddr *address, FILE *stream) 
*
* Explanation: prints all addresses in the sockaddr
*
* inputs:   
*    const struct sockaddr *address) 
*    FILE *stream : output stream descriptor
*
* outputs: 
*
**************************************************/
void PrintSocketAddress(const struct sockaddr *address, FILE *stream) 
{
  // Test for address and stream
  if (address == NULL || stream == NULL)
    return;

  void *numericAddress; // Pointer to binary address

  // Buffer to contain result (IPv6 sufficient to hold IPv4)
  char addrBuffer[INET6_ADDRSTRLEN];
  in_port_t port; // Port to print
  // Set pointer to address based on address family
  switch (address->sa_family) {
  case AF_INET:
    numericAddress = &((struct sockaddr_in *) address)->sin_addr;
    port = ntohs(((struct sockaddr_in *) address)->sin_port);
    break;
  case AF_INET6:
    numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
    port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
    break;
  case AF_PACKET:
    fputs("address Family AF_PACKET, ignore.... \n", stream);   //
    return;
  default:
    fputs("[unknown type] \n", stream);    // Unhandled type
    return;
  }
  // Convert binary to printable address (presentation format)
  if (inet_ntop(address->sa_family, numericAddress, addrBuffer,
      sizeof(addrBuffer)) == NULL)
    fputs("[invalid address]\n", stream); // Unable to convert
  else {
    fprintf(stream, "%s", addrBuffer);
    if (port != 0)                // Zero not valid in any socket addr
      fprintf(stream, "-%u", port);
    fprintf(stream, "\n");
  }
}

bool SockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2) {
  if (addr1 == NULL || addr2 == NULL)
    return addr1 == addr2;
  else if (addr1->sa_family != addr2->sa_family)
    return false;
  else if (addr1->sa_family == AF_INET) {
    struct sockaddr_in *ipv4Addr1 = (struct sockaddr_in *) addr1;
    struct sockaddr_in *ipv4Addr2 = (struct sockaddr_in *) addr2;
    return ipv4Addr1->sin_addr.s_addr == ipv4Addr2->sin_addr.s_addr
        && ipv4Addr1->sin_port == ipv4Addr2->sin_port;
  } else if (addr1->sa_family == AF_INET6) {
    struct sockaddr_in6 *ipv6Addr1 = (struct sockaddr_in6 *) addr1;
    struct sockaddr_in6 *ipv6Addr2 = (struct sockaddr_in6 *) addr2;
    return memcmp(&ipv6Addr1->sin6_addr, &ipv6Addr2->sin6_addr,
        sizeof(struct in6_addr)) == 0 && ipv6Addr1->sin6_port
        == ipv6Addr2->sin6_port;
  } else
    return false;
}


/*************************************************************
*
* Function: int findAF_INET6SocketAddress(struct addrinfo *,
*                   struct sockaddr **callersaddress) 
* 
* Summary:  attempts to find an IP V6 addr in the list.  Returns ERROR if can not find one.
*            else fills in the caller's addr ptr with the address and returns NOERROR.
*
* Inputs:
*   struct addrinfo *  : reference to callers addrinfo  
*   struct sockaddr **callersaddress : reference to the callers address ptr that is to be filled in
*           
*
* outputs:  
*     updates callersaddress if an IPV6 addr is found in the list. 
*     returns the number of IPV6 address that are found.  0 means none found.
*     If > 1, the callersaddress is filled in with the first
*
* notes: 
*    
*
***************************************************************/
int findAF_INET6SocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddress) 
{
  struct addrinfo *addr = NULL;
  uint32_t count =0;


  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    if (addr->ai_addr->sa_family == AF_INET6) {
      if (count == 0)
        *callersaddress = addr->ai_addr;
      count++;
    }
  }

#ifdef TRACEME
  printf("findAF_INET6SocketAddress: Found %d V6 addresses \n", count);
#endif
  return count;
}

/*************************************************************
*
* Function: int findAF_INETSocketAddress(const struct sockaddr *address, struct sockaddr **callersaddress) 
* 
* Summary:  attempts to find an IP V4 addr in the list.  Returns ERROR if can not find one.
*            else fills in the caller's addr ptr with the first V4 addr in the list
*           and returns NOERROR.
*
* Inputs:
*   const struct sockaddr *address : reference to address of interest 
*   struct sockaddr **callersaddress : reference to the callers address ptr that is to be filled in
*           
*
* outputs:  
*     updates callersaddress if an IPV4 addr is found. Returns the number of
*        IPV4 addresses found. A 0 means none found. 
*
* notes: 
*    
*
***************************************************************/
int findAF_INETSocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddress) 
{
  struct addrinfo *addr = NULL;
  uint32_t count =0;

  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    if (addr->ai_addr->sa_family == AF_INET) {
      if (count == 0)
        *callersaddress = addr->ai_addr;
      count++;
    }
  }

#ifdef TRACEME 
  printf("findAF_INETSocketAddress: Found %d V4 addresses \n", count);
#endif

  return count;
}




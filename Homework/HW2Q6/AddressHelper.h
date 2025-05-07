/************************************************************************
* File:  AddressHelper.h
*
* Purpose:
*   This is the include file for the AddressHelper module
*
* Notes:
*   Code should always exit using Unix convention:  exit(EXIT_SUCCESS) or exit(EXIT_FAILURE)
*
* A1: 4/5/25: Added getFirstV4IPAddress
*
* Last update: 4/5/2025
*
************************************************************************/
#ifndef	__AddressHelper_h
#define	__AddressHelper_h

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
//#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>



int NumberOfAddresses(struct addrinfo *addrList);
// Print socket address
void PrintSocketAddress(const struct sockaddr *address, FILE *stream);
//A1
int32_t getFirstV4IPAddress (const struct sockaddr *address, char *callersBuffer, uint32_t callersBufSize);

// Test socket address equality
//
bool SockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2);
int findAF_INET6SocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddresss);
int findAF_INETSocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddresss); 

#ifndef LINUX
#define INADDR_NONE 0xffffffff
#endif


#endif



include Make.defines

PROGS =	 client server getaddrinfo

OPTIONS = -DUNIX  -DANSI


COBJECTS =	AddressHelper.o DieWithError.o DieWithMessage.o  utils.o
CSOURCES =	AddressHelper.c DieWithError.c DieWithMessage.c utils.c

CPLUSOBJECTS = 

COMMONSOURCES =

CPLUSSOURCES =

all:	${PROGS}


client:		client.o $(CPLUSOBJECTS) $(COBJECTS) $(LIBS) $(COMMONSOURCES) $(SOURCES)
		${CC} ${LINKOPTIONS}  $@ client.o $(CPLUSOBJECTS) $(COBJECTS) $(LIBS) $(LINKFLAGS)

server:		server.o $(CPLUSOBJECTS) $(COBJECTS)
		${CC} ${LINKOPTIONS} $@ server.o $(CPLUSOBJECTS) $(COBJECTS) $(LIBS) $(LINKFLAGS)

getaddrinfo:	GetAddrInfo.o $(CPLUSOBJECTS) $(COBJECTS)
		${CC} ${LINKOPTIONS} $@ GetAddrInfo.o $(CPLUSOBJECTS) $(COBJECTS) $(LIBS) $(LINKFLAGS)



.cc.o:	$(HEADERS)
	$(CPLUS) $(CPLUSFLAGS) $(OPTIONS) $<

.c.o:	$(HEADERS)
	$(CC) $(CFLAGS) $(OPTIONS) $<



backup:
	rm -f UDPEchoHW2Q6.tar
	rm -f UDPEchoHW2Q6.tar.gz
	tar -cf UDPEchoHW2Q6.tar *
	gzip -f UDPEchoHW2Q6.tar

clean:
		rm -f ${PROGS} ${CLEANFILES}

depend:
		makedepend client.c server.c $(CFLAGS) $(HEADERS) $(SOURCES) $(COMMONSOURCES) $(CSOURCES)
#		mkdep $(CFLAGS) $(HEADERS) $(SOURCES) $(COMMONSOURCES) $(CSOURCES)

# DO NOT DELETE


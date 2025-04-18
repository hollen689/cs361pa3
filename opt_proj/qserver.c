#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include "qserver.h"

#define	QLEN			5
#define	BUFSIZE			4096

typedef struct 
{
	int *groupsize;
	int tsock;
} thrargs_t;

int passivesock( char *service, char *protocol, int qlen, int *rport );

void *client( thrargs_t *arg )
{
	char buf[BUFSIZE];
	int cc;
	int ssock = arg->tsock;

	/* start working for this guy */
	/* ECHO what the client says */
	for (;;)
	{
		if ( (cc = read( ssock, buf, BUFSIZE )) <= 0 )
		{
			printf( "The client has gone.\n" );
			close(ssock);
			break;
		}
		else
		{
			printf("Client wants to join the group\n");
			buf[cc] = '\0';
			if (strncmp(buf, GROUP, strlen(GROUP)) == 0) {
				char *token = strtok(buf, "|"); // "GROUP"
				char *name = strtok(NULL, "|"); // name
				char *sizeStr = strtok(NULL, "\r\n");

				if (sizeStr && *(arg->groupsize) == 0) {
					*(arg->groupsize) = atoi(sizeStr);
					printf("Group size set to %d by %s\n", *(arg->groupsize), name);
				}

				write(ssock, WAIT, strlen(WAIT));
			} 
			if (strncmp(buf, JOIN, strlen(JOIN)) == 0){

			}
		}
	}
	free(arg);
	pthread_exit(NULL);
}


/*
*/
int
main( int argc, char *argv[] )
{
	//buffer for commandline
	ques_t qbuf[BBUF];
	//default stuff
	char			*service;
	struct sockaddr_in	fsin;
	int			alen = sizeof(fsin);
	int			msock;
	int			ssock;
	int			rport = 0;
	//Counter for number of connections
	int client_count = 0;
	int groupsize = 0;
	
	switch (argc) 
	{
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			// User provides a port? then use it
			service = argv[1];
			break;
		case	3:
			read_questions(argv[1], qbuf);
			service = argv[2];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	// Call the function that sets everything up
	msock = passivesock( service, "tcp", QLEN, &rport );

	if (rport)
	{
		// Tell the user the selected port number
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	
	for (;;)
	{
		int	ssock;
		pthread_t	thr;
		thrargs_t arg;
		if ((ssock = accept( msock, (struct sockaddr *)&fsin, &alen )) >= 0){
			if (client_count == 0) {
				// First client
				write(ssock, WADMIN, strlen(WADMIN));
			} 
			client_count++;
			if (client_count > 1 && client_count < arg.groupsize){
				write(ssock, JOIN, strlen(JOIN));
			}
			
		}
		else
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			break;
		}

		printf( "A client has arrived for echoes - serving on fd %d.\n", ssock );
		fflush( stdout );

		arg.groupsize = &groupsize;
		arg.tsock = ssock;

		pthread_create( &thr, NULL, (void *(*)(void *))client, (void *)&arg);

	}
	pthread_exit(NULL);
}



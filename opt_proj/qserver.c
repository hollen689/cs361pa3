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

#define	QLEN			5
#define	BUFSIZE			4096

int passivesock( char *service, char *protocol, int qlen, int *rport );

void *echo( void *s )
{
	char buf[BUFSIZE];
	int cc;
	int ssock = (int) s;

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
			buf[cc] = '\0';
			//printf( "The client says: %s\n", buf );
			if ( write( ssock, buf, cc ) < 0 )
			{
				/* This guy is dead */
				close( ssock );
				break;
			}
		}
	}
	pthread_exit(NULL);
}


/*
*/
int
main( int argc, char *argv[] )
{
	//buffer for commandline
	char buffer[BUFSIZE];
	//default stuff
	char			*service;
	struct sockaddr_in	fsin;
	int			alen = sizeof(fsin);
	int			msock;
	int			ssock;
	int			rport = 0;
	//Counter for number of connections
	int client_count = 0;
	
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
			FILE *fp = fopen(argv[1], "r");
			if (fp == NULL) {
				printf("Error opening file");
				exit(EXIT_FAILURE);
			}
		
			while (fgets(buffer, BUFSIZE, fp) == NULL) {
				printf("Error reading file");

				//debug statement
				//printf("%s", buffer);
			}
		
			fclose(fp);
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

		if (ssock = accept( msock, (struct sockaddr *)&fsin, &alen ) >= 0){
			client_count++;
		}
		else
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			break;
		}
		if (ssock < 0)
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			break;
		}

		printf( "A client has arrived for echoes - serving on fd %d.\n", ssock );
		fflush( stdout );

		pthread_create( &thr, NULL, echo, (void *) ssock );

	}
	pthread_exit(NULL);
}



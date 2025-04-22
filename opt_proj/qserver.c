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

int group_size = 0;
int client_count = 0;
ques_t qbuf[BBUF]; 

//barriers to synchronize threads
pthread_barrier_t barrier;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


int passivesock( char *service, char *protocol, int qlen, int *rport );

void *client( void *s )
{
	char buf[BUFSIZE];
	int cc;
	int ssock = (int) s;

	/* start working for this guy */
	/* ECHO what the client says */
	
	client_count++;
	if (client_count == 1) {
			// First client
		write(ssock, WADMIN, strlen(WADMIN));
	} else if (client_count > 1 && client_count <= group_size){
		write(ssock, WJOIN, strlen(WJOIN));
	} else {
		write(ssock, FULL, strlen(FULL));
		close(ssock);
	}

	//old code
	if ( (cc = read( ssock, buf, BUFSIZE )) <= 0 )
	{
		printf( "The client has gone.\n" );
		close(ssock);
	}
	else
	{
		printf("Client wants to join the group\n");
		if (strncmp(buf, GROUP, strlen(GROUP)) == 0) {
			char *token = strtok(buf, "|"); // "GROUP"
			char *name = strtok(NULL, "|"); // "Alice"
			char *sizeStr = strtok(NULL, "\r\n"); // "4"
			if (group_size == 0 && sizeStr != NULL) {
				//group_size = atoi(sizeStr); //change to strtol
				group_size = strtol(sizeStr, NULL, 10);
				pthread_barrier_init(&barrier, NULL, group_size);
				printf("Group size set to %d by leader %s\n", group_size, name);
			} 
			write(ssock, WAIT, strlen(WAIT));
			pthread_barrier_wait(&barrier); //wait barrier
		} else if (strncmp(buf, JOIN, strlen(JOIN)) == 0) {
			char *token = strtok(buf, "|");
			char *name = strtok(NULL, "|");
			write(ssock, WAIT, strlen(WAIT));
			pthread_barrier_wait(&barrier); //wait barrier
		}
		if (client_count == group_size){
			char question_msg[BUFSIZE];
			snprintf(question_msg, BUFSIZE, "QUES|%ld|%s\r\n", strlen(qbuf[0].qtext), qbuf[0].qtext);
			write(ssock, question_msg, strlen(question_msg));
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
	//ques_t qbuf[BBUF];
	//default stuff
	char			*service;
	struct sockaddr_in	fsin;
	int			alen = sizeof(fsin);
	int			msock;
	int			ssock;
	int			rport = 0;
	//Counter for number of connections
	//int client_count = 0;
	
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
		int	ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
		pthread_t	thr;

		/*
		if ((ssock = accept( msock, (struct sockaddr *)&fsin, &alen )) >= 0){
			client_count++;
			if (client_count == 1) {
				// First client
				write(ssock, WADMIN, strlen(WADMIN));
			} else if (client_count > 1 && client_count <= group_size){
				write(ssock, WJOIN, strlen(WJOIN));
			} else {
				write(ssock, FULL, strlen(FULL));
				close(ssock);
				continue;
			}
		}
		*/

		if (ssock < 0)
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			break;
		}

		printf( "A client has arrived for echoes - serving on fd %d.\n", ssock );
		fflush( stdout );

		pthread_create( &thr, NULL, client, (void *) ssock );

	}
	pthread_exit(NULL);
}



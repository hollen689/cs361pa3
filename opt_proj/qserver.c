#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include "qserver.h"

#define	QLEN			5
#define	BUFSIZE			4096
//Buffer size for names
#define NBUF            64

//Struct for each client storing their socket, name, and score
typedef struct {
    int socket;
    char name[NBUF];
    int score;
} client_info_t;

int group_size = 0;
int client_count = 0;
ques_t *qbuf[MAXQ];

//condition variables and mutexes
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
bool c = false;

pthread_mutex_t winner_mutex = PTHREAD_MUTEX_INITIALIZER;

//winner variables
char winner_name[NBUF];
bool winner_set = false;

int passivesock(char *service, char *protocol, int qlen, int *rport);

void *client(void *s)
{
	//List of client thread sockets
    static client_info_t clients[500];
    static int answers_received = 0;

    char buf[BUFSIZE];
    int cc;
    int ssock = (int)s;

    pthread_mutex_lock(&lock);
	//Indexing client thread sockets
    int client_index = client_count;
    clients[client_index].socket = ssock;
    client_count++;

    if (client_count == 1) {
        write(ssock, WADMIN, strlen(WADMIN));
    } else if (client_count > 1 && client_count <= group_size) {
        write(ssock, WJOIN, strlen(WJOIN));
    } else {
        write(ssock, FULL, strlen(FULL));
        close(ssock);
        pthread_mutex_unlock(&lock);
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&lock);

    if ((cc = read(ssock, buf, BUFSIZE)) <= 0) {
        printf("The client has gone.\n");
        close(ssock);
        pthread_exit(NULL);
    }

    if (strncmp(buf, GROUP, strlen(GROUP)) == 0) {
        char *token = strtok(buf, "|");
        char *name = strtok(NULL, "|");
        char *sizeStr = strtok(NULL, "\r\n");
        if (group_size == 0 && sizeStr != NULL) {
            group_size = strtol(sizeStr, NULL, 10);
            printf("Group size set to %d by leader %s\n", group_size, name);
        }
        strncpy(clients[client_index].name, name, NBUF);
        clients[client_index].score = 0;
        write(ssock, WAIT, strlen(WAIT));
    } else if (strncmp(buf, JOIN, strlen(JOIN)) == 0) {
        char *token = strtok(buf, "|");
        char *name = strtok(NULL, "\r\n");
        strncpy(clients[client_index].name, name, NBUF);
        clients[client_index].score = 0;
        write(ssock, WAIT, strlen(WAIT));
    }

    pthread_mutex_lock(&lock);
    if (client_count == group_size) {
        c = true;
        pthread_cond_broadcast(&cond);
    } else {
        while (!c) {
            pthread_cond_wait(&cond, &lock);
        }
    }
    pthread_mutex_unlock(&lock);
		int i = 0;
		while (qbuf[i] != NULL){
			// Send question
			char question_msg[BUFSIZE];
			winner_set = false;
			winner_name[0] = '\0';
			snprintf(question_msg, BUFSIZE, "QUES|%ld|%s", strlen(qbuf[i]->qtext), qbuf[i]->qtext);
			write(ssock, question_msg, strlen(question_msg));

			// Wait for answer from client
			if ((cc = read(ssock, buf, BUFSIZE)) <= 0) {
					printf("The client has gone.\n");
					close(ssock);
					pthread_exit(NULL);
			}

			//Find winner
			pthread_mutex_lock(&winner_mutex);
			if (!winner_set) {
				 	printf("%s", clients[client_index].name);
					strncpy(winner_name, clients[client_index].name, NBUF);
					winner_set = true;
			}
			pthread_mutex_unlock(&winner_mutex);

			pthread_mutex_lock(&lock);
			answers_received++;
			if (answers_received == group_size) {
					pthread_cond_broadcast(&cond);
			} else {
					while (answers_received < group_size) {
							pthread_cond_wait(&cond, &lock);
					}
			}
			pthread_mutex_unlock(&lock);

			//Send winner announcement
			char win_msg[BUFSIZE];
			snprintf(win_msg, BUFSIZE, "WIN|%s\r\n", winner_name);
			//printf("*%s*\n", win_msg);
			write(ssock, win_msg, strlen(win_msg));
			i++;
		}
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    char *service;
    struct sockaddr_in fsin;
    int alen = sizeof(fsin);
    int msock;
    int ssock;
    int rport = 0;

    switch (argc) {
        case 1:
            rport = 1;
            break;
        case 2:
            service = argv[1];
            break;
        case 3:
            read_questions(argv[1], qbuf);
            service = argv[2];
            break;
        default:
            fprintf(stderr, "usage: server [port]\n");
            exit(-1);
    }

    msock = passivesock(service, "tcp", QLEN, &rport);
    if (rport) {
        printf("server: port %d\n", rport);	
        fflush(stdout);
    }

    for (;;) {
        int ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
        pthread_t thr;

        if (ssock < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            break;
        }

        printf("A client has arrived for echoes - serving on fd %d.\n", ssock);
        fflush(stdout);

        pthread_create(&thr, NULL, client, (void *)(intptr_t)ssock);
    }

    pthread_exit(NULL);
}
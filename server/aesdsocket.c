/**
 * @file    aesdsocket.c
 * @brief   Code to create a socket, receive packet. Local port 9000. Store in file, 
 *	    send it back to local client. accepts -d argument to run as daemon
 *	    in the background. SIGINT and SIGTERM are the only signals possible
 * 	    and are handled.
 *			
 * @author	Venkat Sai Krishna Tata
 * @Date	10/02/2021
 * @References: Textbook: Linux System Programming
 *		https://stackoverflow.com/questions/803776/help-comparing-an-argv-string
 *		https://beej.us/guide/bgnet/html/
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>	
#include <errno.h>
#include "queue.h"
#include <stdbool.h>


#define PORT "9000"
#define BACKLOG 10
#define CHUNK_SIZE 500
#define TEST_FILE  "/var/tmp/aesdsocketdata"
int timestamp_len=0;
int serv_sock_fd,client_sock_fd,output_file_fd, total_length,len,capacity,counter=1,close_err;
struct sockaddr_in conn_addr;
char IP_addr[INET6_ADDRSTRLEN];
timer_t timerid;
sigset_t socket_set;

typedef struct{
    pthread_t threads;
    int fd;
    int client_socket;
    sigset_t mask;
    bool thread_complete_success;
}threadParams_t;

static inline void timespec_add( struct timespec *result,
                        const struct timespec *ts_1, const struct timespec *ts_2)
{
    result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
    result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
    if( result->tv_nsec > 1000000000L ) {
        result->tv_nsec -= 1000000000L;
        result->tv_sec ++;
    }
}

//Reference : https://blog.taborkelly.net/programming/c/2016/01/09/sys-queue-example.html
typedef struct slist_data_s slist_data_t;
struct slist_data_s{
    threadParams_t threadParams;
    SLIST_ENTRY(slist_data_s) entries;
};

slist_data_t *slist_ptr = NULL;
SLIST_HEAD(slisthead,slist_data_s) head;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thread_data
{
    int fd;
    
}thread_data;


//Function handles all open files when there is an error
void close_all()
{
	//Functions called due to an error, hence close files errno not 
	//checked in this function
	//All close errors handled in signal handler when no error occured
	close(serv_sock_fd);
	//Close regular file - output file fd
	close(output_file_fd);
	
	//After completing above procedure successfuly, exit logged
	syslog(LOG_DEBUG,"Caught signal, exiting");
	
	//Close the syslog once all of logging is complete
	closelog();
	
	//Delete and unlink the file
	remove(TEST_FILE);
	
    // free Linked list
    while(!SLIST_EMPTY(&head))
    {
        slist_ptr = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head,entries);
        free(slist_ptr);
    }
	
    if(timer_delete(timerid) == -1)
    {
		perror("timer delete error");
	}
	
	
    // close log
    closelog();

}

static void timer_thread(union sigval sigval)
{
	int rc=pthread_mutex_lock(&file_mutex);
	if(rc !=0)
	{
		close_all();
		exit(-1);
	}
    
	struct thread_data *td = (struct thread_data*) sigval.sival_ptr;
    char time_string[45];

    time_t rtime;
	time(&rtime);
	 
    size_t size= strftime(time_string,100,"timestamp:%a, %d %b %Y %T %z\n",localtime(&rtime));

    int merr=sigprocmask(SIG_BLOCK, &socket_set, NULL);
	if(merr == -1)
	{
		perror("sigprocmask unblock error");
		close_all();
		exit(-1);
	}
	timestamp_len=size;

    // Write to file
    int wbytes = write(td->fd,time_string,size);
    if (wbytes == -1){
        perror("write error");
        close_all();
        exit(-1);
    }
    
    rc=pthread_mutex_unlock(&file_mutex);
    if(rc !=0)
    {
		close_all();
		exit(-1);
	}
	merr=sigprocmask(SIG_UNBLOCK, &socket_set, NULL);
	if(merr == -1)
	{
		perror("sigprocmask unblock error");
		close_all();
		exit(-1);
	}
	
	

}

//Below function referenced from https://beej.us/guide/bgnet/html/
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Signal handler for Signals SIGTERM and SIGINT
static void signal_handler(int signo)
{
	//thread safe disabling of both reading and writing
	shutdown(serv_sock_fd,SHUT_RDWR);	
}


void handle_connection(void *threadp)
{
	threadParams_t *threadsock = (threadParams_t*)threadp;
	//Allocates buffer and initializes value to 0, same as malloc
	char *buf_data=calloc(CHUNK_SIZE,sizeof(char));
	if(buf_data==NULL)
	{
		syslog(LOG_ERR,"calloc failed");
		close_all();
		exit(-1);
	}
	int loc=0;

	//block the signals before recieving packets and sending back
	int merr=sigprocmask(SIG_BLOCK, &(threadsock->mask),  NULL);
	if(merr == -1)
	{
		perror("sigprocmask block error");
		close_all();
		exit(-1);
	}
	//Recieve the data and store in updated location always if available
	while((len=recv(threadsock->client_socket , buf_data + loc , CHUNK_SIZE , 0))>0)
	{
		if(len == -1)
		{
			perror("recv error");
			close_all();
			exit(-1);
		}
		
		if(strchr(buf_data ,'\n') != NULL)
			break;
		//Update the current location if newline not found
		loc+=len;
		
		//if no new line character, need to add more memory and dynamically
		//reallocate with an extra chunk
		counter++;
		buf_data=(char*)realloc(buf_data,((counter*CHUNK_SIZE)*sizeof(char)));
		if(buf_data==NULL)
		{
			syslog(LOG_ERR,"error realloc");
			close_all();
			exit(-1);
		}
	}
	
	 // mutex lock
    int rc=pthread_mutex_lock(&file_mutex);
	if(rc !=0)
    {
		close_all();
		exit(-1);
	}	
	//Write to file and position is updated
	int werr = write(threadsock->fd,buf_data,strlen(buf_data));
	if (werr == -1)
	{
		perror("write error");
		close_all();
		exit(-1);
	}
	
	//Store last position of file
	//int last_position = lseek(output_file_fd, 0, SEEK_CUR);
	total_length += (strlen(buf_data));
	//Place position ot beginnging
	lseek(threadsock->fd, 0, SEEK_SET);
	char * send_data_buf=calloc(total_length+timestamp_len,sizeof(char));
	if(send_data_buf == NULL)
	{
		perror("calloc error");
		close_all();
		exit(-1);
	}
	int rerr=read(threadsock->fd,send_data_buf,total_length+timestamp_len);
	//syslog(LOG_DEBUG,"read length %d",total_length);
	if (rerr == -1)
	{
		perror("read error");
		close_all();
		exit(-1);
	}
	
	//syslog(LOG_DEBUG," data to send %s",send_data_buf);
	//Send contents of buffer to client back
	int serr=send(threadsock->client_socket,send_data_buf,total_length+timestamp_len, 0);
	if(serr == -1)
	{
		close_all();
		perror("send error");
		exit(-1);
	}
	
	free(buf_data);
	free(send_data_buf);
	
	rc=pthread_mutex_unlock(&file_mutex);
	if(rc !=0)
    {
		close_all();
		exit(-1);
	}

	threadsock->thread_complete_success = true;
	//Once complete, buffer cleared

	
	//Unblock the set of signals once complete
	merr=sigprocmask(SIG_UNBLOCK, &(threadsock->mask), NULL);
	if(merr == -1)
	{
		perror("sigprocmask unblock error");
		close_all();
		exit(-1);
	}
	
	close(threadsock->client_socket);
}

int main(int argc, char *argv[])
{
	//Opens a connection with facility as LOG_USER to Syslog.
	openlog("aesdsocket",0,LOG_USER);
	
	//Registering signal_handler as the handler for the signals SIGTERM 
	//and SIGINT
	//Reference: Ch 10 signals, Textbook: Linux System Programming
	if (signal(SIGINT, signal_handler) == SIG_ERR) 
	{
		fprintf (stderr, "Cannot handle SIGINT\n");
		exit (EXIT_FAILURE);
	}
	
	if (signal(SIGTERM, signal_handler) == SIG_ERR) 
	{
		fprintf (stderr, "Cannot handle SIGTERM\n");
		exit (EXIT_FAILURE);
	}
	
	//Adding only signals SIGINT and SIGTERM to an empty set to enable only them
	
	int rc = sigemptyset(&socket_set);
	if(rc !=0)
	{
		perror("signal empty set error");
		exit(-1);
	}
	rc = sigaddset(&socket_set,SIGINT);
	if(rc !=0)
	{
		perror("error adding signal SIGINT to set");
		exit(-1);
	}
	rc = sigaddset(&socket_set,SIGTERM);
	if(rc !=0)
	{
		perror("error adding signal SIGTERM to set");
		exit(-1);
	}
	
	//With node as null and ai_flags as AI_PASSIVE, the socket address 
	//will be suitable for binding a socket that will accept connections
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo *res;
	
	//Node NULL and service set with port number as 9000, the pointer to
	//linked list returned and stored in res
	if (getaddrinfo(NULL, PORT , &hints, &res) != 0) 
	{
		perror("getaddrinfo error");
		exit(-1);
	}

    //Creating an end point for communication with type = SOCK_STREAM(connection oriented)
	//and protocol =0 which allows to use appropriate protocol (TCP) here
	serv_sock_fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
	if(serv_sock_fd==-1)
	{
		perror("socket error");
		exit(-1);
	}
		
	//Set options on socket to prevent binding errors from ocurring
	int dummie =1;
	if (setsockopt(serv_sock_fd, SOL_SOCKET, SO_REUSEADDR, &dummie, sizeof(int)) == -1) 
	{	
		
		perror("setsockopt error");
    }
    
	//Assign address to the socket created
	rc=bind(serv_sock_fd, res->ai_addr, res->ai_addrlen);
	if(rc==-1)
	{
		perror("bind error");
		exit(-1);
	}
	
	//Below conditional check referenced from https://stackoverflow.com/questions/803776/help-comparing-an-argv-string
	if(argc==2 && (!strcmp(argv[1], "-d")))
	{
		//Below code reference from chapter 5 of textbook reference
		//Child forked to create a daemon custom named customd
		pid_t customd;
		customd = fork();
		if (customd == -1)
		{
			perror("fork error");
			exit(-1);
		}
		else if (customd != 0 )
		{
			//Exit parent, to make child get reparented to init_parent
			exit (EXIT_SUCCESS);
		}

		//Create new session and process group
		if(setsid() == -1) 
		{
			perror("setsid");
			exit(-1);
		}
		

		//Set working to root directory
		if (chdir("/") == -1)
		{
			exit(-1);
		}
		
		//Should close if any open files, skipping that helre
		//Redirect Stdin,stdout,stderr to /dev/null
		open ("/dev/null", O_RDWR); 
		dup (0); 
		dup (0); 
	}
	
	//If bind passes, open a new file to store the packet that will be read
	output_file_fd=open(TEST_FILE,O_CREAT|O_RDWR|O_APPEND|O_SYNC,0644);
	if(output_file_fd == -1)
	{
		perror("error opening file at /var/temp/aesdsocketdata");
		exit(-1);
	}
	
	//Post binding, the server is running on the port 9000, so now
	//remote connections (client) can listen to server
	//Backlog - queue of incoming connections before being accepted to send/recv 
	//backlog set as 4, can be reduced to a lower number since we get only 1 connection
	if(listen(serv_sock_fd,BACKLOG)==-1)
	{
		perror("error listening");
		exit(-1);
	}
	freeaddrinfo(res);

    thread_data td;
    td.fd = output_file_fd;
	struct sigevent sev;
	int clock_id = CLOCK_MONOTONIC;
    memset(&sev,0,sizeof(struct sigevent));
	
	//https://github.com/cu-ecen-aeld/aesd-lectures/blob/master/lecture9/timer_thread.c
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = &td;
    sev.sigev_notify_function = timer_thread;
    
    struct itimerspec itimerspec;
    struct timespec start_time;
    
    //itimerspec.it_value.tv_sec = 10;
    //itimerspec.it_value.tv_nsec = 0;
    itimerspec.it_interval.tv_sec = 10;
    itimerspec.it_interval.tv_nsec = 0;
    if ( timer_create(clock_id,&sev,&timerid) != 0 ) 
    {
        perror("error timer create");
        close_all();
        exit(-1);
    }

    if ( clock_gettime(clock_id,&start_time) != 0 ) 
    {
        perror("error clock_gettime");
        close_all();
        exit(-1);
    } 
	
	timespec_add(&itimerspec.it_value,&start_time,&itimerspec.it_interval);
	
    if( timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL ) != 0 )
    {
        perror("error timer_settime");
        close_all();
        exit(-1);
    } 
    
	while(1)
	{
		
		socklen_t conn_addr_len=sizeof(conn_addr);
		//Accept an incoming connection
		client_sock_fd = accept(serv_sock_fd, (struct sockaddr *)&conn_addr, &conn_addr_len);
		if(client_sock_fd ==-1)
		{
			//If no incoming connection, graceful termination done
			close_all();
			exit(-1);
		}
		
		//Below line of code reference: https://beej.us/guide/bgnet/html/
		//Check if the connection address family is IPv4 or IPv6 and retrieve accordingly
		//Convert the binary to text before logging 
		inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&conn_addr), IP_addr, sizeof(IP_addr));
        syslog(LOG_DEBUG,"Accepted connection from %s", IP_addr);   
		
		//Singely linked list to manage thread parameters
		slist_ptr = (slist_data_t*)malloc(sizeof(slist_data_t));
		SLIST_INSERT_HEAD(&head,slist_ptr,entries);
		slist_ptr->threadParams.mask = socket_set;
		slist_ptr->threadParams.client_socket = client_sock_fd;
		slist_ptr->threadParams.fd=output_file_fd;
		slist_ptr->threadParams.thread_complete_success = false;
		
		pthread_create(&(slist_ptr->threadParams.threads),NULL,(void*)&handle_connection,(void*)&(slist_ptr->threadParams));
		
		SLIST_FOREACH(slist_ptr,&head,entries)
		{    
				pthread_join(slist_ptr->threadParams.threads,NULL);
        }
	}
	return 0;
}	

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include "queue.h"

#define PORT "9000"
#define CHUNK_SIZE 500
#define BACKLOG 10
#define TEST_FILE "/var/tmp/aesdsocketdata"
#define BEGIN 0
typedef struct{
	pthread_mutex_t *lock;
    bool thread_complete;
    pthread_t threads;               
    int fd;                            
    int client_socket;                          
    char* data_buf;                    
    char* send_data_buf;
    sigset_t mask;
}threadParams_t;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef struct slist_data_s slist_data_t;
struct slist_data_s{

    threadParams_t threadParams;
    SLIST_ENTRY(slist_data_s) entries;
};


slist_data_t *slist_ptr = NULL;
SLIST_HEAD(slisthead,slist_data_s) head;



char IP_addr[INET6_ADDRSTRLEN];
pthread_mutex_t mutex_test;
timer_t timerid;
int serv_sock_fd,client_sock_fd,counter =1,output_file_fd,finish;
struct sockaddr_in conn_addr;


typedef struct sigsev_data{

    int fd; 

}sigsev_data;


void close_all();

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



static void timer_handle(union sigval sigval){

    struct sigsev_data* td = (struct sigsev_data*) sigval.sival_ptr;

    char timer_buffer[100];
    time_t rtime;
    struct tm *info;
    time(&rtime);
    info = localtime(&rtime);                

    int time_length= strftime(timer_buffer,100,"timestamp:%a, %d %b %Y %T %z\n",info);

    if (pthread_mutex_lock(&mutex_test) != 0)
    {
        perror("error pthread mutex lock");
        close_all();
        exit(-1);
    }
    
    int test = write(td->fd,timer_buffer,time_length);
    if (test == -1){
        perror("error write");
        close_all();
        exit(-1);
    }

    if (pthread_mutex_unlock(&mutex_test) != 0)
    {
        perror("error pthread mutex unlock");
        close_all();
        exit(-1);
    }
}


void close_all(){

    finish = 1;
    close(serv_sock_fd);
    close(output_file_fd);

    if(remove(TEST_FILE) != 0){
        syslog(LOG_ERR,"error remove");
    }
    
    SLIST_FOREACH(slist_ptr,&head,entries){

        if (slist_ptr->threadParams.thread_complete != true){
            pthread_cancel(slist_ptr->threadParams.threads);
            //free(slist_ptr->threadParams.data_buf);
            //free(slist_ptr->threadParams.send_data_buf);   
        }
    }
    
    while(!SLIST_EMPTY(&head)){
        slist_ptr = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head,entries);
        free(slist_ptr);
    }
    pthread_mutex_destroy(&mutex_test);           
    timer_delete(timerid);                                
    closelog();
}


static void signal_handler(int signo)
{
   if(signo == SIGINT || signo==SIGTERM) 
    {
		shutdown(serv_sock_fd,SHUT_RDWR);
		finish = 1;    
    }
}


void* handle_connection(void *threadp)
{

    threadParams_t *thread_handle_sock = (threadParams_t*)threadp;
    int loc=0;
      
    char *buffer2;
    thread_handle_sock->data_buf = calloc(CHUNK_SIZE,sizeof(char));
    int len;
  
	while((len=recv(thread_handle_sock->client_socket , thread_handle_sock->data_buf + loc , CHUNK_SIZE , 0))>0)
	{
		if(len == -1)
		{
			perror("recv error");
			close_all();
			exit(-1);
		}
		//Update the current location if newline not found
		loc+=len;
		if(strchr(thread_handle_sock->data_buf ,'\n') != NULL)
			break;
		
		
		//if no new line character, need to add more memory and dynamically
		//reallocate with an extra chunk
		counter++;
		thread_handle_sock->data_buf=(char*)realloc(thread_handle_sock->data_buf,((counter*CHUNK_SIZE)*sizeof(char)));
		if(thread_handle_sock->data_buf==NULL)
		{
			syslog(LOG_ERR,"error realloc");
			close_all();
			exit(-1);
		}
	}

    if (pthread_mutex_lock( thread_handle_sock->lock) == -1)
    {
        perror("mutex lock error");
        close_all();
        exit(-1);

    }

    if (sigprocmask(SIG_BLOCK,&(thread_handle_sock->mask),NULL) == -1)
    {
        perror("sigprocmask block error");
        close_all();
        exit(-1);
    }

    
    int werr = write(thread_handle_sock->fd,thread_handle_sock->data_buf,loc);
    if (werr == -1)
    {
        perror("write error");
        close_all();
        exit(-1);
    }
	lseek(thread_handle_sock->fd,BEGIN,SEEK_SET);
    if (sigprocmask(SIG_UNBLOCK,&(thread_handle_sock->mask),NULL) == -1)
    {
        perror("sigprocmask unblock error");
        close_all();
        exit(-1);
    }
    if(pthread_mutex_unlock(thread_handle_sock->lock) != 0)
    {
        perror("mutex unlock error");
        close_all();
        exit(-1);
    }


    int start = 0;
    int extra=0;
    char single_byte;
    int outbuf_size = CHUNK_SIZE;
	ssize_t rerr; 
    if (sigprocmask(SIG_BLOCK,&(thread_handle_sock->mask),NULL) == -1)
    {
        perror("sigprocmask signal block");
        close_all();
        exit(-1);
    }

    if (pthread_mutex_lock( thread_handle_sock->lock) != 0){

        perror("mutex lock error");
        close_all();
        exit(-1);

    }

    thread_handle_sock->send_data_buf = calloc(CHUNK_SIZE,sizeof(char));
    
    /*while((rerr = read(thread_handle_sock->fd,&thread_handle_sock->send_data_buf+start,CHUNK_SIZE)) > 0)
    {
		if(rerr <0 ) {

             perror("read error");
             close_all();
             exit(-1);

        }
        extra += rerr;
        //thread_handle_sock->send_data_buf[start] = single_byte;
        if(strchr(thread_handle_sock->send_data_buf ,'\n') == NULL)
        { 
			break;
			 
            
           
        }

        start+=CHUNK_SIZE;

        if(start >= outbuf_size){
            
            outbuf_size += CHUNK_SIZE;
            buffer2=realloc(thread_handle_sock->send_data_buf,sizeof(char)*outbuf_size);
            thread_handle_sock->send_data_buf=buffer2;

        }
				
	}
	if (send(thread_handle_sock->client_socket,thread_handle_sock->send_data_buf,extra, 0) == -1)
	{ 
		perror("error send");
		close_all();
		//exit(-1);
	}
		*/
    
    while((rerr = read(thread_handle_sock->fd,&single_byte,1)) > 0){

        if(rerr <0 ) {

             perror("read error");
             close_all();
             exit(-1);

        }
        thread_handle_sock->send_data_buf[start] = single_byte;
        if(thread_handle_sock->send_data_buf[start] == '\n')
        {   
            if (send(thread_handle_sock->client_socket,thread_handle_sock->send_data_buf+extra,start - extra + 1, 0) == -1)
            { 
                perror("error send");
                close_all();
                //exit(-1);
            }
            extra = start + 1;
        }

        start++;

        if(start >= outbuf_size){
            
            outbuf_size += CHUNK_SIZE;
            buffer2=realloc(thread_handle_sock->send_data_buf,sizeof(char)*outbuf_size);
            thread_handle_sock->send_data_buf=buffer2;

        }


    }
	thread_handle_sock->thread_complete = true;
    if (sigprocmask(SIG_UNBLOCK,&(thread_handle_sock->mask),NULL) == -1){
        perror("sigprocmask unblock error");
        close_all();
        exit(-1);
    }


    if (pthread_mutex_unlock(thread_handle_sock->lock) != 0){

        perror("mutex unlock error");
        close_all();
        exit(-1);
    }
    close(thread_handle_sock->client_socket);
    free(thread_handle_sock->data_buf);
    free(thread_handle_sock->send_data_buf);
    return threadp;
}



int main(int argc, char* argv[])
{

	//Opens a connection with facility as LOG_USER to Syslog.
	openlog("aesdsocket",0,LOG_USER);
    

    SLIST_INIT(&head);

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
	int rc=bind(serv_sock_fd, res->ai_addr, res->ai_addrlen);
	if(rc==-1)
	{
		perror("bind error");
		exit(-1);
	}
  
    freeaddrinfo(res);

	//Registering signal_handler as the handler for the signals SIGTERM 
	//and SIGINT
	//Reference: Ch 10 signals, Textbook: Linux System Programming
	sigset_t socket_set;
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
	
	rc = sigemptyset(&socket_set);
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

	//Post binding, the server is running on the port 9000, so now
	//remote connections (client) can listen to server
	//Backlog - queue of incoming connections before being accepted to send/recv 
	//backlog set as 4, can be reduced to a lower number since we get only 1 connection
	if(listen(serv_sock_fd,BACKLOG)==-1)
	{
		perror("error listening");
		exit(-1);
	}

	output_file_fd=open(TEST_FILE,O_CREAT|O_RDWR|O_APPEND,0644);
	if(output_file_fd == -1)
	{
		perror("error opening file at /var/temp/aesdsocketdata");
		exit(-1);
	}
    
    if (pthread_mutex_init(&mutex_test,NULL) != 0)
    {
        perror("dynamic mutex init error");
        close_all();
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

	//Below timer reference : https://github.com/cu-ecen-aeld/aesd-lectures/blob/master/lecture9/timer_thread.c
    struct sigevent sev;
    sigsev_data td;
    td.fd = output_file_fd;
    memset(&sev,0,sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = &td;
    sev.sigev_notify_function = timer_handle;
    if ( timer_create(CLOCK_MONOTONIC,&sev,&timerid) != 0 ) {
        perror("timer create error");
    }
    struct timespec start_time;

     if ( clock_gettime(CLOCK_MONOTONIC,&start_time) != 0 ) {
        perror("clock gettime error");
    } 

    struct itimerspec itimerspec;
    itimerspec.it_value.tv_sec = 10;
    itimerspec.it_value.tv_nsec = 0;
    itimerspec.it_interval.tv_sec = 10;
    itimerspec.it_interval.tv_nsec = 0;    
    
    if( timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL ) != 0 ) {
        perror("settime error");
    } 

    slist_data_t *temp_node = NULL;

    while(1) 
    {
		socklen_t conn_addr_len = sizeof(conn_addr);
		//Accept an incoming connection
		client_sock_fd = accept(serv_sock_fd,(struct sockaddr *)&conn_addr,&conn_addr_len);
		if(client_sock_fd ==-1)
		{
			//If no incoming connection, graceful termination done
			perror("client error");

		}
		if(finish) 
			break;

		//Below line of code reference: https://beej.us/guide/bgnet/html/
		//Check if the connection address family is IPv4 or IPv6 and retrieve accordingly
		//Convert the binary to text before logging 
		inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&conn_addr), IP_addr, sizeof(IP_addr));
        syslog(LOG_DEBUG,"Accepted connection from %s", IP_addr);

		slist_ptr = (slist_data_t*)malloc(sizeof(slist_data_t));
		SLIST_INSERT_HEAD(&head,slist_ptr,entries);                                          
		slist_ptr->threadParams.client_socket = client_sock_fd;
		slist_ptr->threadParams.thread_complete = false;
		slist_ptr->threadParams.fd=output_file_fd;
		slist_ptr->threadParams.mask = socket_set;
		slist_ptr->threadParams.lock = &mutex_test;

		if (pthread_create(&(slist_ptr->threadParams.threads),(void*)0,&handle_connection,(void*)&(slist_ptr->threadParams)) != 0){

			perror("pthread error");
			close_all();
			exit(-1);
		}

		SLIST_FOREACH_SAFE(slist_ptr,&head,entries,temp_node )
		{
			if (slist_ptr->threadParams.thread_complete == true){
				pthread_join(slist_ptr->threadParams.threads,NULL);
				SLIST_REMOVE(&head,slist_ptr,slist_data_s,entries);
				free(slist_ptr);
				slist_ptr=NULL;
			}
		}
	}
	close_all();
	return 0;
}

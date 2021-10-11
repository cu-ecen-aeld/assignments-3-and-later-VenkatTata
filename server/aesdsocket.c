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



typedef struct slist_data_s slist_data_t;
struct slist_data_s{

    threadParams_t threadParams;
    SLIST_ENTRY(slist_data_s) entries;
};


slist_data_t *slist_ptr = NULL;
SLIST_HEAD(slisthead,slist_data_s) head;




pthread_mutex_t mutex_test;

pid_t check;
timer_t timerid;
int serv_sock_fd,client_sock_fd,counter =1,output_file_fd,finish;

struct addrinfo host;
struct addrinfo *servinfo;

struct sockaddr_in con_addr;


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


static void sig_handler(int signo){


    if(signo == SIGINT || signo==SIGTERM) {


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
	ssize_t rbytes; 
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
    while((rbytes = read(thread_handle_sock->fd,&single_byte,1)) > 0){

        if(rbytes <0 ) {

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

    if (pthread_mutex_init(&mutex_test,NULL) != 0)
    {
        perror("dynamic mutex init error");
        close_all();
        exit(-1);
    } 

    SLIST_INIT(&head);

    socklen_t len;

    memset(&host, 0, sizeof(host)); 

    host.ai_family = AF_UNSPEC;     
    host.ai_socktype = SOCK_STREAM; 
    host.ai_flags = AI_PASSIVE;     

    openlog("AESDSOCKET :",LOG_PID,LOG_USER);

    check = getaddrinfo(NULL, PORT, &host, &servinfo);

    if (check != 0){
       perror("getaddrinfor error");
        close_all();
        exit(-1);
    }

    serv_sock_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if(serv_sock_fd == -1) {
        perror("socket error");
        close_all();
        exit(-1);
    }

    int reuse_addr = 1;

    if (setsockopt(serv_sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) <0) 
    {
        syslog(LOG_ERR, "reuse socket error");
        close(serv_sock_fd);
        close_all();
        exit(-1);
    } 


    check = bind(serv_sock_fd,servinfo->ai_addr, servinfo->ai_addrlen);

    if (check == -1)
    {
        perror("bind error");
        freeaddrinfo(servinfo);
        close_all();
        exit(-1);
    }

        
  
    freeaddrinfo(servinfo);

 
    if(signal(SIGINT,sig_handler) == SIG_ERR)
    {
        syslog(LOG_ERR,"sigint error");
        close_all();
        exit(-1);
    }
    
    
    if(signal(SIGTERM,sig_handler) == SIG_ERR){
        syslog(LOG_ERR,"sigterm error");
        close_all();
        exit(-1);
    }

  
    sigset_t set;
    sigemptyset(&set);          

    
    sigaddset(&set,SIGINT);      
    sigaddset(&set,SIGTERM);     

    
    
    check = listen(serv_sock_fd, BACKLOG);

    if (check == -1){

        perror("\nERROR listen():");
        close_all();
        exit(-1);

    }

    output_file_fd =  open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_APPEND,S_IRWXU);
    if(output_file_fd<0){
        perror("\nERROR open():");
        close_all();
        exit(-1);
    }
    
    
    if (argc == 2){

        if (!strcmp("-d",argv[1])){

        check = fork();
        if (check == -1){

            perror("fork error");
            close_all();
            exit(-1);
        }

        if (check != 0){
            exit(0);
        }

        setsid();

        chdir("/");

        open("/dev/null", O_RDWR);
		dup(0);
		dup(0);

    }

    }

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
    itimerspec.it_interval.tv_sec = 10;
    itimerspec.it_interval.tv_nsec = 0;

    timespec_add(&itimerspec.it_value,&start_time,&itimerspec.it_interval);
    
    
    if( timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL ) != 0 ) {
        perror("settime error");
    } 

    slist_data_t *temp_node = NULL;

    while(!finish) {


    len = sizeof(con_addr);

   
    client_sock_fd = accept(serv_sock_fd,(struct sockaddr *)&con_addr,&len);

    if(finish) break;

    if (client_sock_fd == -1 ){
        perror("client error");
    }

    char *ip_string = inet_ntoa(con_addr.sin_addr);
    
    syslog(LOG_DEBUG,"Accepted connection from %s",ip_string);
    
    slist_ptr = (slist_data_t*)malloc(sizeof(slist_data_t));
    SLIST_INSERT_HEAD(&head,slist_ptr,entries);                                          
    slist_ptr->threadParams.client_socket = client_sock_fd;
    slist_ptr->threadParams.thread_complete = false;
    slist_ptr->threadParams.fd=output_file_fd;
    slist_ptr->threadParams.mask = set;
    slist_ptr->threadParams.lock = &mutex_test;
    

    
    if (pthread_create(&(slist_ptr->threadParams.threads),(void*)0,&handle_connection,(void*)&(slist_ptr->threadParams)) != 0){

        perror("Error creating thread:");
        close_all();
        exit(-1);
    }

    SLIST_FOREACH_SAFE(slist_ptr,&head,entries,temp_node ){
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

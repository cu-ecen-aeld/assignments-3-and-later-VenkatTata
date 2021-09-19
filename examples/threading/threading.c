/**
 * @file    threading.c
 * @brief   Code to create a thread with starter routine that sleeps, locks mutex, sleeps again, releases mutex and joins back to the 
 * 			parent.
 * @author	Venkat Sai Krishna Tata
 * @Date	09/17/2021
 *
 */
 
#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)


/**
 * @param thread_param the argument to the starter routine 
 * @return true if sleep before and after locking and unlocking mutex is 
 *   successfully executed, false if an error occurred, 
 *   either in invocation of the usleep() or mutex calls
*/

void* threadfunc(void* thread_param)
{
	//Initialise the thread parameters by type casting the void pointer
	struct thread_data* thread_func_args = (struct thread_data *) thread_param;
	
	int rc;
	//Initially set thread complete status as true
	thread_func_args->thread_complete_success=true;
	//Suspends the action for the microseconds as specified in the argument
	//Value in milliseconds converted to microseconds
	rc=usleep(thread_func_args->wait_ms_obtain*(CONVERT_MS_to_US));
	if(rc!=0)
	{
			thread_func_args->thread_complete_success=false;
	}
	//Lock the mutex
	rc=pthread_mutex_lock(thread_func_args->mutex_lock);
	if ( rc != 0 )
	{
		thread_func_args->thread_complete_success=true;
	}
	//Suspends action after locking mutex
	rc = usleep(thread_func_args->wait_ms_release*(CONVERT_MS_to_US));
	if(rc!=0)
	{
			thread_func_args->thread_complete_success=false;
	}
	//Release the mutex lock
	rc=pthread_mutex_unlock(thread_func_args->mutex_lock);
	if(rc!=0)
	{
			thread_func_args->thread_complete_success=false;
	}
	//Equivalent to pthread_exit(3) with the value supplied in the return statement
    return thread_param;
}

/*Refer threading.h for details*/
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     * 
     * See implementation details in threading.h file comment block
     */    
    //Allocate space for the structure
    struct thread_data *thread_data_param=(struct thread_data*)malloc(sizeof(struct thread_data));
       
	//Setup wait and mutex arguments
	thread_data_param->wait_ms_obtain=wait_to_obtain_ms;
	thread_data_param->wait_ms_release=wait_to_release_ms;
	thread_data_param->mutex_lock=mutex;
	
	//Create thread
	int rc=pthread_create(thread,               // pointer to thread descriptor
                      NULL,         // use default attributes
                      &threadfunc,                 // thread function entry point
                      (void *)(thread_data_param) // parameters to pass in
                     );
    if(rc!=0)
    {
		return false;
	}
	//If thread created and joined successfully, return true to the caller
	return true;
}

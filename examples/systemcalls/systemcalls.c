#include "systemcalls.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
/**
 * Author: Venkat Sai Krishna Tata
 * Date: 09/10/2021
 * Brief: Performs system calls using system() function, equivalent fork, execv, wait functionality 
 * 		  and also, redirects standard output to text file
 * Reference : https://stackoverflow.com/a/13784315/1446624
 * 			   Dan Walkes' starter code present in systemcalls.c  
*/

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{
	int ret_val=system(cmd);
	
	//On an invocation of system with a non- NULL command, zero value returned when shell is available
	//returns -1 when status of child process could not be retrieved, 
	//returns -127 when the shell could not be executed and returns exit code of system call if succeeds
	if(ret_val == 0 && cmd==NULL)
		return false;
	else if (ret_val == -1 || ret_val ==127)
		return false;
	else
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    int child_status;
    int exec_ret_value=0,ret_status;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
	}

    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];
    
    
    //Fork to create a child process while the parent process is intact
	pid_t pid;
	pid = fork();
	
	//If could not fork, no child created and pid return value is negative
	if(pid == -1)
		exec_ret_value=0;
	else if (pid == 0) //If current process is child process, execute command
	{
		ret_status=execv(command[0],command);
		if(ret_status==-1)
			exit(EXIT_FAILURE);
	}
	
	//Wait for child to terminate, returns -1 on error of any sort
	//else returns the exit code after replacing the child process and running the command provided in the shell
	//of the child. Based on the exit code, if command successfully executed, returns true else false
	if (waitpid (-1, &child_status, 0) == -1)
			exec_ret_value=0;
	else if (WIFEXITED (child_status))
	{
		if(WEXITSTATUS(child_status)==0)
			exec_ret_value=1;
		else 
			exec_ret_value=0;		
	}
	else
		exec_ret_value=0;

    va_end(args);

	//Return true or false based on the success of execution of provided command
    return exec_ret_value;
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int child_status;
    int i;
    int exec_ret_value=0,ret_status;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];	

//  redirect standard out to a file specified by outputfile
	int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	dup2(fd, 1);
	
	//Fork to create a child process while the parent process is intact    
	pid_t pid;
	pid = fork();
	
	//If could not fork, no child created and pid return value is negative
	if(pid == -1)
		exec_ret_value=0;
	else if (pid == 0) //If current process is child process, execute command
	{
		ret_status=execv(command[0],command);
		if(ret_status==-1)
			exit(EXIT_FAILURE);
	}
	
	//Wait for child to terminate, returns -1 on error of any sort
	//else returns the exit code after replacing the child process and running the command provided in the shell
	//of the child. Based on the exit code, if command successfully executed, returns true else false

	if (waitpid (-1, &child_status, 0) == -1)
			exec_ret_value=0;
	else if (WIFEXITED (child_status))
	{
		if(WEXITSTATUS(child_status)==0)
			exec_ret_value=1;
		else 
			exec_ret_value=0;		
	}
	else
		exec_ret_value=0;

	//Return true or false based on the success of execution of provided command
    va_end(args);
    
    //Close the file descriptor
    close(fd);
    return exec_ret_value;
}

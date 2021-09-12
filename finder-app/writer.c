/**
 * @file    writer.c
 * @brief   Code to write (or create, if not already present) to a file with the string passed as the second parameter. 
 * 			The first parameter being a valid file path.
 * @author	Venkat Sai Krishna Tata
 * @Date	09/05/2021
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
int main(int argc, char *argv[])
{
	//Opens a connection with facility as LOG_USER to Syslog.
	openlog("writer",0,LOG_USER);
	
	//If invalidn number of arguments, log error
	if(argc < 3)
	{
		syslog(LOG_ERR,"Error: Invalid number of arguments\n");
		return 1;
	}
	
	//File I/O to open file with read and write permissions for user
	int assign2_fd;
	assign2_fd=open(argv[1],O_CREAT|O_RDWR,S_IWUSR|S_IRUSR);
	
	//If path not correct, cannot create file and the error is logged
	//else, writes the string passed as second argument to the file passed as first
	//Also, logs the status of operation
	if(assign2_fd==-1)
	{
		syslog(LOG_ERR,"Could not create file\n");
	}
	else
	{
		int written_bytes=write(assign2_fd, argv[2], strlen(argv[2]));
		if(written_bytes!=strlen(argv[2]))
			syslog(LOG_ERR,"Did not complete write\n");
		else
			syslog(LOG_DEBUG,"Writing %s to %s",argv[2],argv[1]);
	}
	close(assign2_fd);
	return 0;
}

#!/bin/bash
#Script to write (or create, if not already present) to a file with the string
#passed as the second parameter. The first parameter being a valid file path
#Author: Venkat Tata
#Date: 08/30/21

#Checks if the parameters are as specified, if not, notifies what is missing
#Exits with status 1 after printing the error message
if [ ! $# -eq 2 ]
then	
	if [ $# -lt 2 ] && [ -d $1 ]
	then
		echo "ERROR: Not enough parameters, text string not entered"
	elif [ $# -lt 2 ] && [ -n $1 ] 
	then
		echo "ERROR: Not enough parameters, path not specified"
	else
		echo "ERROR: Too many parameters"
	fi
	exit 1
fi

#Creates the file if the path specified is valid (touch command error message not printed to terminal)
#if path is not valid, prints that the specified file path cannot be created
touch $1 > /dev/null 2>&1
if [ ! -f $1  ]
then
	echo "$1 cannot be created because the path is incorrect"
	exit 1
fi

#Overwrite the string to the file  specified if the parameters are correct
echo $2 > $1

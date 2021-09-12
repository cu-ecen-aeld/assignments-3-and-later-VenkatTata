#!/bin/sh
#Script to find the number of files in the path specified as first argument
#and the number of occurences of the keyword (specified as second argument) in those files
#Author: Venkat Tata
#Date: 08/30/21

#Checks if the parameters are as specified, if not, notifies what is missing
#Exits with status 1 after printing the error message
if [ ! $# -eq 2 ]
then	
	if [ $# -lt 2 ] && [ -d $1 ]
	then
		echo "ERROR:Not enough parameters, text string not entered"
	elif [ $# -lt 2 ] && [ -n $1 ] 
	then
		echo "ERROR:Not enough parameters, path not specified"
	else
		echo "ERROR:Too many parameters"
	fi
	echo "Usage: ./finder.sh [path_to_search] [string_to_be searched]"
	echo "Searches for files in the specified path for the string specified as second argument"
	exit 1
fi

#If the number of parameters are as expected, checks if the first parameter represents a valid directory
if [ ! -d $1  ]
then
	echo "ERROR: $1 not a directory"
	exit 1
fi

#If parameters are as specified, finds the number of files in the directory path specified and
#also, finds if the keyword specified as second parameter is present in those files. The number
#files and keyword occurences are printed onto the terminal
echo "The number of files are $(find "$1" -type f | wc -l) and the number of matching lines are $(grep -ior "$2" "$1" | wc -w)"

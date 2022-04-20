This program, written in C, represents a "small" version of a shell. The shell runs command line instructions that have been inputted by a user. The shell allows for the redirection of standard input and standard output and supports both foreground and background processes (which are controlled by the command line and by receiving signals). The shell also supports three built in commands: exit, cd, and status. 



# Instructions to run smallsh.c:

# Compile: gcc -std=gnu99 -Wall -g -o smallsh smallsh.c
# Run: ./smallsh

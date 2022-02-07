// Author: Nina Argade
// Project: Assignment 3 Smallsh, CS344 Winter 2022
// Date: 2/7/2022 

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

// global variable array to keep track of background PIDs
int backgroundPIDList[25];
int backgroundPIDListIndex = 0;

// global variable to keep track of exit status in foreground, initialized to 0
int childStatus = 0;

// global variable to keep track of foreground-only mode, initialized to False
int foregroundOnlyMode = 0;


/* struct for inputString tokens (to be stored in a linked-list) */
struct inputString
{
    char *word;

    struct inputString *next;
};


/* This signal handler displays an informative message when the user enters ^Z/ the SIGTSTP signal. This function will change 
global variable foregroundOnlyMode boolean from 0 to 1 and back again as indicated.
Arguments: int signo. global variable foregroundOnlyMode
Returns: prints message regarding entering/exiting foreground-only mode
Code citation: Exploration: Signal Handling API
*/
void handle_SIGTSTP(int signo){

    // if NOT foreground-only
    if (foregroundOnlyMode == 0) {
	    char* message = "\nEntering foreground-only mode (& is now ignored)\n: ";
	    write(STDOUT_FILENO, message, 52);
        foregroundOnlyMode = 1;
    }

    // if foreground-only
    else {
	    char* message = "\nExiting foreground-only mode\n: ";
	    write(STDOUT_FILENO, message, 32);
        foregroundOnlyMode = 0;
    }
}


/* This function checks what Background processes have terminated. It is called at every instance in this program 
before the prompt for a new command is displayed 
Arguments: global variable backgroundPIDList and backgroundPIDListIndex
Returns: when a background process terminates, will print a message showing the process id and exit/termination status.
Code citation: Exploration: Process API - Monitoring Child Processes
*/
void checkBackgroundPIDs() {

    // ofr-loop counter
    int i;

    // background PID
    int checkBackgroundPID;
    // exit/termination status
    int backgroundStatus;
    // result of waitpid()
    pid_t resultBackgroundPID;

    // iterate through all background PIDs
    for (i = 0; i < 25; i++) {

        // check each background PID for termination
        checkBackgroundPID = backgroundPIDList[i];

        // do not block if not terminated (WNOHANG)
        // if terminated, resultBackgroundPID will be > 0 and checkBackgroundPID will contain terminated background PID
        if ((resultBackgroundPID = waitpid(checkBackgroundPID, &backgroundStatus, WNOHANG) > 0)) {
                
                // print process id and exit status
                if (WIFEXITED(backgroundStatus)) {
                    printf("background pid %d is done: exit value %d\n", checkBackgroundPID, WEXITSTATUS(backgroundStatus));
                    fflush(stdout);

                // print process id and termination status   
                } else {
                    printf("background pid %d is done: terminated by signal %d\n", checkBackgroundPID, WTERMSIG(backgroundStatus));
                    fflush(stdout);
                }
            }
        }
}


/* function to fork child and process exec */
/* This function will fork off a child process, perform redirection of user input, and execute commands that are 
non built-in (in either foreground or background).
Arguments: array of input string pointers, boolean tag indicating background command status, output file name, 
input file name, ^Z signal structure; global variables childStatus and foregroundOnlyMode
Returns: will print output of redirection, execvp() results, or started background process PIDs
Code citation: Much of the following code is copied directly from Exploration: Process API - Monitoring Child Processes as 
well as Exploration: Processes and I/O 
*/
void processExec(char **inputArr, int backgroundBool, char inputFileName[], char outputFileName[], struct sigaction SIGINT_action) {

    pid_t spawnPid = -5;
    spawnPid = fork();

    switch (spawnPid) {

        case -1:
            
            perror("fork() failed!");
            fflush(stdout);
		    exit(1);
		    break;

        case 0:

            // if there is a specified input file
            if (strcmp(inputFileName, "") != 0) {

                // open for reading only
                int sourceFD = open(inputFileName, O_RDONLY);

                // if file cannot be opened
	            if (sourceFD == -1) { 
                    
                    // print error message
		            printf("cannot open %s for input\n", inputFileName); 
                    fflush(stdout);
		            exit(1); 
                }

                // perform input redirection
                int result = dup2(sourceFD, 0);
	            if (result == -1) { 
		            printf("cannot redirect to source"); 
                    fflush(stdout);
		            exit(2); 
	            }
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
            }

            // if there is a specified output file
            if (strcmp(outputFileName, "") != 0) {

                // open for writing only; truncate if it already exists or create if it does not exist
                int targetFD = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                // if file cannot be opended
	            if (targetFD == -1) { 
                    
                    // print error message
		            printf("cannot open %s for output\n", outputFileName); 
                    fflush(stdout);
		            exit(1); 
                }

                // perform output redirection
                int result = dup2(targetFD, 1);
	            if (result == -1) { 
		            printf("cannot redirect to target"); 
                    fflush(stdout);
		            exit(2); 
	            }

                // close file descriptor on exec()
                fcntl(targetFD, F_SETFD, FD_CLOEXEC);
            }

            // set ^C signal handler in child foreground process to terminate itself (SIG_DFL) when it receives SIGINT
            SIGINT_action.sa_handler = SIG_DFL;
            sigfillset(&SIGINT_action.sa_mask);
            SIGINT_action.sa_flags = 0;
	        sigaction(SIGINT, &SIGINT_action, NULL);

            // run exec() using execvp() 
            if (execvp(*inputArr, inputArr) == -1) {
                
                fflush(stdout);

                // if command fails (result of execvp() is -1)
                // print error message
                printf("%s: no such file or directory\n", *inputArr);
                fflush(stdout);
                // set exit status to 1
                exit(1);
            }
            
            break;

        default:
            
            // if it is a background process and we have NOT entered foreground-only mode
            if (backgroundBool == 1 && foregroundOnlyMode == 0) {

                // do not block until child terminates; do not wait for command to complete
                pid_t childPid = waitpid(spawnPid, childStatus, WNOHANG);

                // print background PID
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
                
                // add background PID to array of background PIDs
                backgroundPIDList[backgroundPIDListIndex] = spawnPid;
                backgroundPIDListIndex++;
                
            
            }

            // if foreground process
            else {
                // block until child terminates; wait for command to complete
                pid_t childPid = waitpid(spawnPid, &childStatus, 0);  
            }
    }
}


/* This function processes the linked-list of tokens into an array of token strings. It will analyze all tokens to determine if
the input is a comment "#", background process "&", or requres redirection "<" or ">". It will also perform variable expansion "$$"
if indicated. The function then processes the built-in commands "exit," "cd," or "status" based on the first token. If one of these 
built-in commands is not specified, this function will call the processExec() function to execute execvp().
Arguments: first token (first command), linked-list of parsed command line tokens, PID number, ^C signal structure
Returns: nothing; will print output of built-in commands 
*/
int processInput(char firstToken[], struct inputString *list, int pidNumber, struct sigaction SIGINT_action) {

    // count total number of tokens in input by iterating through linked list
    // initialize at 1 for first token
    int listCount = 1;
    struct inputString *tempCount = list;
    struct inputString *tempCopyInput = list;

    // count remaining tokens
    while (tempCount != NULL) {
        listCount++;
        tempCount = tempCount->next;
    }

    // create an appropriately-sized pointer array of command line tokens from linked-list
    // make last element pointer NULL
    int i;
    char *inputArr[listCount + 1];
    for (i = 0; i <= (listCount); i++) {
        inputArr[i] = NULL;
    }

    // point first index to first command line prompt 
    inputArr[0] = strdup(firstToken);

    // if indicated, perform variable "$$" expansion of first token 
    int k;
    for (k = 0; k < strlen(inputArr[0]); k++) {
        if (inputArr[0][k] == '$' && inputArr[0][k + 1] == '$') {
            inputArr[0][k] = '\0';
            sprintf(inputArr[0], "%s%d", inputArr[0], pidNumber);
        }
    }

    // point remaining indices at remaining command line tokens
    int j;
    for (i = 1; i < listCount; i++) {
        inputArr[i] = strdup(tempCopyInput->word);
        tempCopyInput = tempCopyInput->next;

        // if indicated, perform variable "$$"" expansion of remaining tokens
        for (j = 0; j < strlen(inputArr[i]); j++) {
            if (inputArr[i][j] == '$' && inputArr[i][j + 1] == '$') {
                inputArr[i][j] = '\0';
                sprintf(inputArr[i], "%s%d", inputArr[i], pidNumber);
            }
        }
    }

    // initialize variable file names for redirection 
    char inputFileName[256] = "";
    char outputFileName[256] = "";

    // initialize boolean variable to indicate if command is a background process and/or requiring redirection
    int backgroundBool = 0;
    int redirectBool = 0;

    // check if last token is & (background process)
    if (strcmp(inputArr[listCount - 1], "&") == 0) {
        backgroundBool = 1;
    }

    // check if there is an input file
    int f;
    for (f = 0; f < listCount; f++) {
        if (strcmp(inputArr[f], "<") == 0) {
            strcpy(inputFileName, inputArr[f + 1]);
            redirectBool++;
        }
    }

    // check if there is an output file
    int g;
    for (g = 0; g < listCount; g++) {
        if (strcmp(inputArr[g], ">") == 0) {
            strcpy(outputFileName, inputArr[g + 1]);
            redirectBool++;
        }
    }

    // if comment line "#"
    if (strcmp(firstToken, "#") == 0) {
        return 1;
    }

    // if "exit"
    else if (strcmp(firstToken, "exit") == 0) {
        // kill any other processes or jobs before exiting
        // loop through structure of bg pids
        return 2;
    }

    // if "cd"
    else if (strcmp(firstToken, "cd") == 0) {
        
        // if directory is specified
        if (listCount == 2) {
            char directory[256];
            strcpy(directory, inputArr[1]);

            // change directory
            if (chdir(directory) == -1) {

                // if specified directory does not exist (result was -1)
                printf("No such directory\n");
                fflush(stdout);
            }
        }

        // if no directory was specified
        if (listCount < 2) {
            // change directory to HOME environment variable
            chdir(getenv("HOME"));
        }
        return 3;
        
    }

    // if "status"
    // code citation: Exploration: Process API - Monitoring Child Processes
    else if (strcmp(firstToken, "status") == 0) {

        // print exit status of last foreground process
        if (WIFEXITED(childStatus)) {
            printf("exit value %d\n", WEXITSTATUS(childStatus));
            fflush(stdout);
        }
        // or print the terminating signal of the last foreground process
        else {
            printf("terminated by signal %d\n", WTERMSIG(childStatus));
            fflush(stdout);
        }
        return 4;
    }

    // if non built-in command, call execvp() in new function named processExec()
    else {

        // if it is a background process
        if (backgroundBool == 1) {

            // create a duplicate input array that removes the last "&" so execvp() can be run properly
            char *dupInputArr[listCount];
            int x;
            for (x = 0; x < listCount; x++) {
                dupInputArr[x] = NULL;
                int y;
                for (y = 0; y < (listCount - 1); y++) {
                    dupInputArr[y] = inputArr[y];
                }
            }
            processExec(dupInputArr, backgroundBool, inputFileName, outputFileName, SIGINT_action);
        }

        // if command requires redirection
        else if (redirectBool != 0) {

            // create a duplicate input array that removes "<" or ">" so execvp() can be run properly
            char *dupInputArr[listCount];
            int w;
            for (w = 0; w < listCount; w++) {
                dupInputArr[w] = NULL;
                int z;
                int y = 0;
                for (z = 0; z < listCount; z++) {
                    if ((strcmp(inputArr[z], "<") == 0) || (strcmp(inputArr[z], ">") == 0)) {
                        continue;
                    }
                    else { 
                        dupInputArr[y] = inputArr[z];
                        y++;
                    }
                }
            }
            processExec(dupInputArr, backgroundBool, inputFileName, outputFileName, SIGINT_action);
        }
        
        // if NOT a background process and NOT requiring redirect, pass in the existing input array
        else {
            int p;
            for (p = 0; p < listCount; p++) {
            }
            processExec(inputArr, backgroundBool, inputFileName, outputFileName, SIGINT_action);
        }
        return 5;
    } 
}


/* Main function that provides command line prompt to user and tokenizes command line inputs into a linked-list of tokens.
This function also initializes the ^C and ^Z signal handlers.
Arguments: none
Returns: linked-list of command line inputs; pid number, signal handlers
*/
int main() {

    // initialize input commands, PID, first command
    char *command[2084];
    int pidNumber = getpid();
    char firstToken[256];
    char compString[256];

    // ^C SIGINT signal handler; parent and background processes will ignore SIGINT
    // code citation: Exploration: Signal Handling API
    struct sigaction SIGINT_action = {0};
	// The ignore_action struct as SIG_IGN as its signal handler
	SIGINT_action.sa_handler = SIG_IGN;
	// Block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGINT_action.sa_mask);
	// No flags set
	SIGINT_action.sa_flags = 0;
    // Register the SIGINT_action as the handler for SIGINT
	sigaction(SIGINT, &SIGINT_action, NULL);


    // ^Z SIGTSTP signal handler; sends a SIGTSTP signal to parent shell process and all children at the same time
    // code citation: Exploration: Signal Handling API
    struct sigaction SIGTSTP_action = {0};
	// Register handle_SIGTSTP as the signal handler
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Block all catchable signals while handle_SIGTSTP is running
	sigfillset(&SIGTSTP_action.sa_mask);
	// No flags set
	SIGTSTP_action.sa_flags = SA_RESTART;
    // Install our signal handler
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);


    // Continuous loop asking for command line input
    while(1) {

        // print ":"
        printf(": ");
        fflush(stdout);

        // read input
        fgets(command, 2084, stdin);

        // convert input into one large string by changing "\n" to "\0"
        char dupCommand[strlen(command)];
        strcpy(dupCommand, command);
        dupCommand[strlen(command) - 1] = '\0';

        // if command line is blank, re-prompt
        if (strcmp(dupCommand, "") == 0) {

            // check background PIDs before re-prompting
            checkBackgroundPIDs();
            continue;
        }

        char *saveptr;
        // tokenize string; save first token as command
        char *token = strtok_r(dupCommand, " ", &saveptr);
        strcpy(firstToken, token);
        
        // initialize linked list
        struct inputString *head = NULL;
        struct inputString *tail = NULL;

        // continue to tokenize string
        // code citation: this code segment is adapted from the sample students.c
        while (token != NULL) {
            struct inputString *currString = malloc(sizeof(struct inputString));

            char *token = strtok_r(NULL, " ", &saveptr);

            // break when there are no more tokens
            if (token == NULL){
                break;
            }

            currString->word = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currString->word, token);
            strcpy(compString, token);

            struct inputString *newNode = currString;

            // create first node in linked-list
            if (head == NULL)
            {
                // This is the first node in the linked list
                // Set the head and the tail to this node
                head = newNode;
                tail = newNode;
            }
            else
            {
                // This is not the first node.
                // Add this node to the list and advance the tail
                tail->next = newNode;
                tail = newNode;
            }
        }
        
        int inputResult;

        // send linked_list, pidNumber, and ^C signal structure for processing built-in commands or execvp()
        inputResult = processInput(firstToken, head, pidNumber, SIGINT_action); 

        // based on inputResult, re-prompt or exit program; always check background PIDs before re-prompting

        // if comment "#"
        if (inputResult == 1){
            checkBackgroundPIDs();
            continue;
        }

        // if "exit"
        if (inputResult == 2){

            // kill all background PIDs
            exit(0);

            // exit program
            return EXIT_SUCCESS;
        }

        // if "cd"
        if (inputResult == 3){
            checkBackgroundPIDs();
            continue;
        }

        // if "status"
        if (inputResult == 4) {
            checkBackgroundPIDs();
            continue;
        }

        // if execvp()
        if (inputResult == 5) {
            checkBackgroundPIDs();
            continue;
        }
    }
    return 0;
}

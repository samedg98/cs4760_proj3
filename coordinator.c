#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/msg.h>
#include <errno.h>

/*
 * 
 */

#define SHAREDFILE "./coordinator"


 pid_t *g_runningProcesses; // A queue for the running processes
 int g_maxConcurrentProcesses;
 int g_msqid; // id for message queue
 
  
struct msg_data {
   int index; // The index, or offset, in the word list (shared memory)
   int palin; // 1 if it's a palindrome, 0 if it is not a palindrome  
   pid_t pid; // ID of the process that determined the word's nature
};

struct palindrome_msgbuf {
    long mtype;
    struct msg_data msg;
};


//#define FILENAME "sharedata.txt"


// This structure is used in the queue
struct mesg {
    int offset;  // -1 means the slot is empty
    int state; // 0 means not evaluated, 1 means the word is a palindrome, 2 means it's not a palindrome
};

// Shared memory pointers
char *g_TextData; //Points to word list

int get_shared_block(char* filename,int size, int proj_id)
{
    key_t key;
    key = ftok(filename,proj_id);
    return shmget(key,size,0644 | IPC_CREAT);
}

char* attach_memory_block(char* filename, int size,int proj_id)
{
    int shared_block_id = get_shared_block(filename,size, proj_id);
    if(shared_block_id == -1)
        return NULL;
    
    char *result = shmat(shared_block_id,NULL, 0);
    if(result == (void *)-1)
        return NULL;
    return result;
}

bool destroy_memory_block(char* filename,int proj_id)
{
    int shared_block_id = get_shared_block(filename, 0, proj_id);
    if(shared_block_id == -1)
        return 0;
    return(shmctl(shared_block_id, IPC_RMID, NULL) != -1);
}

//Setting up shared memory 
void setupSharedMem(int numberOfBytes)
{
    g_TextData = (char *)attach_memory_block(SHAREDFILE,numberOfBytes,123450);    
}

void destroySharedMem()
{
    destroy_memory_block(SHAREDFILE,123450);   
}


void usage(){
    printf("\n Usage:\n\n");   
    printf("    ./coordinator [-h] [-c i] [-m j] datafile\n\n");

    printf(" -h   		Describes how the project should be run and then terminate.\n");
    printf(" -c i 		Indicate how many child processes i should be launched in total.\n");
    printf(" -m j		Indicate the maximum number of children j allowed to exist in the system at the\n");
    printf("                same time.  (Default 20)\n");
    printf(" datafile	Input file containing the palindromes and non-palindromes, one per line\n\n");
    printf(" Make sure that the 'palin' child process is in the same directory.\n");
    exit(0);
}

long getFileSize(char *filename){
    FILE *fptr;
    long sz;
    if ((fptr = fopen(filename, "r")) == NULL) {        
       fprintf(stderr," Failed to open word file %s\n", filename);        
       exit(0);
    }
    fseek(fptr, 0L, SEEK_END);
    sz = ftell(fptr);
    fclose(fptr);
    return sz;
}

// This loads the word list file into the shared memory
// returns the number of words
int loadFile(char *filename)
{
    FILE *fptr;
    char buffer[150];
    int bufferLength = 150;    
    int length;
    int wordCount=0;
    char* ptr= g_TextData;
    int i;
    
    if ((fptr = fopen(filename, "r")) == NULL) {        
       fprintf(stderr," Failed to open word file %s\n", filename);        
       exit(0);
    }
    
    while(fgets(buffer, bufferLength, fptr)) {
        length = strlen(buffer);
        //Skip empty lines
        if(length == 1){
            continue;
        }
        //printf(" length = %d\n",length);
        buffer[length-1] = ' ';
        for(i=0; i < length;i++){
            *ptr = buffer[i];
            ptr++;
            //filePosition++;
        }
        wordCount++;
    }
    ptr--;//Put a '\0' at the end of memory to indicate the end of buffer
    *ptr = '\0';
    fclose(fptr);
    return wordCount;
}

// Returns 1 if the word is a palindrome
// Returns 0 if the word is not a palindrome
int isPalindrome(char* word)
{
    int length;
    int i, j;
    length = strlen(word);
    if(length%2 == 0){ // Even lengthed words
        j = length/2;
        i = j-1; 
    }else{ //Odd lengthed words
        j = length/2;
        i = j - 1;
        j = j + 1;
    }
    while(j >= 0){
        if(tolower(word[i]) != tolower(word[j])){
            return 0;
        }
        j--;
        i++;
    }
    return 1;
}

pid_t spawnProcess(int index, int fileLength)
{ 
    char *args[4];
    char indexStr[10];
    char lengthStr[10];
    args[0] = "./palin";
    args[1] = indexStr;
    args[2] = lengthStr;
    args[3] = NULL;
    
  //  index = 2;
    //Index to pass to child process
    sprintf(indexStr,"%d",index);
    sprintf(lengthStr,"%d",fileLength);
    
    
    //Spawning a duplicate child process with a different pid
    pid_t pid = fork();    
    switch(pid){ // Error
        case -1:
            perror("fork");
            exit(1);
        case 0: // Child process follows this route              
            // This command executes the process passed as parameters in cmdLine and 
            // replaces the child process, retaining the child process PID
            execvp(args[0],args);
            perror("execvp"); // execvp doesn't return unless there is a problem           
            exit(1);
    }
   // printf("return the pid = %d\n",pid);
    return pid;
}

// Get word in shared memory pointed to by "index" (an offset in the shared mem buffer)
// Pass it back in the variable "word"
void getWord(int index,char* word)
{
    char* ptr;
    int i=0;
    
    ptr = g_TextData + index;
    while(*ptr != ' ' && *ptr != '\0'){
        word[i] = *ptr;
        ptr++;
        i++;
    }
    word[i] = '\0'; 
}

// Go through the word list and pass indices to child processes
void multiprocessing(int fileLength, int wordCount, int totalAllowedProcesses, int maxConcurrentProcesses)
{
    int loop=1;
    char* ptr;
    int i=0, j;
    //int wordCount=0;
    int processCount=0;  // Total processes so far
   // int concurrentProcesses; // concurrent processes now
    int wordIndex=0;  
    int freeSlot; // for process IDs  
    int status; 
    int count;
    struct mesg *wordStates;
    struct palindrome_msgbuf msgbuf;
    char myWord[150];
   
        
    printf(" coordinator: in multiprocessing() AAA\n");
    wordStates = (struct mesg *)malloc(wordCount*sizeof(struct mesg));
     
     i=0;
    ptr= g_TextData;
    //Acquire the indices for the words in shared memory, i.e. the offsets
    while(loop){
        if(*ptr == ' '){
            wordIndex++;
            wordStates[i].offset = wordIndex;
            wordStates[i++].state = 0;         
            ptr++;                   
        }
        else if(*ptr == '\0'){
            break;
        }else{           
            ptr++;
            wordIndex++;
        }
    }   
   
    // Move through word list and spawn a process for each word
    for(j=0; j < wordCount; j++){        
  
        //Make sure to deal with process that are no longer running
        for(i=0;i < maxConcurrentProcesses;i++){           
            if(g_runningProcesses[i] != -1){                
                if(waitpid(g_runningProcesses[i], &status, WNOHANG)!=0){  
                    printf("1) Process (pid = %d) exit status = %d.\n", g_runningProcesses[i], status);
                    fflush(stdout);
                    g_runningProcesses[i] = -1; //make the slot empty
                }
            }
        }    
      
        freeSlot = -1;
        //check for free slot
        for(i=0;i < maxConcurrentProcesses;i++){
            if(g_runningProcesses[i] == -1){
                freeSlot = i; // Acquire free slot for next process to be spawned
            }
        }
    
        // free slot not found.  Wait for one to free up
        if(freeSlot == -1){
            //Check the status of background processes
            i = 0;
            while(true){  // It's stuck here    
                if(waitpid(g_runningProcesses[i], &status, WNOHANG)!=0){               
                    printf("2) Process (pid = %d) exit status = %d.\n", g_runningProcesses[i], status);
                    fflush(stdout);
                    g_runningProcesses[i] = -1; //make the slot empty
                    freeSlot = i;
                    break;
                }
                
                i++;
                if(i == maxConcurrentProcesses){
                    i = 0;
                }            
            }
        }   

        processCount++;       

        //printf(" spawning process, index = %d!\n",wordStates[j].offset);
        // Spawn a process to check if word is a palindrome, and retain process id of spawned process
        g_runningProcesses[freeSlot] = spawnProcess(wordStates[j].offset, fileLength);
        
        //If this is the limit for number of processes, exit the loop
        if(processCount == totalAllowedProcesses){
            break;
        }
    
    }
    
    //Wait for all processes to complete    
    for(i=0;i < maxConcurrentProcesses;i++){
        if(g_runningProcesses[i] != -1){
            //while (waitpid(pid, &status, 0) == -1);
            waitpid(g_runningProcesses[i], &status, 0);
            
            g_runningProcesses[i] = -1; //make the slot empty
            
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                // handle error                        
                printf("process palin (pid = %d) failed\n", g_runningProcesses[i]);               
            }
        }
    }     
    
    printf(" coordinator: in multiprocessing() BBB\n");
    FILE *fptr1, *fptr2;
   
    
    if ((fptr1 = fopen("palin.out", "w")) == NULL) {        
       fprintf(stderr," Failed to open palin.out\n");        
       exit(0);
    }
    
    if ((fptr2 = fopen("nopalin.out", "w")) == NULL) {        
       fprintf(stderr," Failed to open nopalin.out\n");        
       exit(0);
    }
    
    
    //This is necessary if the number of words processed is less than the words in the list
    // which is caused by limiting the number of processes that can be used.
    if(processCount < wordCount){
        wordCount = processCount;
    }
    printf(" coordinator: in multiprocessing() now getting queued messages\n");
    //printf(" ====> Now getting queued messages!!\n");
    count = 0;
    //process messages
     for(;;) { 
        if (msgrcv(g_msqid, &msgbuf, sizeof(struct msg_data), 1, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        
        getWord(msgbuf.msg.index,myWord);
        printf(" coordinator: in multiprocessing() word: %s\n",myWord);
        if(msgbuf.msg.palin == 1){
            //printf(" msg Word = %s, is a palindrome\n",myWord);
            fprintf(fptr1,"%d %d %s\n",msgbuf.msg.pid,msgbuf.msg.index, myWord);
        }else{
             //printf(" msg Word = %s, is not a palindrome\n",myWord);
            fprintf(fptr2,"%d %d %s\n",msgbuf.msg.pid,msgbuf.msg.index, myWord);
        }
        count++;
        if(count == wordCount){
            break;
        }       
    }
    
    fclose(fptr2);
    free(wordStates);
}

void handle_SIGINT()
{
    int i; 
   
    printf("Kill Child Processes, exit coordinator.\n");
    for(i=0; i < g_maxConcurrentProcesses; i++){
        if(g_runningProcesses[i] != -1){
            kill(g_runningProcesses[i], SIGKILL); 
        }
    }  
    exit(0);
}

// Thread function for timer
static void *timerForMaster(void *threadData) {  
   struct timespec now1, now2;    
   int maxTime=25; //25 seconds
   int loop = 1;
 
   clock_gettime(CLOCK_REALTIME, &now1);
   printf("\n In Timer time = %d\n",maxTime);
   while(loop){
        clock_gettime(CLOCK_REALTIME, &now2);
        if((int)now2.tv_sec - (int)now1.tv_sec >= maxTime){
            loop = 0;
        }
   }
   printf("\nnow2.tv_sec = %d, now1.tv_sec = %d\n",(int)now2.tv_sec,(int)now1.tv_sec);
   printf("Timer Timed Out!\n");
   kill(getpid(), SIGINT); 
   return NULL;
}


// Set up so that you are read to send and receive messages
void setupMessageQueue()
{
    key_t key;    
    key = ftok(SHAREDFILE, 'e');
    if(key == -1){
        fprintf(stderr, " ftok: errno: %d\n", errno);
        exit(0);
    }

    g_msqid = msgget(key, 0666 | IPC_CREAT);
    if(g_msqid == -1){
        fprintf(stderr, " msget: errno: %d\n", errno);
        exit(0);
    }   
}


int main(int argc, char** argv) {
    
    int numberOfProcesses=1000;  // The number of processes to launch for this job
    int maxConcurrentProcesses=20; //The maximum number of processes allowed at once, default is 20
    int numberOfArgsRequired=2; //Default is 2
    int option;
    int fileLength;
    int wordCount;
    int i;
    pthread_t timerThread;
    struct sigaction handler;
 
    handler.sa_handler = handle_SIGINT;
    sigemptyset(&handler.sa_mask);
        
    handler.sa_flags = 0;
    sigaction(SIGINT,&handler, NULL);
    
    if(argc == 1){
        usage();
    }
    while((option = getopt(argc, argv, "hc:m:")) != -1){       
        switch(option){
            case 'h': // Get the number of threads to spawn from the command line
                usage();                               
                break;
            case 'c':
                numberOfProcesses = atoi(optarg);
                if(numberOfProcesses == 0){
                    usage();
                }
                numberOfArgsRequired += 2;
                break;
            case 'm':
                maxConcurrentProcesses = atoi(optarg);
                if(maxConcurrentProcesses == 0){
                    usage();
                }
                numberOfArgsRequired += 2;
                break;
        }                
    }
    
   // g_QueueMax = maxConcurrentProcesses;
    fileLength = (int)getFileSize(argv[argc-1]);
    setupSharedMem(fileLength);
    setupMessageQueue();
        
    //experimentWithQueue();
    
    wordCount = loadFile(argv[argc-1]);
    //printf(" wordCount = %d\n",wordCount);
    //testProcessing();
    
    //Launch the timer
    pthread_create(&timerThread, NULL, timerForMaster,NULL); //(void *)&maxTime);
   
     
    g_maxConcurrentProcesses = maxConcurrentProcesses;
    g_runningProcesses = (pid_t *)malloc(maxConcurrentProcesses*sizeof(pid_t));
    
    // Initialize process id slots to -1
    for(i=0; i < maxConcurrentProcesses;i++)
    {
        g_runningProcesses[i] = -1;
    }    

    
    multiprocessing(fileLength,wordCount,numberOfProcesses,maxConcurrentProcesses);
    
    free(g_runningProcesses);
   
    destroySharedMem();
    msgctl(g_msqid, IPC_RMID, NULL);
    return (EXIT_SUCCESS);
}


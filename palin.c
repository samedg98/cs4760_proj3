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
#include <sys/msg.h>
#include <errno.h>

/*
 * 
 */

#define SHAREDFILE "./coordinator"

//#define FILENAME "sharedata.txt"

// Shared memory pointers
char *g_TextData; //Points to word list

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

//Setting up shared memory 
void setupSharedMem(int numberOfBytes)
{   
    g_TextData = (char *)attach_memory_block(SHAREDFILE,numberOfBytes,123450);  
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



// Set up so that you are read to send and receive messages
void setupMessageQueue()
{
    key_t key;    
    key = ftok(SHAREDFILE, 'e');
    if(key == -1){
        fprintf(stderr, "palin: ftok(): errno: %d\n", errno);
        exit(0);
    }
    g_msqid = msgget(key, 0666);
    if(g_msqid == -1){
        fprintf(stderr, "palin: msget(): errno: %d\n", errno);
        exit(0);
    } 
}

/*
 * 
 */
int main(int argc, char** argv) {

    int wordListLength;  
    int index;
    char word[150];
    int i;
    int result;
    //struct msg message;
    struct palindrome_msgbuf message;
    int delay;
    char *ptr; // = g_TextData;
       
    pid_t pid = getpid();
    
    srand(time(0));
    
    //Have the process sleep for a random duration
    delay = rand()%4;
    sleep(delay);
    
    
    //The data passed should be reliable, since I'm passing it via the coordinator.c application
    index = atoi(argv[1]);
    wordListLength = atoi(argv[2]);  
    
   // printf("-->In palin, pid = %d, index = %d, wordListLength = %d\n",pid, index, wordListLength);
    //return 0;
    setupSharedMem(wordListLength);  
    setupMessageQueue();
    
    i=0;
    ptr = g_TextData + index;
    while(*ptr != ' ' && *ptr != '\0'){
        word[i] = *ptr;
        ptr++;
        i++;
    }
    word[i] = '\0';
    if(isPalindrome(word) == 1){
        message.mtype = 1;     
        message.msg.index = index;
        message.msg.palin = 1;  // word is a palindrome
        message.msg.pid = pid;
        //printf(" palin: word: %s, a palindrome!\n",word);
    }else{
        message.mtype = 1;
        message.msg.index = index;
        message.msg.palin = 0;  // word is not a palindrome        
        message.msg.pid = pid;
        //printf(" palin: word: %s, not a palindrome!\n",word);
    }

    result = msgsnd(g_msqid,&message,sizeof(struct msg_data),0); 
    
    if(result == -1)
    {
        fprintf(stderr," Palin: msgsend: errno: %d\n", errno); 
        exit(0);
    }   
    
    return (EXIT_SUCCESS);
}

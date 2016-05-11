/*
 * File:   main.c
 * Author: Neil
 *
 * Created on December 5, 2015, 2:01 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

const char * SMNAME = "/ChatRoom";
const char * PSMNAME = "/pid_str";
const int SMSIZE = 1024;
const int PSMSIZE = 1024;

volatile sig_atomic_t got_usr1 = 0;
int mfd, pfd;
char *msgaddr;
char *paddr;
char pid_str[10];


void test(char *str){
    printf("\n   ---- test - %s ----\n", str);
}
//if get signal means msg is written to shared memory

void sigusr1_handler(int sig) {
    got_usr1 = 1;
}
void help(int argc, char **argv) {
    //|| (strcmp(argv[1], "-h") == 0)
    if (argc != 1 ) {
        printf("Useage:\n");
        printf("       %s --- Run Program\n", argv[0]);
        printf("       %s -h --- This help\n", argv[0]);
        printf("       Input message and hit enter to send message, input \'exit\' to exit program");
        exit(EXIT_SUCCESS);
    }
}

// read shared memory and get the message
void* readMsg(void* arg) {
    
    while (1) {
        //pause();
        //msg is writen and get the notify signal
        if (1 == got_usr1) {
            got_usr1 = 0;
            //do read and print
            //printf("%s\n", msgaddr);
            fprintf(stdout, "%s\n", msgaddr);
        }
    }
    return NULL;
}

void errExit(char *msg) {
    perror(msg);
    exit(1);
}

void setupSignal() {
    struct sigaction sa;
    got_usr1 = 0;
    sa.sa_handler = sigusr1_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        errExit("sigaction");
    }
}


void initProcessSM() {
    
    int flags;
    
    flags = O_CREAT | O_RDWR;
    
    pfd = shm_open(PSMNAME, flags, 0666);
    if (pfd == -1)
        errExit("shm_open");
    
    ftruncate(pfd, PSMSIZE);//must have this
    paddr = mmap(0, PSMSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pfd, 0);
    if (paddr == MAP_FAILED)
        errExit("mmap");
    
    //put self pid into SM
    char ptr[10];
    sprintf(ptr, "%ld&",(long)getpid());
    strcat(paddr, ptr);
}

void initMsgSM() {
    
    int flags;
    flags = O_CREAT | O_RDWR;
    
    //if shared memory does not exist create shared memory
    mfd = shm_open(SMNAME, flags, 0666);
    
    if (mfd == -1)
        errExit("shm_open");
    ftruncate(mfd, SMSIZE);
    msgaddr = mmap(0, SMSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    if (msgaddr == MAP_FAILED)
        errExit("mmap");
}

void removeSubstring(char *s,const char *toremove)
{
    int len = strlen(toremove);
    while( (s=strstr(s,toremove)) )
        memmove(s,s+len,1+strlen(s+len));
}

void exitcomm(){
    
    //with condition
    //delete current pid
    char ptr[10];
    sprintf(ptr, "%ld&",(long)getpid());
    removeSubstring(paddr,ptr);
    
    //assume as last process stand, the unlink the shared m
    if (strlen(paddr) < 3)
    {
        paddr = "";
        if (shm_unlink(PSMNAME) == -1)
            errExit("shm_unlink");
        if (shm_unlink(SMNAME) == -1)
            errExit("shm_unlink");
    }
    
    close(mfd);
    close(pfd);
    exit(EXIT_SUCCESS);
}

// get all the pid of chatter and send signal

void sendSignal() {
    
    char *pch;
    char *deli = "&";
    
    char * my_copy;
    my_copy = malloc(sizeof(char) * strlen(paddr));
    strcpy(my_copy,paddr);
    
    /* get the first token */
    pch = strtok(my_copy, deli);
    /* walk through other tokens */
    while (pch != NULL) {
        if (strcmp(pch, pid_str) != 0 )
        {
            kill(atoi(pch),SIGUSR1);
        }
        pch = strtok(NULL, deli);
    }
}

int main(int argc, char **argv) {
    
    help(argc, argv);
    
    sprintf(pid_str, "%ld",(long)getpid());
    
    setupSignal();
    initProcessSM();
    initMsgSM();
    
    pthread_t rd_t;
    size_t len; /* Size of shared memory object */
    
    //read thread: task is read msg from shared memory
    if (0 != pthread_create(&rd_t, NULL, readMsg, NULL)) {
        errExit("pthread_create");
    }
    
    //in main thread wait for user's input
    while (1) {
        printf("please input:");
        char input[255];
        scanf("%s", input);
        if (strcmp("exit", input) == 0) {
            exitcomm();
        } else {
            //write to shared memory
            char str[SMSIZE];
            sprintf(str, "%s :", pid_str);
            strcat(str, input);
            len = strlen(str);
            
            memcpy(msgaddr, str, len);
            msgaddr[len] = '\0';
            sendSignal();
        }
    }
    return 0;
}
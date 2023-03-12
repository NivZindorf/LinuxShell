#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define EXECUTE 1
#define BACKGROUND 2
#define PIPE 3
#define REDIRECT 4

int get_type(int count, char** arglist);
int execute(char ** arglist);
int background(int count,char** arglist);
int pipeSplit(int count,char ** arglist);
char * redirect(int count,char ** arglist);
int pipeExecute(int count,char ** arglist);


int prepare(void){
    //ERAN'S TRICK
    signal(SIGCHLD, SIG_IGN); //preventing zombies
    signal(SIGINT,SIG_IGN); /*got the idea from eran's trick,but instead I use it for SIGINT
 * I will change it back to SIG_DFL before executing a foreground process so they WILL terminate */
    return 0;
}
int finalize(void){
    return 0;
}
int process_arglist(int count, char** arglist){
    switch (get_type(count,arglist)) {
        case EXECUTE:
            execute(arglist);
            break;
        case BACKGROUND:
            background(count,arglist);
            break;
        case REDIRECT:
            redirect(count,arglist);
            break;
        case PIPE:
            pipeExecute(count,arglist);
            break;
        default:
            //probably error occurred
            return 0;
    }
    return 1;
}

int execute(char ** arglist){
    int pid = fork();
    if (pid == 0){
        //executing the program on the child process
        signal(SIGINT,SIG_DFL); // make the SIGINT work again for foreground process
        if (execvp(arglist[0],arglist) == -1){
            perror("Error");
            exit(1);
        }
    }
    else{
        //waiting for the child to finish before getting new command
        int status;
        waitpid(pid,&status,0); //check about this option field!!

    }
    return 1;
}


int background(int count,char ** arglist){
    arglist[count-1]= NULL; //deleting the '&' word from command
    int pid= fork();
    if (pid == 0 ){
        //keep ignoring the SIGINT signal
        if(execvp(arglist[0],arglist)==-1){
            perror("Error");
            exit(1);
        }
    }
    else{
        //the father continue as usual while new program running in background
        return 1;
    }
    return 1;
}
int get_type(int count, char** arglist){
    //return the "type" of the program to execute
    for (int i=0;i<count;i++) {
        switch (arglist[i][0]) {
            case '|':
                return PIPE;
            case '>':
                return REDIRECT;
            case '&':
                return BACKGROUND;
            default:
                break;
        }
    }

    return EXECUTE;
}

int pipeSplit(int count,char ** arglist){
    /* used for pipe commands,split the arglist to two commands,
     * by replacing the '|' with NULL
     * and return the index where the second command begin
     *  or -1 for error */
    for(int i=0;i<count;i++){
        if (arglist[i][0] == '|'){
            arglist[i] = NULL;
            return i+1;
        }
    }
    return -1;
}
int pipeExecute(int count,char ** arglist){
    /*this function uses parts from Bash session 4  "papa_son_pipe.c"
     * and from https://stackoverflow.com/questions/33884291/pipes-dup2-and-exe
     * with modifications for my code
     */
    int index = pipeSplit(count,arglist);
    int pipefd[2];
    if (-1 == pipe(pipefd)) {
        perror("pipe error");
        return 0;
    }
    int pidsend = fork();
    if (pidsend ==0){
        //sender goes here
        dup2(pipefd[1],1);//redirecting stdout to sender end of the pipe
        signal(SIGINT,SIG_DFL); // make the SIGINT work again for foreground process
        close(pipefd[0]);
        close(pipefd[1]);
        if (execvp(arglist[0],arglist)==-1){
            perror("Error");
            exit(1);
        }
    }
    else{
        //parent
        int pidrecive = fork();
        if (pidrecive == 0){
            //receiver goes here
            dup2(pipefd[0],0);// redirecting receive end of the pipe to stdin
            signal(SIGINT,SIG_DFL); // make the SIGINT work again for foreground process
            close(pipefd[0]);
            close(pipefd[1]);
            if(execvp(arglist[index],arglist+index)==-1){
                perror("Error");
                exit(1);
            }
        }
        else{
            int statusend,statusrecive;
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pidsend,&statusend,0);
            waitpid(pidrecive,&statusrecive,0);

            return 1;
        }

    }
return 1;
}
char * redirect(int count,char ** arglist) {
    /* used for redirect commands,
     * replace the '>' with Null  for execution
     * and keep the filename to use
     */

    arglist[count - 2] = NULL;
    char * filename= arglist[count - 1];
    int pid = fork();
    if (pid ==0 ){
        char filedes = open(filename,O_CREAT|O_TRUNC|O_WRONLY,S_IRWXU);
        dup2(filedes,1);
        signal(SIGINT,SIG_DFL); // make the SIGINT work again for foreground process
        if(execvp(arglist[0],arglist)==-1){
            perror("Error");
            exit(1);
        }
    }
    else{
        int status;
        waitpid(pid,&status,0);
        return 0;
    }
    return 0;

}
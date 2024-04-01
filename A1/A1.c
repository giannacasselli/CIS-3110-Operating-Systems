#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>


//create global arrays for pipes and child pids
int **file_pipe;
int **data_pipe;
pid_t *ids;
//global variable for tracking pids and number of handled children
//handled_child is variable used to terminate parent process
int id_num=0;
int handled_child=0;
//signal handler
void handler(int sig){
    int status;
    while((ids[id_num]=(waitpid(-1,&status,WNOHANG)))>0){
        //if child exited normally
        if(WIFEXITED(status)){
            printf("I caught a SIGCHLD! Child PID is %d\n", ids[id_num]);
            //initialize histogram array to read in to from data pipe
            int hist[26];
            read(data_pipe[id_num][0],hist,sizeof(hist));
            close(data_pipe[id_num][0]);
            //create char array for file name
            char outFile[100];
            //write pid to file name
            snprintf(outFile,sizeof(outFile),"file%d.hist",ids[id_num]);
            //create file ptr for .hist file
            FILE *histo=fopen(outFile,"w");
            //error handling
            if(histo==NULL){
                printf("Error writing to file\n");
                exit(EXIT_FAILURE);
            }
            //write histogram to the file
            for(int b=0;b<26;b++){
                fprintf(histo,"%c %d\n",b+97,hist[b]);
            }
            fclose(histo);
        }
        //if child was terminated by signal
        else if(WIFSIGNALED(status)){
            close(data_pipe[id_num][0]);
            int code=WTERMSIG(status);
            printf("Child %d terminated by signal %d: %s\n", ids[id_num],code, strsignal(code));
        }
        //increment id_num to keep track of pids
        id_num++;
    }
    //increment number of handled children
    handled_child++;
   
}
int histogram(int read_file, int write_data){
    char filename[100];
    //array for storing character counts
    int char_count[26];
    int j;
    //initialize to 0
    for(j=0;j<25;j++){
        char_count[j]=0;
    }
    //read file name from file pipe
    read(read_file,filename,sizeof(filename));
    FILE *file;
    //read text from the file
    file=fopen(filename,"r");
    if(file==NULL){
        return 1;
    }
    char c=fgetc(file);
    while(c!=EOF){
        //count characters
        for(j=0;j<26;j++){
            if((c==j+65)||(c==j+97)){
                char_count[j]++;
                break;
            }
        }
        //printf("%d\n",getpid());
        c=fgetc(file);
    }
    //write to data pipe and close
    write(write_data,char_count,26*sizeof(int));
    fclose(file);
    return 0;
}
int main(int argc, char *argv[])
{
    //register SIGCHLD handler
    struct sigaction sa;
    //set to 0 to avoid pointing to uninitialized bytes
    memset(&sa,0,sizeof(sa));
    sa.sa_handler=handler;
    sigaction(SIGCHLD,&sa,NULL);
    //check number of arguments
    if(argc<2){
        printf("There are no files!\n");
        exit(EXIT_FAILURE);
    }
    //allocate memory for arrays
    file_pipe=(int **)malloc((argc-1)*sizeof(int *));
    data_pipe=(int **)malloc((argc-1)*sizeof(int *));
    ids=(pid_t *)malloc((argc-1)*sizeof(pid_t));
    int i;
    for(i=0;i<argc-1;i++){
        file_pipe[i]=(int *)malloc(2 * sizeof(int));
        data_pipe[i]=(int *)malloc(2 * sizeof(int));
    }
    //loop through argc
    for(i=0;i<argc-1;i++){
        pipe(data_pipe[i]);
        //error handling
        if(pipe(file_pipe[i])==-1){
            printf("Pipes not working!\n");
            exit(EXIT_FAILURE);
        }
        //fork a new child for every argument
        ids[i]=fork();
        if(ids[i]==-1){
            printf("child process error\n");
            exit(EXIT_FAILURE);
        }
        else if(ids[i]==0){
            printf("File '%s' is child process %d\n",argv[i+1],getpid());
            //child process
            //close write end of file pipe
            //close read end of data pipe
            close(file_pipe[i][1]);
            close(data_pipe[i][0]);
            //if argument is SIG
            if(strcmp(argv[i+1],"SIG")==0){
                close(data_pipe[i][1]);
                sleep(10+3*i);
                kill(getpid(),SIGINT);
            }
            //if the file cannot open in histogram function
            if(histogram(file_pipe[i][0],data_pipe[i][1])==1){
                //close(data_pipe[i][1]);
                printf("File Error: Could not open file for child process %d. Child Process will be terminated.\n",getpid());
                sleep(10+3*i);
                kill(getpid(),SIGKILL);
            }
            //sleep
            sleep(10+3*i);
            //close write end of data pipe
            close(data_pipe[i][1]);
            exit(EXIT_SUCCESS);


        }
        else{
            //parent process
            //pass file name to child
            //close read end of file pipe
            //close write end of data pipe
            close(file_pipe[i][0]);
            close(data_pipe[i][1]);
            write(file_pipe[i][1], argv[i+1], strlen(argv[i+1])+1);
            //close write end of file pipe
            close(file_pipe[i][1]);
           
        }
    }
    while(1){
        //if all children have terminated
        if(handled_child==argc-1){
            break;
        }
    }
    //free memory
    for(int k=0;k<2;k++){
        free(file_pipe[i]);
        free(data_pipe[i]);
    }
    free(ids);
    return 0;
}


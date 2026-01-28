#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define READ 0
#define WRITE 1
#define LIMIT 35

void prime(int infd){
    int p;
    if (read(infd,&p,sizeof(int))!=sizeof(int)){
        close(infd);
        exit(0);
    }
    printf("prime %d\n",p);
    int out[2];
    pipe(out);
    int pipe=fork();
    if (pipe==0){// child
        close(out[WRITE]);
        close(infd);
        prime(out[READ]);
        exit(0);
    }else{
        close(out[READ]);
        int x;
        while (read(infd,&x,sizeof(int)) == sizeof(int)){
            if (x % p!=0){
                write(out[WRITE],&x,sizeof(int));
            }
        }
        close(infd);
        close(out[WRITE]);
        wait(0);
        exit(0);
    }

}

int main(int argc, char *argv[]) {
    int fd[2];
    pipe(fd);
    

    int pid=fork();
    if (pid>0){ // parent
        close(fd[READ]);
        
        for (int i=2;i<=LIMIT;i++){
            write(fd[WRITE],&i,sizeof(int));
        }
        close(fd[WRITE]);
        wait(0);
        exit(0);

    }
    else if (pid==0){//child
        close(fd[WRITE]);
        prime(fd[READ]);
        exit(0);
    }else{
        fprintf(2, "fork failed\n");
        exit(1);
    }

}
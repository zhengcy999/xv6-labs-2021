#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fs.h"

#define STDIN 0
#define STDERR 2
#define MAXLINE 1024

// 运行一次命令：base_argv(固定部分) + line(这一行的额外参数)
static void
run_once(char *base_argv[], int base_count, char *line)
{
    char *exec_argv[MAXARG];
    int exec_count = 0;

    for (int i = 0; i < base_count; i++) {
    exec_argv[exec_count++] = base_argv[i];
    }

    int j = 0;
    while (line[j] != 0) {
        while (line[j] == ' ')
            j++;

    // 跳过空格后如果到行尾，结束外层循环
    if (line[j] == 0)
        break;

    if (exec_count >= MAXARG - 1) {
        fprintf(2, "xargs: too many args\n");
        exit(1);
    }

    exec_argv[exec_count++] = &line[j];

    while (line[j] != 0 && line[j] != ' ')
        j++;

    // 如果遇到空格，把它变成 '\0'，完成切分
    if (line[j] == ' ') {
        line[j] = 0;
        j++;
    }
    }
    
    //结尾补 0
    exec_argv[exec_count] = 0;
    int pid=fork();
    if (pid < 0) {
        fprintf(STDERR, "xargs: fork failed\n");
        exit(1);
    }
    if (pid==0){
        exec(exec_argv[0],exec_argv);
        fprintf(STDERR, "xargs: exec failed\n");
        exit(1);
    }
    wait(0);
    return;
}

int main (int argc,char *argv[]){
    if (argc<2){
        fprintf(STDERR,"usage: xargs command [args...]\n");
        exit(1);
    }
    // 保留固定参数 argv[1...argc-1] 到 base_argv
    char *base_argv[MAXARG];
    int base_count=0;
    for (int i=1;i<argc;i++){
        base_argv[base_count++]=argv[i];
    }
    char line[MAXLINE];
    int idx=0;
    char c;
    int n;
    while ((n=read(STDIN,&c,1))>0){
        if (c=='\n'){
            line[idx] = 0;
            if (idx>0){
                run_once(base_argv, base_count, line);
            }
            idx=0;
            
        }else{
            if (idx>=MAXLINE-1){
                fprintf(STDERR, "xargs: line too long\n");
                exit(1);
            }
            line[idx++]=c;
        }
    }
    if (idx > 0) { 
        line[idx]=0;
        run_once(base_argv, base_count, line);
    }
    if (n<0){
        fprintf(STDERR, "xargs: read error\n");
        exit(1);
    }
    exit(0);

}
#include "kernel/types.h"
#include "user.h"

int main(int argc, char const *argv[])
{
	int p1[2], p2[2];
    pipe(p1);
    pipe(p2);
    char buf1[4] = "ping";
    char buf2[4] = "pong";
    char buf[4];
    int pid_child,pid_parent;
    if(fork() == 0){
        /*子进程*/
        pid_child = getpid();   
        close(p1[1]);    //关闭p1写端          
        read(p1[0],buf,4);   //读取管道p1内信息
        printf("%d: received %s\n",pid_child,buf);
        close(p1[0]);    //读取完毕，关闭p1读端
        
        close(p2[0]);    //关闭p2读端
        write(p2[1],buf2,4);
        close(p2[1]);    //写入完毕，关闭p2写端
        exit(0);
    }
    else{
        /*父进程*/
        pid_parent = getpid();
        close(p1[0]);    //关闭p1读端
        write(p1[1],buf1,4);
        close(p1[1]);    //写入完毕，关闭p1写端

        close(p2[1]);   //关闭p1写端
        read(p2[0],buf,4);   //读取管道p2内信息
        printf("%d: received %s\n",pid_parent,buf);
        close(p2[0]);    //读入完毕，关闭p2读端
    }
    exit(0);
}
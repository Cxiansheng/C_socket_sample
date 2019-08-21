#include<sys/file.h>
#include<stdio.h>
#include<string.h>
#define INFO_PATH "/usr/share/os-code/exec-server/client_info.txt"


void main(){
    FILE *fp;
    FILE * buff=NULL;
    char out[1024];
    if((fp = fopen(INFO_PATH, "r")) == NULL){
        printf("Open client_info file error!\n");
    }
    if(flock(fp->_fileno, LOCK_EX) != 0){
        printf("Client_info file locked!\n");
    }
    char cmd[1024];
    sprintf(cmd,"%s","cat ");
    strcat(cmd,INFO_PATH);
    if((buff = popen(cmd,"r")) == NULL){
            printf("error!\n");
            return ;
    }
    while(fgets(out, sizeof(out), buff)!=NULL){
        printf("%s",out);
    }
}

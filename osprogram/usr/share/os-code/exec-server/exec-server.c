#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <pthread.h>

#define DOMAIN                  1
#define TCP                     2
#define MAX_LEN                 1024
#define MAX_MSG                 1024*5
#define TOO_MANY_CLIENT_ERROR   "-1"
#define KEEP_TIME_OUT_ERROR     "-2"
#define INFO_PATH "/usr/share/os-code/exec-server/client_info.txt"
#define CONF_PATH "/usr/share/os-code/exec-server/exec-server.conf"
#define UNIX_FILE "/usr/share/os-code/exec-server/domain-tmp"

int MAX_CLIENT;
int MAX_ACK_TIME;
int RETRY;
int MAX_KEEP_TIME;

/**
 * client_type 记录fd对应的socket 类型
 * 1 代表是domain类型，2 代表是Tcp类型
 */
int client_type[MAX_LEN+5];
int client_fd[MAX_LEN+5];
int client_time[MAX_LEN+5];
int client_cmd_succ[MAX_LEN+5];
int client_cmd_fail[MAX_LEN+5];
int connect_num = 0;
pthread_mutex_t mutex;
pthread_t socketthread;

void write_info(){
    FILE *fp;
    if ((fp = fopen(INFO_PATH, "w")) == NULL) {
        printf("Open client_info file error!\n");
    }
    if (flock(fp->_fileno, LOCK_EX) != 0) {
        printf("Client_info file locked!\n");
    }
    fputs("Connected clinet num:",fp);

    char num[5];

    sprintf(num, "%d\n", connect_num);
    fputs(num,fp);

    struct sockaddr_in sock_in;
    int len = sizeof(sock_in);
    int i;
    int flag = 0; // 是否有客户端信息可写

    for (i=0;i<MAX_CLIENT;++i) {
        if (client_fd[i]) {
            flag = 1;
            sprintf(num,"%d",i+1);
            fputs("\nClient ID:",fp);
            fputs(num, fp);

            fputs("\nIP ADDRESS:",fp);
            if (client_type[i] == TCP) {
                getpeername(client_fd[i], (struct sockaddr *)&sock_in, &len);
                fputs(inet_ntoa(sock_in.sin_addr),fp);
            }
            else {
                fputs("Local IP",fp);
            }

            fputs("\nALL CMD num:",fp);
            sprintf(num, "%d", client_cmd_succ[i]+client_cmd_fail[i]);
            fputs(num, fp);

            fputs("\nSuccess CMD num:",fp);
            sprintf(num, "%d", client_cmd_succ[i]);
            fputs(num, fp);

            fputs("\nFailed CMD num:",fp);
            sprintf(num, "%d", client_cmd_fail[i]);
            fputs(num, fp);
            fputs("\n",fp);
        }
    }
    if(!flag){

    }

    flock(fp->_fileno,LOCK_UN);
    fclose(fp);
}

void read_conf(){
    FILE *fp;
    if ((fp = fopen(CONF_PATH, "r")) == NULL) {
        printf("Open config file error !\n");
        return;
    }

    int i;
    char buff[1024]; 

    for (i = 0; i < 3; ++i) {
        fgets(buff, 1024, fp);
    }
    
    fgets(buff, 1024, fp);
    i = 0;
    while (buff[i]!='[') {
        ++i;
    }
    MAX_CLIENT = 0;
    while (buff[++i]!=']') {
        MAX_CLIENT = MAX_CLIENT*10 + buff[i] - '0';
    }

    fgets(buff, 1024, fp);
    fgets(buff, 1024, fp);
    i = 0;
    while (buff[i]!='[') {
        ++i;
    }
    MAX_ACK_TIME = 0;
    while (buff[++i]!=']') {
        MAX_ACK_TIME = MAX_ACK_TIME*10 + buff[i] - '0';
    }

    fgets(buff, 1024, fp);
    fgets(buff, 1024, fp);
    i = 0;
    while (buff[i]!='[') {
        ++i;
    }
    RETRY = 0;
    while (buff[++i]!=']') {
        RETRY = RETRY*10 + buff[i] - '0';
    }

    fgets(buff, 1024, fp);
    fgets(buff, 1024, fp);
    i = 0;
    while (buff[i]!='[') {
        ++i;
    }
    MAX_KEEP_TIME = 0;
    while (buff[++i]!=']') {
        MAX_KEEP_TIME = MAX_KEEP_TIME*10 + buff[i] - '0';
    }    
    
}

void time_alive(){
    int i;
    while (1) {
        sleep(1);
        for (i=0;i<MAX_CLIENT;++i) {
            if (client_fd[i] != 0 && client_time[i] != 0) {
                pthread_mutex_lock(&mutex);
                //printf("client %d time left:%d\n",i+1, client_time[i]);
                client_time[i]--;
                if (client_time[i] == 0) {
                    send(client_fd[i], KEEP_TIME_OUT_ERROR,3, 0);
                    printf("Client %d timeout!\n",i+1);
                    client_cmd_fail[i] = 0;
                    client_cmd_succ[i] = 0;
                    client_fd[i] = 0;
                    connect_num--;
                    client_type[i] = 0;
                    client_time[i] = 0;
                    //close(client_fd[i]);
                    //超时后 无需关闭客户端fd,由客户端接收超时消息后，自行关闭。
                }
                pthread_mutex_unlock(&mutex);
                write_info();
            }

        }
    } /* End of while */
    
}

void get_out(char *cmd,int fd, int index){
    
	FILE * buff=NULL;
	char out[MAX_LEN];
    char msg[MAX_MSG];
	memset(out,'\0',sizeof(out));
    memset(msg,'\0',sizeof(msg));
    char *tmp_cmd = strtok(cmd, "&");
    while (tmp_cmd != NULL) {
        strcat(msg,"\n[*] ");
        strcat(msg,tmp_cmd);
        strcat(msg,":\n");
        if ((buff = popen(tmp_cmd,"r")) == NULL) {
            printf("Popen error!\n");
            return ;
        }

        int flag = 0; //命令是否正确

        while (fgets(out, sizeof(out), buff)!=NULL) {
            flag = 1;
            if ((MAX_MSG - strlen(msg)) < (strlen(out) + 1)) {
                char err[] ="Error return data is too large!\n";
                send(fd, "Error return data is too large!\n", sizeof(err), 0);
                goto end;
            }
            strcat(msg,out);
        }
        if (!flag) {
            client_cmd_fail[index]++;
            strncat(msg,tmp_cmd,strlen(tmp_cmd));
            strcat(msg,": not found or no result!\n");
        }
        else {
            client_cmd_succ[index]++;
        }

        tmp_cmd = strtok(NULL, "&");
    } /* End of while */
    send(fd, msg, sizeof(msg), 0);
end:
    write_info();
	pclose(buff);
}

void handle_connect(int sock_fd, int type){
    int conn_fd;

    if (listen(sock_fd, MAX_CLIENT) < 0) {
        printf("Listen socket failed!\n");
        close(sock_fd);
        return;
    }
    else {
        printf("Listen socket success!\n");
    }

    int i;
    fd_set  fds;
    
    memset(client_fd,0,sizeof(client_fd));
    memset(client_type,0,sizeof(client_fd));
    memset(client_time, 0, sizeof(client_time));

    while (1) {
        FD_ZERO(&fds);
        FD_SET(sock_fd, &fds);

        /*domain 监听domain的  TCP监听TCP的 */
        for (i = 0; i < MAX_CLIENT; ++i) {
            if (client_fd[i] != 0 && client_type[i] == type) {
                FD_SET(client_fd[i], &fds);
            }
        }

        if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) < 0) {
            printf("Select Error!\n");
            break;
        }

        if (FD_ISSET(sock_fd, &fds)) {
            if ((conn_fd = accept(sock_fd, NULL,NULL)) < 0) {
                printf("Accpet error!\n");
                continue;
            }

            if (connect_num < MAX_CLIENT) {
                for (i = 0; i < MAX_CLIENT; ++i){
                    if(client_fd[i] == 0){
                        pthread_mutex_lock(&mutex);
                        client_fd[i] = conn_fd;
                        connect_num++;
                        client_type[i] = type;
                        client_time[i] = MAX_KEEP_TIME;
                        client_cmd_fail[i] = 0;
                        client_cmd_succ[i] = 0;
                        pthread_mutex_unlock(&mutex);
                        write_info();
                        break;
                    }
                }
                  
                char id[5];

                sprintf(id, "%d", i+1);
                send(conn_fd,id,sizeof(id),0);
                printf("Client ID:%d has connected!\n",i+1);
            }
            else {
                send(conn_fd,TOO_MANY_CLIENT_ERROR,3,0);
                close(conn_fd);
            }
        }

        char buff[MAX_LEN];
        int ret;

        for (i = 0; i < MAX_CLIENT; ++i) {
            if (client_fd[i] && FD_ISSET(client_fd[i], &fds)) {
                if ((ret = recv(client_fd[i], buff, MAX_LEN, 0)) <= 0) {
                    printf("Client %d disconnected(other)!\n",i+1);
                    close(client_fd[i]);
                    FD_CLR(client_fd[i], &fds);
                    pthread_mutex_lock(&mutex);
                    client_fd[i] = 0;
                    connect_num--;
                    client_type[i] = 0;
                    client_time[i] = 0;
                    client_cmd_fail[i] = 0;
                    client_cmd_succ[i] = 0;
                    pthread_mutex_unlock(&mutex);	                    
                }
                else {
                    client_time[i] = MAX_KEEP_TIME;
                    get_out(buff, client_fd[i], i);
                }
            }
        }
    } /* End of while */

    for (i = 0 ;i < MAX_CLIENT; ++i){
        if(client_fd[i] && client_type[i] == type){
            close(client_fd[i]);
        }
    }
    close(sock_fd);
}

void domain_socket(){
    int sock_fd;
    struct sockaddr_un server_un, client_un;
    const char * filename  = UNIX_FILE;
    unlink(filename);
    server_un.sun_family = AF_UNIX;
    strcpy(server_un.sun_path, filename);

    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        printf("Create domain socket error!\n");
        return;
    }
    else {
        printf("Create socket success!\n");
    }

    if (bind(sock_fd, (struct sockaddr *)&server_un, sizeof(server_un)) < 0) {
        printf("Bind socket error!\n"); 
        close(sock_fd);
    }
    else {
        printf("Bind socket success!\n");
    }

    handle_connect(sock_fd,DOMAIN);

    
}

void tcp_socket(){
    int sock_fd, conn_fd;
    struct sockaddr_in server_in, client_in;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Create tcp socket error!\n");
        return;
    }
    else {
        printf("Create socket success!\n");
    }

    memset(&server_in, 0 ,sizeof(server_in));
    server_in.sin_family = AF_INET;
    server_in.sin_addr.s_addr = htonl(INADDR_ANY);
    server_in.sin_port = htons(6666);

    if (bind(sock_fd, (struct sockaddr *)&server_in, sizeof(server_in)) < 0) {
        printf("Bind socket error!\n"); 
        close(sock_fd);
    }
    else{
        printf("Bind socket success!\n");
    }

    handle_connect(sock_fd, TCP);
   
}

void main(){
    read_conf();
    write_info();

    if (pthread_mutex_init(&mutex, NULL) != 0) {
		printf("Mutex init faied\n");  
        return;
	}
    
	pthread_create(&socketthread, NULL, (void *)&domain_socket, NULL);
    pthread_create(&socketthread, NULL, (void *)&tcp_socket, NULL);
    pthread_create(&socketthread, NULL, (void *)&time_alive, NULL);
    pthread_exit(NULL);
    pthread_mutex_destroy(&mutex);
}

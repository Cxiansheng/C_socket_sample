#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAX_LEN_CMD             1024
#define MAX_MSG                 1024*5
#define TOO_MANY_CLIENT_ERROR   -1
#define KEEP_TIME_OUT_ERROR     "-2"
#define CONF_PATH "/usr/share/os-code/exec-server/exec-server.conf"
#define UNIX_FILE "/usr/share/os-code/exec-server/domain-tmp"

int RETRY;
int MAX_ACK_TIME;
char msg_buffer[MAX_MSG];

void read_conf(){
    FILE *fp;
    if ((fp = fopen(CONF_PATH, "r")) == NULL) {
        printf("Open config file error !\n");
        return;
    }

    int i;
    char buff[1024]; 

    for (i = 0; i < 5; ++i) {
        fgets(buff, 1024, fp);
    }

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
}

void handle_socket(int sock_fd, char *host){
    char clientID[5];
    char shell_cmd[MAX_LEN_CMD];

    if (recv(sock_fd, clientID, sizeof(clientID), 0) < 0) {
        printf("Receive ClientID failed!\n");
        close(sock_fd);
        return ;
    }

    int client_id =atoi(clientID);

    if (client_id == TOO_MANY_CLIENT_ERROR) {
        printf("Too many client ,please wait for a while...\n");
        close(sock_fd);
        return ;
    }
    printf("Your Client ID is:%d\n",client_id);

    while (1) {  
        printf("CLIENT> ");
        fgets(shell_cmd, MAX_LEN_CMD, stdin);
        strtok(shell_cmd, "\n");

        if (strcmp(shell_cmd, "quit") == 0) {
            if(sock_fd)
            printf("Disconneted from %s server\n", host);
            break;
        }
        
        int len = strlen(shell_cmd);
        char next_cmd[MAX_LEN_CMD];
        /**
         * 未处理命令字符串的尾空格
         */
        while (shell_cmd[len-1] == '&') {
            printf("CLIENT> ");
            fgets(next_cmd, MAX_LEN_CMD, stdin);
            strtok(next_cmd, "\n");
            if (strcmp(next_cmd, "wait") == 0) {
                break;
            }
            strcat(shell_cmd,next_cmd);
            len = strlen(shell_cmd);
        }

        if (send(sock_fd, shell_cmd, MAX_LEN_CMD, 0) < 0) {
            printf("Send msg to server failed !\n");
            break;
        }
        usleep(100000); // sleep 0.1 秒

        fd_set  fds;
        struct timeval timeout; 
        int retry = RETRY; 
        int flag = 0; // 是否正确收到ack回复

        while (retry--) {
            FD_ZERO(&fds);
            FD_SET(sock_fd, &fds);
            timeout.tv_sec = MAX_ACK_TIME;
            timeout.tv_usec = 0;
            // printf("begin : %ld\n",timeout.tv_sec);
            int re;

            if ((re = select(sock_fd+1, &fds, NULL, NULL, &timeout)) > 0) {
                if (FD_ISSET(sock_fd, &fds)) {
                    flag = 1;
                    memset(msg_buffer, '\0', sizeof(msg_buffer));
                    int ret;
                    if ((ret = recv(sock_fd, msg_buffer, sizeof(msg_buffer), 0)) <= 0) {
                        printf("%s server disconnected!\n", host);
                        goto end;
                    }
                    else {
                        if (strcmp(msg_buffer, KEEP_TIME_OUT_ERROR) == 0) {
                            printf("Connected with %s server error!(timeout)\n",host);
                            goto end;
                        }
                        printf("\n");
                        printf("%s\n",msg_buffer);
                        fflush(stdout);
                        break;
                    }
                }
            }
            else if(re == 0){
                send(sock_fd, shell_cmd, MAX_LEN_CMD, 0);
            }
        } /* End of while */
        if (!flag) {
            printf("command '%s' excute failed !\n",shell_cmd);
        }

    } /* End of while */
end:
    close(sock_fd);
    return ;
}

void domain_socket()
{
    struct sockaddr_un client_un;
    int sock_fd;
    const char *filename = UNIX_FILE;
    strcpy(client_un.sun_path, filename);
    client_un.sun_family = AF_UNIX;

    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        printf("Create domain socket error!\n");
        return ;
    }
    else {
        printf("Create domain socket success\n");
    }

    if (connect(sock_fd, (struct sockaddr*)&client_un, sizeof(client_un)) < 0) {
        printf("Connet with local server failed!\n");
        close(sock_fd);
        return ;
    }
    else {
        printf("Connet with local server success\n");
    }

    handle_socket(sock_fd, "local");
    
}

void tcp_socket(char *IPaddress){
    struct sockaddr_in sock_in;
    int sock_fd;
    char clientID[5];
    char shell_cmd[MAX_LEN_CMD];

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Create TCP socket error!\n");
        return ;
    }
    else {
        printf("Create TCP socket success\n");
    }

    memset(&sock_in, 0, sizeof(sock_in));
    sock_in.sin_family = AF_INET;
    sock_in.sin_port = htons(6666);
    
    if (inet_pton(AF_INET, IPaddress, &sock_in.sin_addr) <0) {
        printf("inet_pton error!!!!");
        return ;
    }

    if (connect(sock_fd, (struct sockaddr*)&sock_in, sizeof(sock_in)) < 0) {
        printf("Connet with remote server failed!\n");
        close(sock_fd);
        return ;
    }
    else {
        printf("Connet with remote server success\n");
    }

    handle_socket(sock_fd, "remote");

}

void main(int argc, char*argv[]){

    read_conf();

    char *model;
    if (argc < 2) {
        printf("please input parameters:-l or -r -h HostIP\n");
        return ;
    }

    model = argv[1];

    if (strcmp(model, "-l") == 0) {
        printf("reday to connet with local server.\n");
        domain_socket();
    }
    else if (strcmp(model, "-r") == 0) {
        if (argc < 4) {
            printf("use '-r -h HostIP' to connect with remote server\n");
            return ;
        }

        char *IPaddress = argv[3];

        printf("ready to connet with remote server: %s\n",IPaddress);
        tcp_socket(IPaddress);
    }
    return ;
}

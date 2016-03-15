#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
 
#define MSG_SIZE 64
#define SOCKET int
 
int status = -1; // 0 -conected, 1 - user, 2 - super-user, -1 disc-ed
int id;
int s;
char buf_pass[4];
 
 
int readn( SOCKET s, char *buf, int len)
{
        int cnt;
        int rc;
        cnt = len;
        while ( cnt > 0 )
        {
                rc = recv( s, buf, cnt, 0 );
                //puts(buf);
                //for (int i=0; i<cnt; i++) printf("%c",buf[i]);
                if ( rc < 0 )                           /* read error? */
                {
                        return -1;                              /* return error */
                }
                if ( rc == 0 )                          /* EOF? */
                        return len - cnt;               /* return short count */
                buf += rc;
                cnt -= rc;
        }
        return len;
}
 
int readvrec( SOCKET fd, char *bp, size_t len )
{
        u_int32_t reclen;
        int rc;
 
        /* Retrieve the length of the record */
 
        rc = readn( fd, ( char * )&reclen, sizeof( u_int32_t ) );
        if ( rc != sizeof( u_int32_t ) )
                return rc < 0 ? -1 : 0;
        reclen = ntohl( reclen );
        if ( reclen > len )
        {
                /*
                 *  Not enough room for the record--
                 *  discard it and return an error.
                 */
 
                while ( reclen > 0 )
                {
                        rc = readn( fd, bp, len );
                        if ( rc != len )
                                return rc < 0 ? -1 : 0;
                        reclen -= len;
                        if ( reclen < len )
                                len = reclen;
                }
                //set_errno( EMSGSIZE );
                return -1;
        }
 
        /* Retrieve the record itself */
 
        rc = readn( fd, bp, reclen );
        if ( rc != reclen )
                return rc < 0 ? -1 : 0;
        return rc;
}
 
struct packet_structure{
        int reclen;
        char buf[MSG_SIZE];
};
 
//determine type of user
void reg_user() {
        int n;
        struct packet_structure packet;
        char buf[MSG_SIZE];
        bzero(buf,MSG_SIZE);
        int rc;
        puts("Enter password:");
        if (fgets(packet.buf, sizeof(packet.buf), stdin) != NULL) {
                n = strlen(packet.buf);
                printf("strlen is %d\n",n);
                packet.reclen = n;
                if (send(s, (char*)&packet, n+sizeof(packet.reclen), 0) < 0)
                        perror("send error");
                else puts("pass sended");
        }
        //get status
        if ((rc = readn(s, buf, sizeof(buf) ) ) < 0) {
                perror("readvrec pass error");
                //exit(1);
        }
        printf("Status = %s \n ", buf);
        status = atoi(buf);
        if (status == 1)        puts("Status -> user");
        else    puts("Status -> Super-user");
        //status = atoi(buf);
        //printf("%d \n", &status);
}
 
void interaction() {
        char buf[MSG_SIZE];
        int n;
        struct packet_structure pack;
        while (1) {
                //bzero(buf, MSG_SIZE);
 
 
                if (fgets(pack.buf, sizeof(pack.buf), stdin) != NULL) {
                        n = strlen(pack.buf);
                        pack.reclen = n;
                        if (send(s, (char*)&pack, n + sizeof(pack.reclen), 0) < 0)
                                perror("send error");
                }
 
                fd_set rfds;
                struct timeval time_out;
                time_out.tv_sec = 0;
                time_out.tv_usec = 200000;
                while(1) {             
                        FD_ZERO(&rfds);
                        FD_SET(s, &rfds);
                        int s_res = select(s+1, &rfds, NULL, NULL, &time_out);
                        //printf("select_result = %d \n", s_res);
                        if (FD_ISSET(s, &rfds)) {
                                if (readn(s, buf, sizeof(buf)) <= 0)
                                        break;
                                puts(buf);
                        }
                        else    break;
 
                }
                       
        }
 
}
 
int getStatus() {
        return status;
}
int getId() {
        return id;
}
 
void* print_thread(void* args) {
        char buf[MSG_SIZE];
        while(1) {
                if (readn(s, buf, sizeof(buf)) < 0)
                        puts("readn print_thread error");
                printf("print_thread's buf = %s", buf);
        }
}
 
 
void main(void) {
 
        struct sockaddr_in serv;
        int rc;
        serv.sin_family = AF_INET;
        serv.sin_port = htons(27011);
        serv.sin_addr.s_addr = inet_addr("169.254.206.26");
        if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("Socket error");
                exit(1);
        }
        if ( (rc = connect(s, (struct sockaddr*)&serv, sizeof(serv))) ) {
                perror("Connect error");
                exit(1);
        }
        status = 0;
 
        //pthread_t tid_print;
        //pthread_create(&tid_print, NULL, &print_thread, NULL);
       
        reg_user();
        interaction();
 
 
 
}
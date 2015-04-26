#include<iostream>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
using namespace std;

#define DO 0xfd
#define WONT 0xfc
#define WILL 0xfb
#define DONT 0xfe
#define IAC 0xff
#define IAC_ECHO 1
#define NAWS 0x1F
#define SB 0xFA
#define SE 0xF0

// By Client
// IAC WILL NAWS
// IAC WON'T NAWS
// IAC SB NAWS <16-bit value> <16-bit value> IAC SE

// By Server
// IAC DO NAWS
// IAC DON'T NAWS

void negotiate(int sockfd, unsigned char *buf, int len)
{
    int i;
    int foundIAC = 0;
    unsigned char resp[256];

    printf("Negotiating....");
    int respIdx = 0;
    for(i=0 ; i < len ; i++)
    {
        printf("Parsing commands...\n");
        if (buf[i] == IAC)
        {
            foundIAC = 1;
            //printf("Found IAC...\n");
            resp[respIdx] = IAC;
            respIdx = respIdx + 1;
        }
        else if (buf[i] == DO && foundIAC == 1)
            if (buf[i+1] == NAWS)
            {
                printf ("Server said: IAC DO NAWS\n");
                resp[respIdx] = WILL;
                resp[respIdx+1] = NAWS;
                resp[respIdx+2] = IAC;
                resp[respIdx+3] = SB;
                resp[respIdx+4] = NAWS;
                resp[respIdx+5] = 0;
                resp[respIdx+6] = 80;
                resp[respIdx+7] = 0;
                resp[respIdx+8] = 24;
                resp[respIdx+9] = IAC;
                resp[respIdx+10] = SE;
                respIdx = respIdx + 11;
                printf ("Client says: IAC WILL NAWS IAC SB NAWS 0 80 0 64 SE\n");
                i++;
            }
            else
            {
                resp[respIdx] = WONT;
                respIdx = respIdx + 1;
            }
        else if (buf[i] == WILL && foundIAC == 1)
        {
            printf ("Server said: IAC WILL .....\n");
            resp[respIdx] = DO;
            printf ("Client says: IAC DO .....\n");
            respIdx = respIdx + 1;
        }
        else if (foundIAC == 1)
        {
            printf ("Server said: IAC something I dont know.. will echo .....\n");
            resp[respIdx] = buf[i];
            printf ("Client said: echoing %d.....\n",buf[i]);
            respIdx = respIdx + 1;
        }
    }

    foundIAC = 0;
    //printf ("Response is : %s\n",resp);
    if (send(sockfd,resp,respIdx,0) < 0)
    {
        exit(-1);
    }
}

int readuntil(int sockfd,fd_set fds,char *prpmt)
{
    int readRes, byteCnt, ioctlRes;
    int prmptFlg, len;
    int negStage;
    int cmplen = strlen(prpmt)+2;
    char lastNbytes[256];

    struct timeval selto;
    selto.tv_sec = 2;
    selto.tv_usec = 0;

    prmptFlg = 0;
    //printf("Flag is : %d\n",prmptFlg);
    byteCnt = 0;
    negStage = 0;

    while (prmptFlg == 0)
    {
        printf("Selecting...\n");
        int nready = select(sockfd+1,&fds,NULL, NULL, &selto);
        if (nready < 0 )
        {
            printf("Select Error...");
            return -1;
        }
        else if (nready == 0)
        {
            printf("Selection timeout...\n");
            printf("Last Bytes were : %s\n",lastNbytes);
            selto.tv_sec = 2;
            selto.tv_usec = 0;
        }
        else if (sockfd != 0 && FD_ISSET(sockfd, &fds))
        {
            ioctlRes = ioctl(sockfd, FIONREAD, &len);
            assert (ioctlRes>=0);
            //printf("Length is %d\n",len);

            if (len > 0)
            {
                unsigned char rdBytes[len];
                readRes = recv(sockfd, &rdBytes,len,0);
                if (readRes < 0)
                {
                    printf("ERROR reading data");
                    return -1;
                }
                else if (readRes == 0)
                {
                    printf("Connection closed by remote host\n");
                    return -1;
                }
                int i;
                for (i=0;i<len;i++)
                {
                    if (rdBytes[i] == IAC)
                    {
                        negStage = 1;
                        //printf("Found IAC in readuntil...\n");
                    }
                }
                byteCnt = byteCnt + len;
                if (negStage == 1)
                {
                    negotiate(sockfd,&rdBytes[0],len);
                    negStage = 0;
                }
                //printf("Flag is : %d\n",prmptFlg);
                int c = 0;
                while (c < cmplen && len-cmplen+c-1 >= 0)
                {
                    lastNbytes[c] = rdBytes[len-cmplen+c-1];
                    c++;
                }
                lastNbytes[c] = '\0';
                //printf("Last Bytes are : %s\n",lastNbytes);
                if (strstr(lastNbytes, prpmt) != NULL)
                {
                    prmptFlg = 1;
                    printf("Prompt Reached!\n");
                }
            }
        }
    }
    return 1;
}

int SendData(int sockfd,char *data)
{
    int len = strlen(data);
    if (send(sockfd,data,len,0) < 0)
    {
        printf("Data sending failed...\n");
        exit(-1);
    }
    return 1;
}


int main()
{
    // AF_INET = Internet doamin socket for use with IPv4 address
    // SOCK_STREAM = Byte-stream socket
    // AF_UNSPEC = unspecified protocol
    int shutResp, sockopRes;
    int sockfd;
    struct sockaddr_in srv_addr;
    char serverIp[] = "192.168.1.100";
    int port = 23;

    // Socket timeout settings
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    // Define connect parameters // Destination address and port
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);  // Convert to network byte order
    srv_addr.sin_addr.s_addr = inet_addr(serverIp);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, AF_UNSPEC);
    assert (sockfd>=0);

    // Select setup
    fd_set fds;
    FD_ZERO(&fds);
    if (sockfd != 0)
        FD_SET(sockfd, &fds);
    FD_SET(0, &fds);

    // Set socket options // Timeout
    sockopRes = setsockopt(sockfd,SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout));
    assert (sockopRes>=0);

    // Connect using socket
    if (connect(sockfd,(struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0)
    {
        printf("Error Connecting, errno is %d \n", errno);
        return -1;
    }
    printf("Connected to %s\n", serverIp);
    // Finding login prompt
    char prpmt[] = "login:";
    readuntil(sockfd, fds ,prpmt);
    // Sending username
    char sndCmd1[] = "root\r\n";
    SendData(sockfd,sndCmd1);
    // Finding password prompt
    char prpmt1[] = "ssword:";
    readuntil(sockfd, fds ,prpmt1);
    // Sending password
    char sndCmd2[] = "passowrd\r\n";
    SendData(sockfd,sndCmd2);
    // Finding command prompt
    char prpmt2[] = "#";
    readuntil(sockfd, fds ,prpmt2);
    char sndCmd3[] = "ls\r\n";
    SendData(sockfd,sndCmd3);

    readuntil(sockfd, fds ,prpmt2);

    shutResp = shutdown(sockfd,0);
    printf("Shutdown response is %d, errno is %d \n",shutResp, errno);
    assert (shutResp>=0);
    return 1;
}

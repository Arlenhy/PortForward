#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <errno.h>
#include <signal.h>

#pragma comment(lib, "ws2_32.lib")

#define TIMEOUT 300
#define MAXSIZE 20480
#define HOSTLEN 40
#define CONNECTNUM 5

struct transocket
{
	SOCKET fd1;
	SOCKET fd2;
	char host[HOSTLEN];
	int port;
};

int create_socket();
int create_server(int sockfd, int port);
void transmitdata(LPVOID data);
void bind2conn(int port1, char *host, int port2);
void ascii2bin(char *p, int len);


int main(int argc, char* argv[])
{
//	if(argc != 4)
//		return 0;

	char sConnectHost[HOSTLEN], sTransmitHost[HOSTLEN];
	int iConnectPort = 0, iTransmitPort = 0;

	memset(sConnectHost, 0, HOSTLEN);
	memset(sTransmitHost, 0, HOSTLEN);

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	iConnectPort = 5678;
	strncpy_s(sTransmitHost, "127.0.0.1", HOSTLEN);
	iTransmitPort = 7788;
	bind2conn(iConnectPort, sTransmitHost, iTransmitPort);
	WSACleanup();
	return 0;
}

void bind2conn(int port1, char *host, int port2)
{
	SOCKET sockfd1, sockfd2;
	int size;
	char buffer[1024];

	HANDLE hThread = NULL;
	transocket sock;
	DWORD dwThreadID;

	if(port1 > 65535 || port1 < 1)
	{
		printf("[-] ConnectPort invalid.\r\n");
		return;
	}

	if(port2 > 65535 || port2 < 1)
	{
		printf("[-] TransmitPort invalid.\r\n");
		return;
	}

	memset(buffer, 0, 1024);

	if((sockfd1 = create_socket()) == INVALID_SOCKET) return;

	if(create_server(sockfd1, port1) == 0)
	{
		closesocket(sockfd1);
		return;
	}

	if((sockfd2 = create_socket()) == 0) return;
	
	if((create_server(sockfd2, 6789)) == 0)
	{
		closesocket(sockfd1);
		closesocket(sockfd2);
		return;
	}

	size = sizeof(struct sockaddr);

	while(1)
	{
		sock.fd1 = sockfd1;
		sock.fd2 = sockfd2;
		memcpy((char *)sock.host, host, HOSTLEN);
		sock.port = port2;

		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)transmitdata, (LPVOID)&sock, 0, &dwThreadID);

 		if(hThread == NULL)
		{
			TerminateThread(hThread, 0);
			return ;
		}

		Sleep(10000);
	}

}
void transmitdata(LPVOID data)
{
	SOCKET fd1, fd2;
	transocket *sock;
	struct timeval timeset;
	fd_set readfd,writefd;
	int result,i=0;
	char read_in1[MAXSIZE],send_out1[MAXSIZE];
	char read_in2[MAXSIZE],send_out2[MAXSIZE];
	int read1=0,totalread1=0,send1=0;
	int read2=0,totalread2=0,send2=0;
	int sendcount1,sendcount2;
	int maxfd;
	struct sockaddr_in client1,client2;
	int structsize1,structsize2;
	char host1[20],host2[20];
	int port1=0,port2=0;
	char tmpbuf[100];

	sock = (transocket *)data;
	fd1 = sock->fd1;
	fd2 = sock->fd2;
	client2.sin_family = AF_INET;
	client2.sin_addr.S_un.S_addr = inet_addr(sock->host);
	client2.sin_port = htons(sock->port);

	memset(host1, 0, 20);
	memset(host2, 0, 20);
	memset(tmpbuf, 0, 100);

	structsize1=sizeof(struct sockaddr);
	structsize2=sizeof(struct sockaddr);

	maxfd = max(fd1, fd2) + 1;
	memset(read_in1,0,MAXSIZE);
	memset(read_in2,0,MAXSIZE);
	memset(send_out1,0,MAXSIZE);
	memset(send_out2,0,MAXSIZE);

	timeset.tv_sec = TIMEOUT;
	timeset.tv_usec = 0;

	while(1)
	{
		FD_ZERO(&readfd);
		FD_ZERO(&writefd);

		FD_SET((UINT)fd1, &readfd);
		FD_SET((UINT)fd1, &writefd);
		FD_SET((UINT)fd2, &readfd);
		FD_SET((UINT)fd2, &writefd);

		result = select(maxfd, &readfd, &writefd, NULL, &timeset);
		if((result<0) && (errno!=EINTR))
		{
			printf("[-] Select error.\r\n");
			break;
		}
		else if(result==0)
		{
			printf("[-] Socket time out.\r\n");
			break;
		}

 		if(FD_ISSET(fd1, &readfd))
		{
//			printf("[+] Select fd1 OK.\r\n");
			if(totalread1 < MAXSIZE)
			{
				read1 = recvfrom(fd1, read_in1, MAXSIZE - totalread1, 0, (sockaddr *)&client1, &structsize1);
				
				if((read1 == SOCKET_ERROR) || (read1 == 0))
				{
					printf("[-] Read fd1 data error,maybe close?\r\n");
					break;
				}
				memcpy(send_out1 + totalread1, read_in1, read1);
				printf(" Recv from client %5d bytes.\r\n", read1);
				totalread1 += read1;
				memset(read_in1, 0, MAXSIZE);
			}
		}

		if(FD_ISSET(fd2, &writefd))
		{
			int err = 0;
			sendcount1 = 0;
			while(totalread1 > 0)
			{
				send1 = sendto(fd2, send_out1 + sendcount1, totalread1, 0, (sockaddr *)&client2, structsize1);
				if(send1 == 0) break;
				if((send1 < 0) && (errno != EINTR))
				{
					printf("[-] Send to fd2 unknow error.\r\n");
					err = 1;
					break;
				}

				if((send1 < 0) && (errno == ENOSPC)) break;
				sendcount1 += send1;
				totalread1 -= send1;

				printf(" Send to server %5d bytes.\r\n", send1);
			}

			if(err == 1) break;
			if((totalread1 > 0) && (sendcount1 > 0))
			{
				memcpy(send_out1,send_out1+sendcount1,totalread1);
				memset(send_out1+totalread1,0,MAXSIZE-totalread1);
			}
			else
				memset(send_out1,0,MAXSIZE);
		}

		if(FD_ISSET(fd2, &readfd))
		{
			if(totalread2 < MAXSIZE)
			{
				read2 = recvfrom(fd2, read_in2, MAXSIZE - totalread2, 0, (sockaddr*)&client2, &structsize2);
				if(read2==0)break;
				if((read2<0) && (errno!=EINTR))
				{
					printf("[-] Read fd2 data error,maybe close?\r\n\r\n");
					break;
				}

				memcpy(send_out2+totalread2,read_in2,read2);
				printf(" Recv from server %5d bytes.\r\n", read2);
				totalread2+=read2;
				memset(read_in2,0,MAXSIZE);
			}
		}

		if(FD_ISSET(fd1, &writefd))
		{
			int err2=0;
			sendcount2=0;
			while(totalread2>0)
			{
				send2 = sendto(fd1, send_out2+sendcount2, totalread2, 0, (sockaddr*)&client1, structsize1);
				if(send2==0) break;
				if((send2<0) && (errno!=EINTR))
				{
					printf("[-] Send to fd1 unknow error.\r\n");
					err2=1;
					break;
				}
				if((send2<0) && (errno==ENOSPC)) break;
				sendcount2+=send2;
				totalread2-=send2;

				printf(" Send to client%5d bytes.\r\n", send2);
			}
			if(err2==1) break;
			if((totalread2>0) && (sendcount2 > 0))
			{
				memcpy(send_out2, send_out2+sendcount2, totalread2);
				memset(send_out2+totalread2, 0, MAXSIZE-totalread2);
			}
			else
				memset(send_out2,0,MAXSIZE);
			Sleep(5);
		}
	}
	closesocket(fd1);
	closesocket(fd2);

	printf("\r\n[+] OK! I Closed The Two Socket.\r\n"); 
}

int create_socket()
{
	int sockfd;
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd < 0)
	{
		printf("[-] Create socket error.\r\n");
	}
	return sockfd;
}
int create_server(int sockfd, int port)
{
	struct sockaddr_in srvaddr;
	int on = 1;

	memset(&srvaddr, 0, sizeof(struct sockaddr));

	srvaddr.sin_port = htons(port);
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if(bind(sockfd, (struct sockaddr *)&srvaddr, sizeof(struct sockaddr)) < 0)
	{
		printf("[-] Socket bind %d port error.\r\n", port);
		return 0;
	}
	return 1;
}
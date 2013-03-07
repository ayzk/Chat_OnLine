#include <winsock2.h>
#include <iostream>
#pragma comment(lib, "ws2_32")
int gPort = 7777;
using namespace std;
 void main()
 {
        SOCKET lhSocket;
        SOCKADDR_IN lSockAddr;
        WSADATA wsaData;
        int lConnect;
       int lLength;
        char lData[]="SendData";
        if(WSAStartup(MAKEWORD(2,0),&wsaData) != 0)
        {
             cout<<"Socket Initialization Error. Program aborted\n";
             return;
         }
        lhSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(lhSocket == INVALID_SOCKET)
        {
             cout<<"Invalid Socket "<<GetLastError()<<". Program Aborted\n"<<endl;
         }
        memset(&lSockAddr,0, sizeof(lSockAddr));
        lSockAddr.sin_family = AF_INET;
        lSockAddr.sin_port = htons(gPort);
        lSockAddr.sin_addr.s_addr = inet_addr("140.112.253.61");
        lConnect = connect(lhSocket,(SOCKADDR *)&lSockAddr,sizeof(SOCKADDR_IN));
        if(lConnect != 0)
        {
              cout<<"Connect Error. Program aborted\n";
              return;
         }
        /*lLength = send(lhSocket,lData,strlen(lData),0);
        if(lLength < strlen(lData))
        {
             cout<<"Send Error.\n";
        }*/
        closesocket(lhSocket);
		system("pause");
        return;
 }
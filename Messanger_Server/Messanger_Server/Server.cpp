#include <WinSock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma comment(lib, "ws2_32")
#define MAX_FILE 1024
#define MAX_USER 255

typedef struct User{
	char Name[255];
	char Pwd[255];
	bool OnLine;
	HANDLE Mutex;
	HANDLE HThread;
	SOCKET Sock;
} User;

typedef struct FileHwnd{
	int Sender, Rcpt;
	int Status;
} FileHwnd;

SOCKET ServSock;
WORD SockVer;
WSADATA WsaData;
HANDLE HServThread;

FileHwnd FHwnd[MAX_FILE];
User UList[MAX_USER];
int UserCnt;

HANDLE HListMutex;
HANDLE HFileMutex;

bool Running;
bool Debug;

void inline PrintByte(char* buffer, int len){
	int i;
	for(i = 0; i < len; i++){
		unsigned char t = (unsigned char)buffer[i];
		printf("%02X ", t);
	}
}

int snd(SOCKET Sock, char* buffer, int len, int flags){
	if(Debug){		
		struct sockaddr  name;
		int size = sizeof(name);
		getpeername(Sock, &name, &size);
		printf("=> %u.%u.%u.%u : %u\n", (unsigned char)name.sa_data[2], (unsigned char)name.sa_data[3], (unsigned char)name.sa_data[4], (unsigned char)name.sa_data[5], (unsigned short)((unsigned short)name.sa_data[0] * 256 + (unsigned short)name.sa_data[1]));
		PrintByte(buffer, len);
		printf("\n");
	}
	return send(Sock, buffer, len, flags);
}

int rcv(SOCKET Sock, char* buffer, int len, int flags){
	int ret = recv(Sock, buffer, len, flags);
	if(Debug && ret > 0){		
		struct sockaddr  name;
		int size = sizeof(name);
		getpeername(Sock, &name, &size);
		printf("<= %u.%u.%u.%u : %u\n", (unsigned char)name.sa_data[2], (unsigned char)name.sa_data[3], (unsigned char)name.sa_data[4], (unsigned char)name.sa_data[5], (unsigned short)((unsigned short)name.sa_data[0] * 256 + (unsigned short)name.sa_data[1]));
		PrintByte(buffer, ret);
		printf("\n");
	}
	return ret;
}


void StrToByte(unsigned char* buffer, char* str){
	int i, j, s, l = strlen(str);
	char tmp;
	
	s = 0;
	for(i = j = 0; i < l; i ++){	
		if(str[i] >= '0' && str[i] <= '9'){
			if(s == 0){
				tmp = str[i] - '0';
				s = 1;
			}
			else if(s == 1){
				tmp = (tmp << 4) + str[i] - '0';
				buffer[j++] = tmp;
				s = 0;
			}
		}
		else if(str[i] >= 'a' && str[i] <= 'f'){
			if(s == 0){
				tmp = str[i] - 'a' + 10;
				s = 1;
			}
			else if(s == 1){
				tmp = (tmp << 4) + str[i] - 'a' + 10;
				buffer[j++] = tmp;
				s = 0;
			}
		}
		else if(str[i] >= 'A' && str[i] <= 'F'){
			if(s == 0){
				tmp = str[i] - 'A' + 10;
				s = 1;
			}
			else if(s == 1){
				tmp = (tmp << 4) + str[i] - 'A' + 10;
				buffer[j++] = tmp;
				s = 0;
			}
		}
	}
}

bool Register(char* Name, char* Pwd){
	int i, ret;
	
	if(UserCnt >= MAX_USER)	return false;

	if(WaitForSingleObject(HListMutex, INFINITE) != WAIT_OBJECT_0)	return false;
	for(i = 0; i < UserCnt; i++){
		if(strncmp(UList[i].Name, Name, 256) == 0){
			ReleaseMutex(HListMutex);
			return false;
		}
	}

	strncpy(UList[UserCnt].Name, Name, 256);
	strncpy(UList[UserCnt].Pwd, Pwd, 256);
	
	FILE* FUList = fopen("UserList.txt", "a");
	if(FUList == NULL){
		ReleaseMutex(HListMutex);
		return false;
	}
	fprintf(FUList, "%s %s\n", UList[UserCnt].Name, UList[UserCnt].Pwd);
	fclose(FUList);

	UList[UserCnt].Mutex = CreateMutex(NULL, false, NULL);
	if(Debug)	printf("User Registered (%s, %s)\n", UList[UserCnt].Name, UList[UserCnt].Pwd);
	UserCnt++;
	
	ReleaseMutex(HListMutex);

	return true;
}

int Login(char* Name, char* Pwd){
	int i, ret = -1;

	if(WaitForSingleObject(HListMutex, INFINITE) != WAIT_OBJECT_0)	return -1;
	for(i = 0; i < UserCnt; i++){
		if(strncmp(UList[i].Name, Name, 256) == 0 && strncmp(UList[i].Pwd, Pwd, 256) == 0 && !UList[i].OnLine){
			ret = i;
			break;
		}
	}
	if(ret != -1 && Debug)	if(Debug)	printf("User %d Logged In\n", ret);
	ReleaseMutex(HListMutex);
	return ret;
}

int GetSession(int src, int dest){
	int i, ret = -1;
	if(WaitForSingleObject(HFileMutex, INFINITE) != WAIT_OBJECT_0){
		if(Debug)	printf("Error Lock File List\n");
		return -1;
	}
	for(i = 0; i < MAX_FILE; i++){
		if(FHwnd[i].Status == 0){
			FHwnd[i].Status = 1;
			FHwnd[i].Sender = src;
			FHwnd[i].Rcpt = dest;
			ret = i;
			break;
		}
	}
	ReleaseMutex(HFileMutex);
	return ret;
}

bool SendTo(int sn, char* buffer, int len){
	if(WaitForSingleObject(UList[sn].Mutex, INFINITE) != WAIT_OBJECT_0){
		if(Debug)	printf("Error Lock Sock\n");
		return false;
	}
	if(!UList[sn].OnLine)	return false;
	snd(UList[sn].Sock, buffer, len, 0);
	ReleaseMutex(UList[sn].Mutex);
	return true;
}

DWORD Serve(LPVOID lpdwThreadParam){
	SOCKET Sock = *((SOCKET*)lpdwThreadParam);
	unsigned char *rbuffer, *wbuffer;
	int i, ret, len, len2;
	int dest, Hwnd, sn = -1;	//sn = Current User Sn

	rbuffer = (unsigned char*)malloc(1024);
	wbuffer = (unsigned char*)malloc(1024);

	if(Debug)	if(Debug)	printf("Client Connected\n");

	//Login, Reg
	while(true){
		ret = rcv(Sock, (char*)rbuffer, 2, MSG_WAITALL);
		if(ret == 2 && rbuffer[0] == 0x01){
			//Reg
			if(rbuffer[1] == 0x00){
				char id[256], pwd[256];
				//Read ID
				ret = rcv(Sock, (char*)rbuffer, 1, MSG_WAITALL);
				if(ret != 1){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				len = rbuffer[0];
				ret = rcv(Sock, (char*)rbuffer, len, MSG_WAITALL);
				if(ret != len){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				rbuffer[len] = '\0';
				strncpy(id, (char*)rbuffer, 256);
				//Read Pwd
				ret = rcv(Sock, (char*)rbuffer, 1, MSG_WAITALL);
				if(ret != 1){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				len2 = rbuffer[0];
				ret = rcv(Sock, (char*)rbuffer, len2, MSG_WAITALL);
				if(ret != len2){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				rbuffer[len2] = '\0';
				strncpy(pwd, (char*)rbuffer, 256);
				//Register
				if(Register(id, pwd)){
					StrToByte(wbuffer, "01 01 00");
				}
				else{
					StrToByte(wbuffer, "01 01 01");
				}
				snd(Sock, (char*)wbuffer, 3, 0);
			}
			//Login
			else if(rbuffer[1] == 0x02){
				char id[256], pwd[256];
				//Read ID
				ret = rcv(Sock, (char*)rbuffer, 1, MSG_WAITALL);
				if(ret != 1){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				len =  rbuffer[0];
				ret = rcv(Sock, (char*)rbuffer, len, MSG_WAITALL);
				if(ret != len){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				rbuffer[len] = '\0';
				strncpy(id,(char*)rbuffer, 256);
				//Read Pwd
				ret = rcv(Sock, (char*)rbuffer, 1, MSG_WAITALL);
				if(ret != 1){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				len2 = rbuffer[0];
				ret = rcv(Sock, (char*)rbuffer, len2, MSG_WAITALL);
				if(ret != len2){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				rbuffer[len2] = '\0';
				strncpy(pwd, (char*)rbuffer, 256);
				sn = Login(id, pwd);
				if(sn != -1){
					StrToByte(wbuffer, "01 03 00");
					snd(Sock, (char*)wbuffer, 3, 0);
					break;
				}
				else{
					StrToByte(wbuffer, "01 03 01");
					snd(Sock, (char*)wbuffer, 3, 0);
				}			
			}
			else{
				if(Debug)	printf("Protocol Error\n");
				goto out;
			}
		}
		else{
			if(Debug)	printf("Socket Error\n");
			goto out;
		}
	}

	//Update User Data
	if(WaitForSingleObject(HListMutex, INFINITE) != WAIT_OBJECT_0){
		if(Debug)	printf("Error Update List\n");
		goto out;
	}

	UList[sn].OnLine = true;
	UList[sn].Sock = Sock;
	UList[sn].HThread = GetCurrentThread();

	//Boardcase
	for(i = 0; i < UserCnt; i++){
		if(UList[i].OnLine){
			if(WaitForSingleObject(UList[i].Mutex, INFINITE) != WAIT_OBJECT_0){
				if(Debug)	printf("Error Notify %d\n", i);
				continue;
			}
			StrToByte(wbuffer, "03 00");
			wbuffer[2] = sn;
			len = strlen(UList[sn].Name);
			wbuffer[3] = len;
			strncpy((char*)(wbuffer + 4), UList[sn].Name, 256);
			wbuffer[len + 4] = 1;
			snd(UList[i].Sock, (char*)wbuffer, len + 5, 0);
			ReleaseMutex(UList[i].Mutex);
		}
	}
	
	//Send User List
	if(WaitForSingleObject(UList[sn].Mutex, INFINITE) != WAIT_OBJECT_0){
		ReleaseMutex(HListMutex);
		if(Debug)	printf("Error Get Sock\n");
		goto out;
	}
	for(i = 0; i < UserCnt; i++){
		StrToByte(wbuffer, "03 00");
		wbuffer[2] = i;
		len = strlen(UList[i].Name) + 1;
		wbuffer[3] = len;
		strncpy((char*)(wbuffer + 4), UList[i].Name, 256);
		if(UList[i].OnLine)	wbuffer[len + 4] = 1;
		else	wbuffer[len + 4] = 0;
		ret = snd(UList[sn].Sock, (char*)wbuffer, len + 5, 0);
	}

	//Send Offline Msg
	char fname[1024] = "Msgbox\\";
	strncat(fname, UList[sn].Name, 256);
	FILE* FMsg = fopen(fname, "rb");
	if(FMsg != NULL){
		while(true){
			len = fread(wbuffer + 2, 1, 2, FMsg);
			if(len != 2){
				break;
			}
			StrToByte(wbuffer, "02 00");
			len = wbuffer[3];
			fread(wbuffer + 4, 1, len, FMsg);
			snd(UList[sn].Sock, (char*)wbuffer, len + 4, 0);
		}
		fclose(FMsg);
	}
	DeleteFile(fname);
	ReleaseMutex(UList[sn].Mutex);

	ReleaseMutex(HListMutex);

	while(true){
		len = rcv(UList[sn].Sock, (char*)rbuffer, 2, MSG_WAITALL);
		if(len != 2){
			if(Debug)	printf("Protocol Error\n");
			goto out;
		}

		switch(rbuffer[0]){
		case 1:
			switch(rbuffer[1]){
			case 4:	//Logout
				goto out;
				break;
			default:
				if(Debug)	printf("Protocol Error\n");
				goto out;
			}
			break;
		case 2:
			switch(rbuffer[1]){			
			case 0:	//Send
				len = rcv(UList[sn].Sock, (char*)(rbuffer + 2), 2, MSG_WAITALL);
				if(len != 2){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				dest = rbuffer[2];
				len = rcv(UList[sn].Sock, (char*)(rbuffer + 4), rbuffer[3], MSG_WAITALL);
				if(len != rbuffer[3]){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				if(dest == 255){
					for(i = 0; i < UserCnt; i++){
						if(WaitForSingleObject(UList[i].Mutex, INFINITE) != WAIT_OBJECT_0){
							if(Debug)	printf("Error Lock Sock \n");
							continue;
						}
						if(UList[i].OnLine){
							rbuffer[2] = sn;
							len = rbuffer[3];
							snd(UList[dest].Sock, (char*)rbuffer, len + 4, 0);
						}
						else{
							char fname[1024] = "Msgbox\\";
							strncat(fname, UList[i].Name, 256);
							FILE* FMsg = fopen(fname, "ab");
							if(FMsg != NULL){
								fwrite(rbuffer + 2, 1, len + 2, FMsg);
								fclose(FMsg);
							}
							else{
								if(Debug)	printf("Error Write Msg Box\n");
							}
						}
						ReleaseMutex(UList[i].Mutex);
					}
				}
				else{
					if(WaitForSingleObject(UList[dest].Mutex, INFINITE) != WAIT_OBJECT_0){
						if(Debug)	printf("Error Lock Sock\n");
						continue;
					}
					if(UList[dest].OnLine){
						rbuffer[2] = sn;
						len = rbuffer[3];
						snd(UList[dest].Sock, (char*)rbuffer, len + 4, 0);
					}
					else{
						char fname[1024] = "Msgbox\\";
						strncat(fname, UList[dest].Name, 256);
						FILE* FMsg = fopen(fname, "ab");
						if(FMsg != NULL){
							fwrite(rbuffer + 2, 1, len + 2, FMsg);
							fclose(FMsg);
						}
						else{
							if(Debug)	printf("Error Write Msg Box\n");
						}
					}
					ReleaseMutex(UList[dest].Mutex);
				}
				break;
			case 1:
				len = rcv(UList[sn].Sock, (char*)(rbuffer + 2), 2, MSG_WAITALL);
				if(len != 2){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				dest = rbuffer[2];
				len = rcv(UList[sn].Sock, (char*)(rbuffer + 4), rbuffer[3], MSG_WAITALL);
				if(len != rbuffer[3]){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				Hwnd = GetSession(sn, dest);
				if(Hwnd != -1){
					StrToByte(wbuffer, "02 02");
					wbuffer[2] = Hwnd & 0xFF;
					wbuffer[3] = (Hwnd & 0xFF00) >> 8;
					wbuffer[4] = (Hwnd & 0xFF0000) >> 16;
					wbuffer[5] = (Hwnd & 0xFF000000) >> 24;
				}
				else{
					StrToByte(wbuffer, "02 02 FF FF FF FF");
				}
				if(!SendTo(sn, (char*)wbuffer, 6)){
					if(Debug)	printf("Fatal Error\n");
					goto out;
				}
				if(Hwnd != -1){
					wbuffer[1] = 0x03;
					wbuffer[6] = sn;
					wbuffer[7] = rbuffer[3];
					memcpy(wbuffer + 8, rbuffer + 4, rbuffer[3]);
					if(!SendTo(dest, (char*)wbuffer, rbuffer[3] + 8)){
						//fail
						wbuffer[1] = 0x04;
						wbuffer[6] = 0x01;
						if(!SendTo(dest, (char*)wbuffer, 7)){
							if(Debug)	printf("Error Response\n");
							continue;
						}
					}
				}
				break;
			case 4:
				len = rcv(UList[sn].Sock, (char*)(rbuffer + 2), 5, MSG_WAITALL);
				if(len != 5){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				
				Hwnd = rbuffer[5];
				Hwnd = Hwnd << 8 | rbuffer[4];
				Hwnd = Hwnd << 8 | rbuffer[3];
				Hwnd = Hwnd << 8 | rbuffer[2];
				dest = FHwnd[Hwnd].Sender;				
					
				if(WaitForSingleObject(HFileMutex, INFINITE) != WAIT_OBJECT_0){
					if(Debug)	printf("Error Lock File List\n");
					continue;
				}
				if(rbuffer[6] == 0){
					if(FHwnd[Hwnd].Status != 1 || FHwnd[Hwnd].Rcpt != sn){
						ReleaseMutex(HFileMutex);
						if(Debug)	printf("Faltal Error\n");
						goto out;
					}
					FHwnd[Hwnd].Status = 2;
				}
				else if(rbuffer[6] == 1){
					if(FHwnd[Hwnd].Status != 1 || FHwnd[Hwnd].Rcpt != sn){
						ReleaseMutex(HFileMutex);
						if(Debug)	printf("Faltal Error\n");
						goto out;
					}
					FHwnd[Hwnd].Status = 0;
				}
				ReleaseMutex(HFileMutex);
				
				if(!SendTo(dest, (char*)rbuffer, 7)){
					if(Debug)	printf("Error Send\n");
					continue;
				}
				break;
			case 5:
				len = rcv(UList[sn].Sock, (char*)rbuffer + 2, 5, MSG_WAITALL);
				if(len != 5){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				len = rcv(UList[sn].Sock, (char*)(rbuffer + 7), rbuffer[6], MSG_WAITALL);
				if(len != rbuffer[6]){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}

				Hwnd = rbuffer[5];
				Hwnd = Hwnd << 8 | rbuffer[4];
				Hwnd = Hwnd << 8 | rbuffer[3];
				Hwnd = Hwnd << 8 | rbuffer[2];
				
				if(WaitForSingleObject(HFileMutex, INFINITE) != WAIT_OBJECT_0){
					if(Debug)	printf("Error Lock File List\n");
					continue;
				}
				dest = FHwnd[Hwnd].Rcpt;
				if(FHwnd[Hwnd].Status != 2 || FHwnd[Hwnd].Sender != sn){
					ReleaseMutex(HFileMutex);
					if(Debug)	printf("Invalid File Content\n");
					continue;
				}
				ReleaseMutex(HFileMutex);

				if(!SendTo(dest, (char*)rbuffer, rbuffer[6] + 7)){
					if(Debug)	printf("Error Send\n");
				}
				break;
			case 6:
				len = rcv(UList[sn].Sock, (char*)(rbuffer + 2), 4, MSG_WAITALL);
				if(len != 4){
					if(Debug)	printf("Protocol Error\n");
					goto out;
				}
				
				Hwnd = rbuffer[5];
				Hwnd = Hwnd << 8 | rbuffer[4];
				Hwnd = Hwnd << 8 | rbuffer[3];
				Hwnd = Hwnd << 8 | rbuffer[2];

				if(WaitForSingleObject(HFileMutex, INFINITE) != WAIT_OBJECT_0){
					if(Debug)	printf("Error Lock File List\n");
					continue;
				}
				if(FHwnd[Hwnd].Status == 0){
					ReleaseMutex(HFileMutex);
					if(Debug)	printf("Protocol Error\n");
					continue;
				}
				if(FHwnd[Hwnd].Sender == sn){
					FHwnd[Hwnd].Status = 0;
					dest = FHwnd[Hwnd].Rcpt;
				}
				else if(FHwnd[Hwnd].Rcpt == sn){
					FHwnd[Hwnd].Status = 0;
					dest = FHwnd[Hwnd].Sender;
				}
				else{
					ReleaseMutex(HFileMutex);
					if(Debug)	printf("Protocol Error\n");
					continue;
				}
				ReleaseMutex(HFileMutex);
				if(!SendTo(dest, (char*)rbuffer, 6)){
					if(Debug)	printf("Error Send\n");
				}
				break;
			default:
				if(Debug)	printf("Protocol Error\n");
				goto out;
			}
			break;
		default:
			if(Debug)	printf("Protocol Error\n");
			goto out;
		}
	}

out:;

	if(sn != -1){
		if(WaitForSingleObject(HListMutex, INFINITE) == WAIT_OBJECT_0){
			UList[sn].OnLine = false;
			UList[sn].Sock = INVALID_SOCKET;
			UList[sn].HThread = NULL;

			//Boardcase
			for(i = 0; i < UserCnt; i++){
				if(UList[i].OnLine){
					if(WaitForSingleObject(UList[i].Mutex, INFINITE) != WAIT_OBJECT_0){
						if(Debug)	printf("Error Notigy %d\n", i);
						continue;
					}
					StrToByte(wbuffer, "03 00");
					wbuffer[2] = sn;
					len = strlen(UList[sn].Name) + 1;
					wbuffer[3] = len;
					strncpy((char*)(wbuffer + 4), UList[sn].Name, 256);
					wbuffer[len + 4] = 0;
					snd(UList[i].Sock, (char*)wbuffer, len + 5, 0);
					ReleaseMutex(UList[i].Mutex);
				}
			}
			ReleaseMutex(HListMutex);
		}
		else{
			if(Debug)	printf("Error Update List\n");
		}		

		//Close File Handle
		if(WaitForSingleObject(HFileMutex, INFINITE) == WAIT_OBJECT_0){
			for(i = 0; i < MAX_FILE; i++){
				if(FHwnd[i].Status != 0){
					if(FHwnd[i].Sender == sn || FHwnd[i].Rcpt == sn){
						FHwnd[i].Status = 0;
						if(FHwnd[i].Sender == sn)	dest = FHwnd[i].Rcpt;
						else	dest = FHwnd[i].Sender;
						StrToByte(rbuffer, "02 06");
						wbuffer[2] = i & 0xFF;
						wbuffer[3] = (i & 0xFF00) >> 8;
						wbuffer[4] = (i & 0xFF0000) >> 16;
						wbuffer[5] = (i & 0xFF000000) >> 24;
						if(!SendTo(dest, (char*)rbuffer, 6)){
							if(Debug)	printf("Error Close Handle\n");
						}
					}
				}
			}
			ReleaseMutex(HFileMutex);
		}
		else{
			if(Debug)	printf("Error Lock File List\n");
		}

		if(Debug)	printf("User %d Leave\n", sn);
	}
	else{
		if(Debug)	printf("Client Leave\n");
	}

	free(rbuffer);
	free(wbuffer);
	closesocket(Sock);
	CloseHandle(GetCurrentThread());
	return 0;
}

void PrintIP(){
	char hostname[80];
	if (gethostname(hostname, sizeof(hostname)) != SOCKET_ERROR) {
		struct hostent *phe = gethostbyname(hostname);
		if (phe != 0) {
			for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
				struct in_addr addr;
				memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
				printf("%s\n", inet_ntoa(addr));
				break;
			}
		}
	}
}

DWORD Start(LPVOID lpdwThreadParam){
	int ret;

    /// Creating socket
    ServSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(ServSock == INVALID_SOCKET)
    {
        printf("Unable to create socket\n");
        WSACleanup();
        return SOCKET_ERROR;
    }
 
    /// Filling in sockaddr_in struct 
    SOCKADDR_IN sin;
    sin.sin_family = PF_INET;
    sin.sin_port = htons(7777);
    sin.sin_addr.s_addr = INADDR_ANY;
    ret = bind(ServSock, (LPSOCKADDR)&sin, sizeof(sin));
    if(ret == SOCKET_ERROR)
    {
		printf("Unable to bind\n");
        WSACleanup();
        return SOCKET_ERROR;
    }
 
    /// Trying to listen socket
    ret = listen(ServSock, 10);
    if(ret == SOCKET_ERROR)
    {
		printf("Unable to listen\n");
        WSACleanup();
        return SOCKET_ERROR;
    }

	printf("Service Started\n");
	printf("IP: ");
	PrintIP();
	printf("PORT: 7777\n");
 
    /// Waiting for a client
	while(Running){
		SOCKET* clientSock = (SOCKET*)malloc(sizeof(SOCKET));
		*clientSock = accept(ServSock, NULL, NULL); 
		if(*clientSock == INVALID_SOCKET)
		{
			break;
		}
		else
		{
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&Serve, clientSock, 0, NULL);
		}
	}

	return 0;
}

int main(){
    int ret, i;

	Debug = false;
	Running = true;

	SockVer = MAKEWORD(2,2); 
	WSAStartup(SockVer, &WsaData);

	memset(UList, 0, sizeof(UList));
	memset(FHwnd, 0, sizeof(FHwnd));
	CreateDirectory("Msgbox", NULL);

	HListMutex = CreateMutex(NULL, false, NULL);
	if(HListMutex == NULL){
		if(Debug)	printf("Fatal Error\n");
		goto out;
	}
	HFileMutex = CreateMutex(NULL, false, NULL);
	if(HFileMutex == NULL){
		if(Debug)	printf("Fatal Error\n");
		goto out;
	}
	
	UserCnt = 0;
	FILE* FUList = fopen("UserList.txt", "r");
	if(FUList != NULL){
		if(WaitForSingleObject(HListMutex, INFINITE) != WAIT_OBJECT_0){
			if(Debug)	printf("Fatal Error\n");
			goto out;
		}
		while(true){
			ret = fscanf(FUList, "%s %s", UList[UserCnt].Name, UList[UserCnt].Pwd);
			if(ret != 2)	break;
			UList[UserCnt].Mutex = CreateMutex(NULL, false, NULL);
			UserCnt++;
		}
		ReleaseMutex(HListMutex);
		fclose(FUList);
	}

	HServThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&Start, NULL, 0, NULL);

	char cmd[256];
	while(true){
		scanf("%s", cmd);
		if(strcmp(cmd, "EXIT") == 0){
			Running = false;
			closesocket(ServSock);
			WaitForSingleObject(HServThread, 10000);
			break;
		}
		if(strcmp(cmd, "LIST") == 0){
			if(WaitForSingleObject(HListMutex, INFINITE) == WAIT_OBJECT_0){
				printf("Sn\t\tID\t\tPassword\tStatus\n");
				for(i = 0; i < UserCnt; i++){
					printf("%d\t\t%s\t\t%s", i, UList[i].Name, UList[i].Pwd);
					if(UList[i].OnLine){
						printf("\t\tOnline\n");
					}
					else{
						printf("\t\tOffline\n");
					}
				}
				ReleaseMutex(HListMutex);
			}
			else{
				if(Debug)	printf("Error Read List\n");
			}		
		}
		if(strcmp(cmd, "IP") == 0){
			PrintIP();
		}
		if(strcmp(cmd, "HELP") == 0){
			printf("LIST: List all registered user");
			printf("EXIT: Stop service and terminate server process");
			printf("IP: Show server IP address");
			printf("HELP: Show instructions");
			printf("DEBUG: Show debug information");
			printf("DEBUGOFF: Hide debug information");
		}
		if(strcmp(cmd, "DEBUG") == 0){
			Debug = true;
			printf("Enter Debug Mode\n");
		}
		if(strcmp(cmd, "DEBUGOFF") == 0){
			Debug = false;
			printf("Quit Debug Mode\n");
		}
	}
   
out:;
	WSACleanup();
	system("pause");
	return 0;
}
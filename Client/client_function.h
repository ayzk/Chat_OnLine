
#ifndef _CLIENT_FUNCTION_H_
#define _CLIENT_FUNCTION_H_

#include<time.h>
#define buf_length 512
#define leng_of_user_name 8
#define leng_of_msg 128
#define leng_of_file_name 256
#define number_of_file 256
#define number_of_message 1024

//file state
#define FILE_NULL 0
#define FILE_ACC 1
#define FILE_REJ 2
#define FILE_UND 3
#define FILE_CLOSE 4
#define FILE_TRA 5
//FILE_TRA file transfer


#define historyfile "historymsg" 


struct User_list
{
	char name[leng_of_user_name];
	char flag;
};


//store message
struct msg
{
	time_t time;
	char from[leng_of_user_name];
	char to[leng_of_user_name];
	enum {READ,UNREAD} state;
	char data[leng_of_msg];
};




/////////////////////////////////////////////////////////////////////
/////////Functinos to notify user and write to disk//////////////////
/////////////////////////////////////////////////////////////////////

void PutToBackContent(char  str[buf_length]);

void PrintScreen();

void write_history(struct msg m);

/////////////////////////////////////////////////////////////////////////
////////////Functions for communication between local and server/////////
/////////////////////////////////////////////////////////////////////////

int init();

void *Mylisten(void * arg);

int SendMsg(int SocketFd,unsigned char RecSn, struct msg);

int SendOpenFile(int SocketFd, unsigned char RecSn, unsigned char FNameLen, char Fname[]);

int SendFileResponse(int SocketFd, int handle, char opt); //opt: 0-> ok; 1->reject

void *SendFileContent(void *arg);

int SendFileClose(int SocketFd);

int RecMsg(char Buf[]);

int RecFileHandle(char Buf[], int *handle);

int RecFileReq(char Buf[], int *handle, unsigned char *SenSn, unsigned char* FNameLen, char Fname[]);

int RecFileResponse(int SocketFd, char Buf[], int *handle, unsigned char *opt);

int RecFileContent(char Buf[]);

int RecFileClose(char Buf[],int *handle);

int RecUser(char Buf[], unsigned char *sn);

#endif

#include "client_function.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern pthread_mutex_t mutexWriteSocket;
extern pthread_mutex_t mutexWriteData;
extern pthread_mutex_t mutexWriteHistoryMsg;
extern pthread_mutex_t mutexPrintScreen;

extern char  content[60][buf_length];
extern int nPrint;
char back_content[4][buf_length];
char Back_buf[buf_length];
int nBack_print;

//user[i] is user who's Sn is i
extern struct User_list user[256];
extern char username[leng_of_user_name];


//store msg
extern int nMsg;
extern struct msg Msg[number_of_message];
//scan for state==UNREAD and read it(print)


//store the receive file table
extern int nFile;
extern char* FileName[number_of_file]; //initial <-null
extern int FileHandle[number_of_file];
extern int FileSeek[number_of_file];
extern int FileFrom[number_of_file];
extern int FileState[number_of_file];
// scan for FILE_UND, let user determine
// scan for FILE_CLOSE, let user know


//store the send file information
//given that user once can only send a file
//so there doesn't need a table like receice file table.
extern char Send_FileName[number_of_file];
extern int Send_FileHandle;
extern int Send_FileState;
//scan for FILE_ACC or FILE_REJ
//FILE_ACC: set it to FILE_TRA and transfer file
//FILE_REJ: set it to FILE_NULL


/////////////////////////////////////////////////////////////////////
/////////Functinos to notify user and write to disk//////////////////
/////////////////////////////////////////////////////////////////////
void PrintScreen()
{

  system("clear");

  int i;
  for (i=0;i<3;i++)
    printf("%s",back_content[i]);
  for(i=0;i<nPrint;i++)
    printf("%s",content[i]);
  
  fflush(stdout);

  return ;
}

void PutToBackContent(char  str[buf_length])
{
    if (nBack_print<3)
        memcpy(back_content[nBack_print++],str,buf_length);
    else
    {
        memcpy(back_content[0],back_content[1],buf_length);
        memcpy(back_content[1],back_content[2],buf_length);
        memcpy(back_content[2],str,buf_length); 
    }
}

void write_history(struct msg m)
{
    pthread_mutex_lock(&mutexWriteHistoryMsg);

    FILE *fp = fopen(historyfile,"ab");
    if (fp==NULL)
    {
        perror("write history open:");
        return ;
    }
    if(fwrite(&m,1,sizeof(struct msg),fp)<sizeof(struct msg)){
        perror("write history");
        exit(0);
    }
    fclose(fp);

    pthread_mutex_unlock(&mutexWriteHistoryMsg);

    return ;
}


/////////////////////////////////////////////////////////////////////////
////////////Functions for communication between local and server/////////
/////////////////////////////////////////////////////////////////////////

int init()
{
    memset(user,0,sizeof(user));

    nMsg=0;
    memset(Msg,0,sizeof(Msg));

    nFile=0;
    memset(FileName,0,sizeof(FileName));
    memset(FileHandle,0,sizeof(FileHandle));
    memset(FileFrom,0,sizeof(FileFrom));
    memset(FileState,0,sizeof(FileState));
    int i ;
    for(i=0;i<256;i++)
    {
        memset(user[i].name,0,sizeof(leng_of_user_name));
        user[i].flag=0;
    }
    nBack_print=0;
    for (i=0;i<3;i++)
        strcpy(back_content[i],"\n");

    memset(Send_FileName,0,sizeof(Send_FileName));
    Send_FileState=FILE_NULL;
    Send_FileHandle=-1;
    return 1;
}

void *Mylisten(void *arg)
{
    int SocketFd=(int) arg;
    char Buf[buf_length];
    char buf1[buf_length];

    int handle;
    unsigned char len,sn,opt;
    while (1)
    {
        int nRecByte;
        memset(Buf,0,buf_length);

        if ((nRecByte=recv(SocketFd,Buf,2,0))<0)
        {
            perror("cann't receive data From server");
            break;
        };

//        printf("The first two bit receive: %d %d\n",Buf[0],Buf[1]);

        if (Buf[0]==2)
        {
            if (Buf[1]==0)
            {
                recv(SocketFd,Buf+2,2,0);
                len=Buf[3];
                recv(SocketFd,Buf+4,len,0);
                RecMsg(Buf);
            }
            else if (Buf[1]==2)
            {
                recv(SocketFd,Buf+2,4,0);
                if (RecFileHandle(Buf,&handle)==-1)
                    Send_FileState=FILE_REJ;
                //printf("file handle is %d\n",handle);
            }
            else if (Buf[1]==3)
            {
                recv(SocketFd,Buf+2,6,0);
                len=Buf[7];
                recv(SocketFd,Buf+8,len,0);
                RecFileReq(Buf,&handle,&sn,&len,buf1);
            }
            else if (Buf[1]==4)
            {
                recv(SocketFd,Buf+2,5,0);
                RecFileResponse(SocketFd,Buf,&handle,&opt);
            }
            else if (Buf[1]==5)
            {
                recv(SocketFd,Buf+2,5,0);
                len=Buf[6];
                recv(SocketFd,Buf+7,len,0);
                RecFileContent(Buf);
            }
            else if (Buf[1]==6)
            {
                recv(SocketFd,Buf+2,4,0);
                RecFileClose(Buf,&handle);
            }
        }
        else if (Buf[0]==3)
        {
            if (Buf[1]==0)
            {
                recv(SocketFd,Buf+2,2,0);
                len=Buf[3];
                recv(SocketFd,Buf+4,len+1,0);
                RecUser(Buf,&sn);

            };
        };

    };
};



int SendMsg(int SocketFd, unsigned char RecSn,struct msg msg1)
{
    //printf("RecSn:%d; Msglen:%d; Msg:%s\n",RecSn,strlen(msg1.data),msg1.data);

    char Buf[buf_length];
    memset(Buf,0,sizeof(Buf));

    Buf[0]=2;
    Buf[1]=0;
    Buf[2]=RecSn;
    Buf[3]=sizeof(struct msg);
    memcpy(Buf+4,&msg1,sizeof(struct msg));

    int nByte;
    pthread_mutex_lock(&mutexWriteSocket);
    if ((nByte = send( SocketFd, Buf, 4+sizeof(struct msg), 0 )) <0 )
    {
        perror("SendMsg");
        exit(1);
    }
    pthread_mutex_unlock(&mutexWriteSocket);

    return nByte;
};


int SendOpenFile(int SocketFd, unsigned char RecSn, unsigned char FNameLen, char Fname[])
{
    memcpy(Send_FileName,Fname,FNameLen);
    Send_FileState=FILE_UND;
    Send_FileHandle=-1;

    char Buf[buf_length];
    memset(Buf,0,sizeof(Buf));

    Buf[0]=2;
    Buf[1]=1;
    Buf[2]=RecSn;
    Buf[3]=FNameLen;
    memcpy(Buf+4,Fname,FNameLen);

    int nByte;
    pthread_mutex_lock(&mutexWriteSocket);
    if ((nByte = send(SocketFd,Buf,4+FNameLen,0)) <0 )
    {
        perror("SendOpenFile");
        exit(1);
    }
    pthread_mutex_unlock(&mutexWriteSocket);
    return nByte;
}

int SendFileResponse(int SocketFd, int handle, char opt) //opt: 0-> ok; 1->reject
{
    char Buf[buf_length];
    memset(Buf,0,sizeof(Buf));

    Buf[0]=2;
    Buf[1]=4;
    memcpy(Buf+2,&handle,4);
    memcpy(Buf+6,&opt,1);

    int nByte;
    pthread_mutex_lock(&mutexWriteSocket);
    if ((nByte = send(SocketFd,Buf,7,0)) <0 )
    {
        perror("SendFileResponse");
        exit(1);
    }
    pthread_mutex_unlock(&mutexWriteSocket);
    return nByte;
}


void* SendFileContent(void * arg)
{
    int SocketFd=(int)arg;

    char Buf[buf_length];

    FILE *fd1=fopen(Send_FileName,"r+b");

   // printf("read file: %s\n",Send_FileName);
   // printf("file Handle is : %d \n",Send_FileHandle);

    while (!feof(fd1) && Send_FileState!=FILE_NULL)
    {
        pthread_mutex_lock(&mutexWriteSocket);
        unsigned char nByte,nByte2;

        memset(Buf,0,buf_length);

        Buf[0]=2;
        Buf[1]=5;
        memcpy(Buf+2,&Send_FileHandle,4);
        if ((nByte=fread(Buf+7,1,50,fd1))<=0)
            continue;

        Buf[6]=nByte;

     /*   int i;
        for (i=0;i<7+nByte;i++)
            if (i<7)
                printf("%d ",Buf[i]);
            else
                printf("%c ",Buf[i]);
        printf("\n");
*/
        if ((nByte2 = send(SocketFd, Buf, 7+nByte,0)) <0 )
        {
            perror("SendFileContent");
            exit(1);
        }
        if (nByte2!=7+nByte)
        {
            printf("send data error\n");
            return ;
        }
        pthread_mutex_unlock(&mutexWriteSocket);
    }
    fclose(fd1);

    SendFileClose(SocketFd);
};


int SendFileClose(int SocketFd)
{

    char Buf[buf_length];
    memset(Buf,0,sizeof(Buf));

    Buf[0]=2;
    Buf[1]=6;
    memcpy(Buf+2,&Send_FileHandle,4);

    pthread_mutex_lock(&mutexWriteSocket);
    int nByte;

    if ((nByte = send(SocketFd, Buf, 6,0 )) <0 )
    {
        perror("SendFileClose");
        exit(1);
    }
    pthread_mutex_unlock(&mutexWriteSocket);


    pthread_mutex_lock(&mutexWriteData);
    Send_FileState=FILE_NULL;
    Send_FileHandle=-1;
    memset(Send_FileName,0,sizeof(Send_FileName));
    pthread_mutex_unlock(&mutexWriteData);


    sprintf(Back_buf,"==Me==:Finish send file!\n");
    PutToBackContent(Back_buf);
    PrintScreen();

  //  printf("sendfileclose\n");
    return nByte;
};


int RecMsg(char Buf[])
{

    pthread_mutex_lock(&mutexWriteData);
    memcpy(&Msg[nMsg],Buf+4,sizeof(struct msg));

    sprintf(Back_buf,"==User %s==:%s\n",Msg[nMsg].from,Msg[nMsg].data);
    PutToBackContent(Back_buf);
    PrintScreen();
    write_history(Msg[nMsg]);

    nMsg++;
    pthread_mutex_unlock(&mutexWriteData);
}

int RecFileHandle(char Buf[], int *handle)
{
    memcpy(handle,Buf+2,4);

    if (*handle==0xFFFFFFFF)
    {
        Send_FileState=FILE_REJ;
        return -1;

    }
    else
    {
        Send_FileHandle=*handle;
        return *handle;
    };
}

int RecFileReq(char Buf[], int *handle, unsigned char *SenSn, unsigned char* FNameLen, char Fname[])
{

    memcpy(handle,Buf+2,4);
    memcpy(SenSn,Buf+6,1);
    memcpy(FNameLen,Buf+7,1);
    memcpy(Fname,Buf+8,*FNameLen);

    pthread_mutex_lock(&mutexWriteData);

    int i;
    for (i=0;i<nFile;i++)
        if (FileState[i]==FILE_NULL)
            break;
    if (i==nFile)
        nFile++;
    if (FileName[i]==NULL)
        FileName[i]=malloc(leng_of_file_name);

    memcpy(FileName[i],Fname,*FNameLen);
    FileFrom[i]=*SenSn;
    FileState[i]=FILE_UND;
    FileHandle[i]=*handle;


    sprintf(Back_buf,"==server==:%s want to send you File:%s\n",user[FileFrom[i]].name,FileName[i]);
    PutToBackContent(Back_buf);
    PrintScreen();

    pthread_mutex_unlock(&mutexWriteData);
};

int RecFileResponse(int SocketFd, char Buf[], int *handle, unsigned char *opt)
{
    memcpy(handle,Buf+2,4);
    memcpy(opt,Buf+6,1);

    pthread_mutex_lock(&mutexWriteData);
    Send_FileState=(*opt==0)? FILE_ACC : FILE_REJ;
    
    if(Send_FileState==FILE_ACC)
    {
        FILE *file =fdopen(SocketFd,"w+");
        fflush(file);

        sprintf(Back_buf,"==server==:Receiver agree, begin to send file!\n");
        PutToBackContent(Back_buf);
        PrintScreen();

        pthread_t tSendFile;
        pthread_create(&tSendFile,NULL,SendFileContent,(void *)SocketFd);
        Send_FileState = FILE_TRA;
    }
    else if (Send_FileState==FILE_REJ)
    {
        sprintf(Back_buf,"==server==:Receiver reject to receive the file\n");
        PutToBackContent(Back_buf);
        PrintScreen();

        Send_FileState = FILE_NULL;
    }

    pthread_mutex_unlock(&mutexWriteData);

    return 0;
}

int RecFileContent(char Buf[])
{
    int handle;
    unsigned char len;
    memcpy(&handle,Buf+2,4);
    memcpy(&len,Buf+6,1);

    int i;
  //  printf("%d\n",handle);
    for (i=0;i<nFile;i++)
    {
 //      printf("%d-th file:%s, handle:%d, state %d\n",i,FileName[i],FileHandle[i],FileState[i]);
       if (FileState[i]==FILE_ACC && FileHandle[i]==handle)
            break;
    };
    FILE *fd;
    
    if (i==nFile)
    {
        printf("Wrong handle for receive file\n");
        return 1;
    }

  //  printf("file name :%s, handle:%d\n",FileName[i],FileHandle[i]);
    if ((fd=fopen(FileName[i],"a"))==NULL)
    {
        perror("RecFileContent_OpenFIle");
        return 1;
    }


    int nByte;
    nByte=fwrite(Buf+7,1,len,fd);
    if (nByte!=len)
    {
        printf("write file error\n");
        return 1;
    }
    fclose(fd);
    return nByte;
};

int RecFileClose(char Buf[],int *handle)
{
    memcpy(handle,Buf+2,4);
    if (*handle==Send_FileHandle)
    {

        pthread_mutex_lock(&mutexWriteData);
        Send_FileState=FILE_NULL;
        pthread_mutex_unlock(&mutexWriteData);
        return 1;
    }

    int i;
    for (i=0;i<nFile;i++)
        if (FileState[i]==FILE_ACC && FileHandle[i]==*handle)
            break;
    if (FileHandle[i]!=*handle)
        return -1;

    sprintf(Back_buf,"==server==:Finish receive file:%s\n",FileName[i]);
    PutToBackContent(Back_buf);
    PrintScreen();
    
    pthread_mutex_lock(&mutexWriteData);

    FileHandle[i]=0;
    FileFrom[i]=0;
    FileState[i]=FILE_NULL;
    pthread_mutex_unlock(&mutexWriteData);
    
    return 0;
};

int RecUser(char Buf[], unsigned char *sn)
{

    *sn=Buf[2];
    unsigned char idlen=Buf[3];

    pthread_mutex_lock(&mutexWriteData);
    memcpy(user[*sn].name,Buf+4,idlen);
    user[*sn].flag=Buf[4+idlen];
//    printf("user name %s with sn %d is %d\n",user[*sn].name,*sn,user[*sn].flag);
    pthread_mutex_unlock(&mutexWriteData);

    return *sn;
}

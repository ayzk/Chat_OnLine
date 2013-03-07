#include "client_function.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int getch(){
    struct termios oldt, newt;
    int ch;
    tcgetattr( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}


pthread_mutex_t mutexWriteSocket;
pthread_mutex_t mutexWriteData;
pthread_mutex_t mutexWriteHistoryMsg;
pthread_mutex_t mutexPrintScreen;


//user[i] is user who's Sn is i
struct User_list user[256];
char username[8];


//store msg
int nMsg;
struct msg Msg[number_of_message];
//scan for state==UNREAD and read it(print)


//store the receive file table
int nFile;
char* FileName[number_of_file]; //initial <-null
int FileHandle[number_of_file];
int FileSeek[number_of_file];
int FileFrom[number_of_file]; 
int FileState[number_of_file]; 
// scan for FILE_UND, let user determine
// scan for FILE_CLOSE, let user know
  

//store the send file information
//given that user once can only send a file
//so there doesn't need a table like receice file table.
char Send_FileName[number_of_file];
int Send_FileHandle; 
int Send_FileState; 
//scan for FILE_ACC or FILE_REJ 
//FILE_ACC: set it to FILE_TRA and transfer file
//FILE_REJ: set it to FILE_NULL

char  content[60][buf_length];
int nPrint;



void put_to_buffer(char* str){
    
    int i ;
    if(nPrint==60){
        for(i=0;i<nPrint-1;i++)
            memcpy(content[i],content[i+1],buf_length);
         memset(content[nPrint],0,sizeof(buf_length));
         memcpy(content[nPrint-1],str,strlen(str));
    }else if(nPrint<60){
        memset(content[nPrint],0,sizeof(buf_length));
        memcpy(content[nPrint],str,strlen(str)+1);
        nPrint++;
    }

    return ;
}

void print_UI(){
    
  nPrint = 0;
    put_to_buffer("1. Message Mode\n\n");
    put_to_buffer("2. File Mode\n\n");
    put_to_buffer("3. User list\n\n");
    put_to_buffer("4. History Message\n\n");
    put_to_buffer("5. Help\n\n");
    put_to_buffer("6. Exit \n\n");
    put_to_buffer("please enter which one do you want to do by number :");

    PrintScreen();
    return ;
}

int go_register(int sockfd){

    char password[8];

    char tmp[buf_length];
    char str[buf_length];

    system("clear");

    printf("Please enter the username to register (length should be at most 8):");
    char tp;
    if ((tp=getchar())!='\n')
      ungetc(tp,stdin);
    scanf("%s",username);

    printf(str,"Please enter the username to register (length should be at most 8): %s\n",username);

    printf("Please enter your password(length should be at most 8):");
 
    if ((tp=getchar())!='\n')
       ungetc(tp,stdin);
    gets(password);



    tmp[0] = 1;
    tmp[1] = 0;
    unsigned char len1=strlen(username)+1;
    memcpy(tmp+2,&len1,1);
    memcpy(tmp+3,username,len1);

    unsigned char len2=strlen(password)+1;
    memcpy(tmp+len1+3,&len2,1);
    memcpy(tmp+len1+4,password,len2);

    if(send(sockfd,tmp,4+len1+len2,0)<0){
        perror("regi fail\n");

        printf("can not send to server!\n");
        printf("please try again\n");

        return 0;
    }

    char status[3];
    if(recv(sockfd,status,3,0)<0){
        perror("fail to read from server!\n");
        return 0;
    }


    if(status[0]==1&&status[1]==1){
        if(status[2]==0){
            printf("Registration success!\n");
            getch();
            return 1;
        }else if(status[2]==1){
            printf("fail to register \n");
            printf("please try another username\n");
            getch();
            return 0;
        }else{
            perror("wrong reg state\n");
            printf("system occured error\n");
            printf("please try again\n");
            getch();
            return 0;
        }
    }else{
        perror("wrong registration signal\n");
        printf("system occured error\n");
        printf("please try again\n");
        getch();
 
        return 0;
    }
    return 0;
}





void read_history(){
    struct msg buff;
    FILE * fp=fopen(historyfile,"ab+");
    if (fp==NULL)
    {
      perror("cannot open");
      return ;
    }

    int i,n=0;
    char ans[buf_length];
    char str[buf_length];

   
    while (1)
    {
      nPrint = 0;
  
      for (i=0;i<8 && !feof(fp);i++,n++)
      {
        if (fread(&buff,1,sizeof(struct msg),fp)<sizeof(struct msg))
          break;
        memset(str,0,sizeof(buf_length));
        sprintf(str,"No.%d: %s" ,n,ctime(&buff.time));
        str[strlen(str)-1]=0;
        put_to_buffer(str);
        sprintf(str,", from %s to %s, ",buff.from,buff.to);
        put_to_buffer(str);
        sprintf(str,"message :%s\n",buff.data);
        put_to_buffer(str);
        PrintScreen();
      };
      if (feof(fp))
        break;
      put_to_buffer("Press q to quit or any other key to see more history message\n");

      PrintScreen();
      
      //getch();
      if (getch()=='q')
        return ;
      //scanf(" %s",ans);
      //if (!strcmp(ans,"-q"))
        //return;
    };
      put_to_buffer("Press any key to exit\n");
      PrintScreen();
      getch();
 
    return;
}

int login(char* usernamed , char* password,int sockfd){

    char tmp[buf_length];
    int n;
    tmp[0]= 1;
    tmp[1]= 2;
    unsigned char len1=strlen(usernamed)+1;
    memcpy(tmp+2,&len1,1);
    memcpy(tmp+3,username,len1);

    unsigned char len2=strlen(password)+1;
    memcpy(tmp+len1+3,&len2,1);
    memcpy(tmp+len1+4,password,len2);

    //put_to_buffer("begin to log send\n");
    //PrintScreen();

//    pthread_mutex_lock(&mutexWriteSocket);

    int nByte;
    if((nByte=send(sockfd,tmp,2+2+len1+len2,0))<0){
        perror("can not send to server\n");
        exit(0);
    }
//     pthread_mutex_unlock(&mutexWriteSocket);

 //   char screen[buf_length];
    //sprintf(screen,"send %d\n",nByte);
    //put_to_buffer(screen);
    //PrintScreen();

    char status[3];
    if(recv(sockfd,status,3,0)<0){
        perror("can not read from server\n");
        exit(0);
    }
    if(status[0]==1&&status[1]==3){
        if(status[2]==0){
            printf("Login success\n");
            return 1;
        }else if(status[2]==1){
            printf("wrong username or password!\n");
            return 0;
        }else{
            printf("system occured error\n");
            printf("please try again\n");
            return 0;
        }

    }else{
        printf("system occured error\n");
        printf("please try again\n");

        return 0;
    }
    return 0;
}

int logout(int sockfd){

    char tmp[buf_length];
    int n;
    char ans;
    tmp[0]= 1;
    tmp[1]= 4;

    printf("Do you really want to logout (y/n)? :");
    scanf(" %c",&ans);
    if(ans=='y'){
        if(write(sockfd,tmp,strlen(tmp))<0){
            perror("can not send to server\n");
            exit(0);
        }
        return 1;
    }else
        return 0;
}


void chatmode(int sockfd,char* recver,char sn){

  char tmp[buf_length];
  char str[buf_length];
  char ans;
  int n;
  nPrint--;
  PrintScreen();
  while(1){
    
    put_to_buffer("please enter the message you want to send(or enter -q to leave):");
    PrintScreen();

    char tp;
    if ((tp=getchar())!='\n')
      ungetc(tp,stdin);    
    gets(tmp);
    
    sprintf(str,"\nplease enter the message you want to send(or enter -q to leave): %s",tmp);
    nPrint--;
    put_to_buffer(str);
    PrintScreen();

   
    if(strcmp(tmp,"-q")==0){
      system("clear");
      nPrint=0;
      break;
    }

    n = strlen(tmp)+1;
    time_t t = time(NULL);
    struct msg m;
    m.time = t ;
    m.state = READ;
    memcpy(m.from,username,strlen(username)+1);
    memcpy(m.to,user[sn].name,strlen(user[sn].name)+1);
    memcpy(m.data,tmp,strlen(tmp)+1);
    
    if(SendMsg(sockfd,sn,m)<0){
        perror("can't chat\n");
        exit(0);
      }    
    
      sprintf(str,"to %s : %s\n" ,recver,tmp);
      nPrint = nPrint -1 ;
      put_to_buffer(str);
      PrintScreen();

      
      write_history(m);

  }

  return ;
}


void chat(int sockfd){

    char tmp[buf_length]="";
    int n;
    char recver[leng_of_user_name];
    char *recver2;
    char sn ;
    int i;
    char str[buf_length];
    nPrint=0;
    char ans;
    system("clear");

    pthread_mutex_lock(&mutexWriteData);
    for(i=0;i<256;i++)
        if(strcmp(user[i].name,"")!=0){
          if(user[i].flag){
            sprintf(str,"ID:%d, Name:%s, status:online\n",i,user[i].name);
            put_to_buffer(str);
          }else if(!user[i].flag){
            sprintf(str,"ID:%d, Name:%s, status:online\n",i,user[i].name);
             put_to_buffer(str); 
          } 
        }
    pthread_mutex_unlock(&mutexWriteData);
    
   

    while(1){    
      
      PrintScreen();

      put_to_buffer("please enter the name that you want to send the message(or -q to quit): ");
      PrintScreen();
      
      char tp;
      if ((tp=getchar())!='\n')
        ungetc(tp,stdin);
      gets(recver);

      if(strcmp(recver,"-q")==0)
        break;

      
      nPrint--;
      put_to_buffer("please enter the message you want to send:");
      PrintScreen();

      fflush(stdin);


      if ((tp=getchar())!='\n')
        ungetc(tp,stdin);
      gets(tmp);
      
      sprintf(str,"please enter the message you want to send: %s\n",tmp);
      nPrint--;
      put_to_buffer(str);
      PrintScreen();


      if (strstr(recver," ")!=NULL)
      {
          recver2=strtok(recver," ");
          while (recver2!=NULL)
          {
            n = strlen(tmp)+1;
      
            for(i=0;i<256;i++){
              if(strcmp(user[i].name,recver2)==0){
                  memcpy(&sn,&i,1);
                  break;
              }
            }
            
            time_t t = time(NULL);
            struct msg m;
            memset(&m,0,sizeof(struct msg));

            m.time = t ;
            m.state = READ;
            memcpy(m.from,username,strlen(username)+1);
            memcpy(m.to,user[sn].name,strlen(user[sn].name)+1);
            memcpy(m.data,tmp,n);
            if(SendMsg(sockfd,sn,m)<0){
              perror("can't chat\n");
              exit(0);
            }    
            
            write_history(m);
            sprintf(str,"to %s : %s\n" ,recver2,tmp);
            //nPrint = nPrint -1 ;
            put_to_buffer(str);
            PrintScreen();

            recver2=strtok(NULL," ");
          }
      }
      else
      {
        n = strlen(tmp)+1;
      
        for(i=0;i<256;i++){
          if(strcmp(user[i].name,recver)==0){
              memcpy(&sn,&i,1);
              break;
          }
        }
        
        time_t t = time(NULL);
        struct msg m;
        memset(&m,0,sizeof(struct msg));

        m.time = t ;
        m.state = READ;
        memcpy(m.from,username,strlen(username)+1);
        memcpy(m.to,user[sn].name,strlen(user[sn].name)+1);
        memcpy(m.data,tmp,n);
        if(SendMsg(sockfd,sn,m)<0){
          perror("can't chat\n");
          exit(0);
        }    
        
        write_history(m);
        sprintf(str,"to %s : %s\n" ,recver,tmp);
        nPrint = nPrint -1 ;
        put_to_buffer(str);
        PrintScreen();

        
        
        sprintf(str,"if you want to enter the chat mode with %s (y/n) ?:",recver);
        put_to_buffer(str);
        PrintScreen();
        scanf(" %c",&ans);
        sprintf(str,"if you want to enter the chat mode with %s (y/n) ? : %c\n" ,recver,ans);
        nPrint--;
        put_to_buffer(str);
        PrintScreen();

        if(ans=='y')
          chatmode(sockfd,recver,sn);
        else{
          nPrint--;
          PrintScreen();
        }
      }
    }
    return;
}


void print_list(){
    
    int i;
    char str[buf_length];
    char ans;
    nPrint=0;
      pthread_mutex_lock(&mutexWriteData);
  
    for(i=0;i<256;i++)
        if(strcmp(user[i].name,"")!=0){
          if(user[i].flag){
            sprintf(str,"ID:%d, Name:%s, status:online\n",i,user[i].name);
            put_to_buffer(str);
          }else if(!user[i].flag){
            sprintf(str,"ID:%d, Name:%s, status:online\n",i,user[i].name);
             put_to_buffer(str); 
          } 
        }
          pthread_mutex_unlock(&mutexWriteData);
  

        put_to_buffer("Press any key to quit \n");
        PrintScreen();
        getch();
        getch();

        //scanf(" %c",&ans);
    
        return;
}

void filestate_scan(int sockfd){

    int i;
    char ans;
    char Buf[buf_length];
    pthread_mutex_lock(&mutexWriteData);
  
    for(i=0;i<nFile;i++){
        if(FileState[i]== FILE_UND){
            sprintf(Buf,"User:%s want to send you file :%s\n",user[FileFrom[i]].name,FileName[i]);
            put_to_buffer(Buf);
            sprintf(Buf,"please enter if you accept (y/n)?:\n");
            put_to_buffer(Buf);
            PrintScreen();
            
            scanf(" %c",&ans);
            if(ans=='y'){
                sprintf(Buf,"You agree to receive file\n");
                put_to_buffer(Buf);
                PrintScreen();

                FileState[i] = FILE_ACC;
                SendFileResponse(sockfd,FileHandle[i],0);
              //  printf(sockfd,FileHandle[i],0);
            }else{
                sprintf(Buf,"You reject to receive\n");
                put_to_buffer(Buf);
                PrintScreen();


                FileState[i] = FILE_REJ;
                SendFileResponse(sockfd,FileHandle[i],1);
            }
        }
    }

    pthread_mutex_unlock(&mutexWriteData);

}



void sendfile_req(int sockfd){


    unsigned char sn;
    char filename[buf_length]={0};
    char str[buf_length];
    char recver[8];
    nPrint = 0;
    int i;

    system("clear");

    pthread_mutex_lock(&mutexWriteData);
  
    for(i=0;i<256;i++)
      if(strcmp(user[i].name,"")!=0){
        if(user[i].flag){
          sprintf(str,"user : %s  status : online \n",user[i].name);
          put_to_buffer(str);
        }else if(!user[i].flag){
          sprintf(str,"user : %s  status : offline \n",user[i].name);
          put_to_buffer(str); 
        } 
      }

    pthread_mutex_unlock(&mutexWriteData);
  
    PrintScreen();

    filestate_scan(sockfd);

    put_to_buffer("please enter the person you want to send (press -q to exit):");
    PrintScreen();
    scanf(" %s",recver);
    if (!strncmp(recver,"-q",2))
      return ;

    
    sprintf(str,"please enter the person you want to send: %s\n",recver);
    nPrint--;
    put_to_buffer(str);
    PrintScreen();
    
    put_to_buffer("enter the file you want to send:");
    PrintScreen();
    scanf(" %s",filename);
    sprintf(str,"enter the file you want to send: %s\n",filename);
    nPrint--;
    put_to_buffer(str);



    int len = strlen(filename)+1;
    
    for(i=0;i<256;i++)
        if(strcmp(recver,user[i].name)==0){
            memcpy(&sn,&i,1);
            break;
        }

    sprintf(str,"begin to send file to user %s\n",recver);
    put_to_buffer(str);
    PrintScreen();


    SendOpenFile(sockfd,sn,len,filename);
    memcpy(Send_FileName, filename, strlen(filename)+1);
    
    put_to_buffer("please press any key to leave\n");
    getch();
    getch();

    //scanf(" %c",&sn);
    return ;
}




void help(){

  
   char n;
   char buf[buf_length];
   PrintScreen();
   getch();
    
   while(1)
   {

    print_UI();

    put_to_buffer("\nplease enter the number of the command\n");
    put_to_buffer("that you want to know how to use or press q to quit :\n");
    PrintScreen();
    n=getch();

    sprintf(buf,"%c:\n",n);
    put_to_buffer(buf);
 
    if(n=='q')
      break;
    
    if(n=='1'){
    // chatd
      put_to_buffer("used to send message to other user and allowed to send a message to many people in one time\n");
      put_to_buffer("chat mode means to send message to a specify user and don't need to answer who you want talk everytime\n");
    
    }else if(n=='2'){
    //sendfile
      put_to_buffer("used to send a file to other user or receive the file that other sends to you\n");
      put_to_buffer("Send File :you need to enter the receiver's name and the filename that you want to send \n");
      put_to_buffer("after the receiver accept your sending request, file transmission will get start until it completes\n\n");
      put_to_buffer("Receive File : you need to answer whether or not you want to accept the file\n");
      put_to_buffer("if you accept it then the file transfer will start or you can just reject it\n");
    }else if(n=='3'){
    //userlist
      put_to_buffer("used to see all users' status\n");
    }else if(n=='4'){
      //historynehi
      put_to_buffer("used to see the history message\n");
    }else if(n=='5'){
      //help
      put_to_buffer("used to know how commands work\n");
    }else if(n=='6'){
      //logout
      put_to_buffer("used to exit \n");
    }else if(n=='q'){
      return ;
    }else{
      put_to_buffer("no such number allowed\n");
    }
    put_to_buffer("press any key to continue\n");
    PrintScreen();

    getch();
    
  }
    return ;
}






int main(int argc , char* argv[]){


  init();

  pthread_mutex_init(&mutexWriteSocket,NULL);
  pthread_mutex_init(&mutexWriteData,NULL);
  pthread_mutex_init(&mutexWriteHistoryMsg,NULL);

    struct sockaddr_in srv;

  int sock = socket(AF_INET,SOCK_STREAM,0);
  if(sock < 0){
    perror("fail to creat socket\n");
    return 0;
  }

    FILE *fp=fopen("config.txt","r");
    if (fp==NULL)
    {
      printf("Please put the server ip address in config.txt\n");
      printf("The format is one line with ip such as 1.1.1.1\n");
      return;
    }
    char server_ip_address[buf_length];
    fgets(server_ip_address,buf_length,fp);
    fclose(fp);

    srv.sin_family = AF_INET;
        srv.sin_port = htons(7777);
        srv.sin_addr.s_addr =inet_addr(server_ip_address);

    printf("begin to connect!\n");
    if((connect(sock,(const struct sockaddr*)&srv,sizeof(srv)))==-1){
    perror("can not connect to server \n");
    return 0 ;
  }
    nPrint = 0;
    char password[8];
    printf("Enter your username (use -new to register) :");
    scanf(" %s",username);
    if((strcmp("-new",username))==0){
        while(1){
            if(go_register(sock))
                break;
        }
        while(1){
             printf("please enter your username: ");
             scanf(" %s",username);
             printf("please enter your password: ");
             scanf(" %s",password);
             if(login(username,password,sock))
                break;
        }
    }else{
        printf("please enter your password :");
        scanf(" %s",password);
//        printf("password:%s\n",password);
        while(1){
            if(login(username,password,sock))
                break;
            else{
                printf("please enter your username: ");
                scanf(" %s",username);
                printf("please enter your password: ");
                scanf(" %s",password);
            }
        }
    }
    
    pthread_t tlisten;
    pthread_create(&tlisten,NULL,Mylisten,(void *)sock);

    int n;
    //FD_ZERO(&rfds);
    //FD_SET(0, &rfds);
    //tv.tv_sec =0 ;
    //tv.tv_usec = 100;
    //retval = select(1, &rfds, NULL, NULL, &tv);
    while(1){
      
      //retval = select(1, &rfds, NULL, NULL, &tv);
      print_UI();
      
      //if(retval==-1){
        //perror("select");
      //}else if(retval){
        
        scanf(" %d",&n);
        //printf("%d",n);
        if(n == 1)
         chat(sock);
        else if(n == 2)
           sendfile_req(sock);
        else if(n == 3)
         print_list();
        else if(n == 4)
         read_history();
        else if(n == 5)
         help();
        else if(n == 6){
         if(logout(sock))
            break;                      
        }else{
         printf("Wrong number\n");
        }
    }
    pthread_mutex_destroy(&mutexWriteSocket);
    pthread_mutex_destroy(&mutexWriteData);
    pthread_mutex_init(&mutexWriteHistoryMsg,NULL);

    system("clear");
    return 0;

}

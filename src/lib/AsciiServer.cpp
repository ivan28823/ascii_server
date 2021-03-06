/**
 * Author: Ivan Moreno
 * December 2018
 * License - https://www.gnu.org/licenses/gpl-3.0.en.html
 */

#include "AsciiServer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

/**
 * Function for clean child process from sigaction()
 */
static void cleanChildProcess(int signal_number){
  wait (&signal_number);
}

AsciiServer::AsciiServer(){
  port = 8080;
  memset(&serverAddr,'\0',sizeof(serverAddr));
  sockFd = clientFd = 0;
  responseFunct = NULL;
  buffer = new char[BUFF_SER_LEN];
  bzero(buffer,BUFF_SER_LEN);
}
AsciiServer::AsciiServer(int p){
  port = p;
  memset(&serverAddr,'\0',sizeof(serverAddr));
  //initilize struct from server
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  sockFd = clientFd = 0;
  responseFunct = NULL;
  buffer = new char[BUFF_SER_LEN];
  bzero(buffer,BUFF_SER_LEN);
}
AsciiServer::~AsciiServer(){
  if(buffer)
    delete[] buffer;
  if(sockFd > 0)
    close(sockFd);
  if(clientFd > 0)
    close(clientFd);
}
void AsciiServer::setServerStruct(){
  memset(&serverAddr,'\0',sizeof(serverAddr));
  //initilize struct from server
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr = INADDR_ANY;
}
int AsciiServer::initServer(){
  sockFd = socket(AF_INET,SOCK_STREAM,0);
  if(sockFd < 0){
    printf("[-] Error in Connection\n");
    return 1;
  }
#ifdef SERVER_DBG
  printf("[+] Server socket is created\n");
#endif //SERVER_DBG
  
  int opt = 1;
  if (setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))){
		printf("[-] Error sock option\n");
		return 2;
	}
  
  if(bind(sockFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
    printf("[-] Error binding\n");
    return 3;
  }
#ifdef SERVER_DBG
  printf("[+] Binding port %d\n", port);
#endif //SERVER_DBG
  return 0;
}
void AsciiServer::startServer(){
  struct sigaction sigchld_action;
  memset(&sigchld_action,0,sizeof(sigchld_action));
  sigchld_action.sa_handler = &cleanChildProcess;
  sigaction(SIGCHLD, &sigchld_action, NULL);

  socklen_t addr_size = sizeof(clientAddr);  // size
  
  if(listen(sockFd, 10) < 0){
    printf("[-] Error in binding.\n");
  }

#ifdef SERVER_DBG
    printf("[+] Listening....\n");
#endif //SERVER_DBG

  for(;;){
    clientFd = accept(sockFd,(struct sockaddr *)&clientAddr,&addr_size);
    if(clientFd == -1 ){
      if (errno == EINTR){
	      /* The call was interrupted by a signal.  Try again.  */
	      continue;
      }else{
	      /* Something else were wrong.  */
        printf("[-] Error in accept\n");
        break;
      }
    }
    
#ifdef SERVER_DBG
      printf("[+] Connection accepted from %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
#endif //SERVER_DBG

      pid_t chiltPID = fork();
      if(chiltPID == 0){
        /*Child process*/
        close (sockFd);
        handleConnection();
        close(clientFd);
        exit (0);
      }else if(chiltPID > 0){
        /*Parent process*/
        close(clientFd);
      }else{
        printf("[-] Error in fork()\n");
      }
  }
}
void AsciiServer::closeServer(){
  close(sockFd);
  close(clientFd);
}
void AsciiServer::setFunctions(char * (**ptr)(char *buff)){
  responseFunct = ptr;
}
void AsciiServer::setPort(int p){
  port = p;
  setServerStruct();
}
void AsciiServer::setComands(const char ** ptrArr,int numOfCmd){
  comandsArray = (char **)ptrArr;
  numOfComands = numOfCmd;
}
void AsciiServer::setComands(const char ** ptrArr){
  comandsArray = (char **)ptrArr;
  numOfComands = 0;
  for (char ** aux = (char **)ptrArr; *aux; aux++, numOfComands++);
}
void AsciiServer::handleConnection(){
  int index,size_buff;
  for(;;){
    size_buff = recv(clientFd,buffer,BUFF_SER_LEN,0);

    index = -1;

    for (int i = 0; i < numOfComands ;i++)
      if (strncmp(buffer,comandsArray[i],strlen(comandsArray[i])) == 0) {
        index = i;
        break;
      }

    if (index == numOfComands - 1) {
      break;
    } else if (index >= 0) {
      char * auxbuff = new char[1024];
      strncpy(auxbuff,buffer,size_buff);
      //*(auxbuff + size_buff) = NULL; 
      sprintf(buffer,"%s\n",(responseFunct[index + 1])(auxbuff));
      send(clientFd,buffer,strlen(buffer),0);
      delete []auxbuff;
    } else {
      char *auxUnknow = new char[1024];
      strncpy(auxUnknow,buffer,size_buff);
      //*(auxUnknow + size_buff ) = NULL;
      sprintf(buffer,"%s\n",(responseFunct[0])(auxUnknow));
      send(clientFd,buffer,strlen(buffer),0);
      delete []auxUnknow;
    }
    bzero(buffer,BUFF_SER_LEN);
  }
}
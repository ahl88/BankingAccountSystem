#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
int _socket = -1;

void *_input(void * b)
{
	int a,i;

	for(;;){
	  char c[256];
	  //printf("client> ");
		memset(c,'\0',256);
		fgets(c, 256, stdin);
	  for(i = 0; i < 256; i++){
	      if(c[i] == '\n'){
	        c[i] = '\0';
	        break;
	      }
	    }
	    a = strlen(c);
		if(send(_socket, c, a, 0) == -1){	/* send command to bank server */
			perror("Unable to send to server");
			sleep(3);
			}

			if (strcmp(c,"quit") == 0){
				sleep(1);
				return NULL;
			}
    }

}

void * _output(void * b)
{
 	char c[256];
	int counter;
  	for(;;){
	    if(((counter = recv(_socket, c, 255, 0)) == -1)) {
	      	perror("recv failed");
	      	exit(1);
	    }
	    if(counter == 0){
	      	printf("Connection is closed\n");
					exit(1);
	    }
	    c[counter] = '\0';
	    printf("%s\n", c);
  	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]){

	if(argc != 3){
		fprintf(stderr, "Error!  Provide a hostname.  Usage: hostname\n and port number");
		return 1;
	}
	int num_port = atoi(argv[2]);
	if(num_port < 8000){
		fprintf(stderr, "Error!  port number must be greater than 8k\n");
		return -1;
	}
  	struct hostent *name_of_host;
  	struct sockaddr_in address;
  	char ip[INET_ADDRSTRLEN + 1];
	if( (_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		perror("Unable to create socket");
		return -1;
	}
  	name_of_host = gethostbyname(argv[1]);
  	if(name_of_host == NULL)
	{
		perror("Error in retrieving name of host");
		return -1;
	}
  	memset(&address, 0, sizeof(struct sockaddr_in));
	address.sin_family = AF_INET;
	address.sin_port   = htons(num_port);
	//address.sin_addr.s_addr =  inet_ntop(name_of_host->h_addrtype, name_of_host->h_addr_list[0], ip, INET_ADDRSTRLEN)//INADDR_ANY;//(!argv[1]) 
	memcpy(&address.sin_addr, name_of_host->h_addr_list[0], name_of_host->h_length);
  	if(inet_ntop(name_of_host->h_addrtype, name_of_host->h_addr_list[0], ip, INET_ADDRSTRLEN) == NULL)//converts into serverIP
	{
		perror("Error, in converting ip address\n");
		return -1;
	}
  	for(;;){//connects to first available socket
    	printf("Connecting: %s ...\n", ip);
			if(connect(_socket, (struct sockaddr*) &address, sizeof(address)) == -1)
			{
				perror("connection to server failed, retrying after 3 seconds");
				sleep(3);//retry every 3 seconds
			}
			else
			{
			printf("Connected to server on port %s\n",argv[2]);
			break;
			}
  	}
  	if(_socket < 0){
    	return 1;
  	}
  	pthread_t tid_in;
  	pthread_t tid_out;
  	if( (pthread_create(&tid_in, NULL, _input,  NULL) != 0) ||
				(pthread_create(&tid_out,NULL, _output, NULL) != 0) )
		{
			perror("Thread was not created, exiting program\n");
			return 1;
		}
  	pthread_join(tid_out, NULL);
		pthread_join(tid_in,  NULL);
  	close(_socket);
  	return 0;
}

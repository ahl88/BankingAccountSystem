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
#include <netinet/in.h>
#include <errno.h>

typedef struct Account{
	char* name;
	float balance;
	int inSession;
	struct Account* next;
} account;

typedef struct Threads{
	pthread_t tid;
	struct Threads* next;
} threads;

typedef struct Sockets{
	int sockfd;
	struct Sockets* next;
} sockets;

void* service(void* sock);
account* search(char* target);
void createAccount(char* target);
void trackThread(pthread_t newTID);
void trackSocket(int sockfd);
void terminate(int num);
void* status_handler();
void printBankdetails();

account* bank;
threads* createdThreads;
sockets* createdSockets;
int alarmOn;


pthread_mutex_t bankMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t threadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t socketMutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char** argv){
	alarmOn = 1;
	signal(SIGINT, terminate);
	signal(SIGALRM, printBankdetails);
	//Diagnose every 15 seconds
	pthread_t alarmThread;
	pthread_create(&alarmThread,NULL,status_handler,NULL);
	//Check arguments
	if (argc != 2){
		fprintf(stderr, "Invalid arguments");
		exit(1);
	}
	createdThreads = (threads*) malloc(sizeof(threads));
	bank = NULL;
	int port = atoi(argv[1]);
	int sockfd, newsockfd;
	int * arg = (int*) malloc(sizeof(int));
	socklen_t clientLength;
	struct sockaddr_in serverAddr, clientAddr;
	//Create Socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Unable to create socket");
		exit(-1);
	}
	memset(&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = INADDR_ANY; //not sure if this is correct
	printf("Socket Created!\n");
	//Bind to Socket
	if (bind(sockfd, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0){
		perror("Unable to bind");
		exit(1);
	}
	printf("Binded to socket!\n");
	//Listen for connection
	if(listen(sockfd,5) < 0){
		printf("In here\n");
		perror("listen\n");
		exit(1);
	}
	printf("Listening for Connections...\n");
	clientLength = sizeof(clientAddr);

	while(1){
		//Accept Connection
		newsockfd = accept(sockfd, (struct sockaddr*) &clientAddr, &clientLength);
		if(newsockfd == -1) {
			perror("accept\n");
			continue;
		}
		printf("Accepting new connection\n");
		trackSocket(newsockfd);
		//create new client service thread
		pthread_t tid;
		*arg = newsockfd;
		pthread_create(&tid,NULL,service,(void*) arg);
		printf("created new thread\n");
		//Wait
		//Will exit when client sends quit command
	}


	close(sockfd);
	close(newsockfd);
	return 1;
}

void* service(void* sock){
	trackThread(pthread_self());
	int sockfd = *((int*)sock);
	char buffer[1000];
	char * token; //read the first word of input
	float amount;
	account* currentAccount = NULL;
	memset(buffer,'\0',1000);
	strcpy(buffer, "Accepted connection from client\n");
	send(sockfd, buffer,  strlen(buffer), 0);
	memset(buffer,'\0',1000);
	strcpy(buffer, "Enter commands...\n");
	send(sockfd, buffer,  strlen(buffer), 0);

	while (1){
		//receive input from client
		memset(buffer,'\0',1000);
		recv(sockfd, buffer, 1000, 0);
		printf("Recieved: %s\n",buffer);
		token = strtok(buffer, " ");
		if (strcmp(token,"create") == 0){
			//create new account
			token = strtok(NULL, " \n\0"); //account name
			currentAccount = search(token); //error here
			if (currentAccount ==  NULL){
				//Could not find address
				createAccount(token);
				strcpy(buffer, "Account created\n");
				send(sockfd, buffer,  strlen(buffer), 0);
			} else {
				strcpy(buffer,"An account with that name already exists\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}

		} else if (strcmp(token,"serve") == 0){
			//Set in service bool to 1
			token = strtok(NULL, " \n\0"); //account name
			currentAccount = search(token);
			if (currentAccount == NULL){
				//Could not find address
				strcpy(buffer,"An account with that name does not exist\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}
			if (currentAccount->inSession == 1){
				strcpy(buffer,"Account is already in a service session\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}
			currentAccount->inSession = 1;
			strcpy(buffer,"Account is now in a service session\n");
			send(sockfd, buffer, strlen(buffer), 0);

		} else if (strcmp(token,"deposit") == 0){
			//Add money to current balance
			token = strtok(NULL, " \n\0"); //amount float
			amount = atof(token);
			if (currentAccount == NULL || currentAccount->inSession == 0){
				strcpy(buffer,"Account is not in a service session\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}
			currentAccount->balance = currentAccount->balance + amount;
			snprintf(buffer, sizeof(buffer), "Deposited $%f into %s\n",amount,currentAccount->name);
			send(sockfd, buffer, strlen(buffer), 0);

		} else if (strcmp(token,"withdraw") == 0){
			//Remove money from balance
			token = strtok(NULL, " \n\0"); //amount float
			amount = atof(token);
			if (currentAccount == NULL || currentAccount->inSession == 0){
				strcpy(buffer,"Account is not in a service session\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}
			if (amount > currentAccount->balance){
				strcpy(buffer,"Withdraw amount exceeds account balance\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}
			currentAccount->balance = currentAccount->balance - amount;
			snprintf(buffer, sizeof(buffer), "Withdrew $%f from %s\n",amount,currentAccount->name);
			send(sockfd, buffer, strlen(buffer), 0);

		} else if (strcmp(token,"query") == 0){
			//Send account balance
			if (currentAccount == NULL || currentAccount->inSession == 0){
				strcpy(buffer,"Account is not in a service session\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}
			snprintf(buffer, sizeof(buffer), "Account: %s has balance of $%f\n",
					currentAccount->name,currentAccount->balance);
			send(sockfd, buffer, strlen(buffer), 0);

		} else if (strcmp(token,"end") == 0){
			//end current session set service bool to 0
			if (currentAccount == NULL || currentAccount->inSession == 0){
				strcpy(buffer,"Account is not in a service session\n");
				send(sockfd, buffer, strlen(buffer), 0);
				continue;
			}
			currentAccount->inSession = 0;
			snprintf(buffer, sizeof(buffer), "Account: %s is no longer in a service session\n",
					currentAccount->name);
			send(sockfd, buffer, strlen(buffer), 0);

		} else if (strcmp(token,"quit") == 0){
			//dc client from server and end client process
			currentAccount->inSession = 0;
			strcpy(buffer,"Disconnecting from server\n");
			send(sockfd, buffer, strlen(buffer), 0);
			close(sockfd);
			break;
		} else {
			//Invalid input argument(s)
			strcpy(buffer, "Invalid input argument(s)\n");
			send(sockfd, buffer, strlen(buffer), 0);
		}
	}
	pthread_exit(NULL);
	return NULL;
}

account* search(char* target){
	pthread_mutex_lock(&bankMutex);
	account* ptr = bank;
	while (ptr != NULL){
		if (strncmp(ptr->name, target, 255) == 0){
			pthread_mutex_unlock(&bankMutex);
			return ptr;
		}
		ptr = ptr->next;
	}
	pthread_mutex_unlock(&bankMutex);
	return NULL; //target could not be found
}

void createAccount(char* target){
	account* ptr = bank;
	account* prev = NULL;
	account* newAccount = (account*) malloc(sizeof(account));
	newAccount->name = (char*) malloc(256);
	memset(newAccount->name, '\0', 256);
	strcpy(newAccount->name, target);
	newAccount->balance = 0;
	newAccount->inSession = 0;
	newAccount->next = NULL;

	pthread_mutex_lock(&bankMutex);
	while (ptr != NULL){
		prev = ptr;
		ptr = ptr->next;
	}
	if (prev == NULL){ //first item
		bank = newAccount;
	} else {
		prev->next = newAccount;
	}
	pthread_mutex_unlock(&bankMutex);

	return;signal(SIGALRM, printBankdetails);
}

void trackThread(pthread_t newTID){
	threads* ptr = createdThreads;
	threads* prev = NULL;
	threads* newThread = (threads*) malloc(sizeof(threads));
	newThread->tid = newTID;
	newThread->next = NULL;
	pthread_mutex_lock(&threadMutex);
	while(ptr!=NULL){
		prev = ptr;
		ptr = ptr->next;
	}
	if (prev == NULL) {//first item
		ptr = newThread;
	} else {
		prev->next = newThread;
	}
	pthread_mutex_unlock(&threadMutex);

	return;
}

void trackSocket(int sockfd){
	sockets* ptr = createdSockets;
	sockets* prev = NULL;
	sockets* newSocket = (sockets*) malloc(sizeof(sockets));
	newSocket->sockfd = sockfd;
	newSocket->next = NULL;

	pthread_mutex_lock(&socketMutex);
	while(ptr !=NULL){
		prev = ptr;
		ptr = ptr->next;
	}
	if (prev == NULL) {//first item
		ptr = newSocket;
	} else {
		prev->next = newSocket;
	}
	pthread_mutex_unlock(&socketMutex);

	return;
}

void printBankdetails() {
	account* current = bank;
	pthread_mutex_lock(&bankMutex);
	while(current != NULL){
		printf("%s\t%f",current->name,current->balance);
		if (current->inSession == 1){
			printf("\tIN SERVICE\n");
		} else {
			printf("\n");
		}
		current = current->next;
	}
	printf("\n");
	pthread_mutex_unlock(&bankMutex);
}

void* status_handler(){
	signal(SIGALRM, printBankdetails);
	while(alarmOn == 1)
	{
		alarm(15);
		sleep(15);
	}
	pthread_exit(NULL);
}
void terminate(int num){
	char buffer[1000];
	sockets* ptr = createdSockets;
	//stop timer
	alarmOn = 0;
	//lock all accounts**
	pthread_mutex_lock(&bankMutex);

	//disconnect all clients and send shutdown message
	while (ptr != NULL){
		strcpy(buffer, "Server is shutting down\n");
		send(ptr->sockfd, buffer, strlen(buffer), 0);
		ptr = ptr->next;
	}

	//close all sockets
	ptr = createdSockets;
	while (ptr != NULL){
		close(ptr->sockfd);
		ptr = ptr->next;
	}//deallocate
	/*
	free(bank);
	free(createdSockets);
	free(createdThreads);
	*/
	pthread_mutex_unlock(&bankMutex);


	//join all threads (all pthreads exit so we dont have to join??)
	/*
	threads* threadptr = createdThreads;
	while (ptr != NULL){
		pthread
		ptr = ptr->next;
	}*/
	exit(1);
}

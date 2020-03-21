#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <pthread.h>
#include "socket_utils.h"
#include "msgs.h"
#include "dc_structs.h"
#include "dirutils.h"
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

CList* clientList = NULL;
pthread_mutex_t cwmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clmutex = PTHREAD_MUTEX_INITIALIZER;
int bufWrite = 0; //index for the cyclic buffer write
int bufNext = 0; //index for the cyclic buffer
int bufSize;
int semId;
char* dirName;
unsigned short pNumU;
struct sockaddr_in server;
int workThreads;
pthread_t* tids;

char* getIpStr(struct sockaddr_in* client) {
	char* ipStr = malloc(INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &(client->sin_addr), ipStr, INET_ADDRSTRLEN);
	return ipStr;
}

void handleSig(int sig) {
	int sockId = getCSocket(pNumU);
	struct sockaddr_in* client = conCToS(sockId, (struct sockaddr*)&server);
	if (client != NULL) {
		char* ipStr = getIpStr(client);
		char* sendMsg = malloc(500);
		int payloadSize = sizeof(int) + 1 + 32 + 1 + 16 + 1 + 1;
		snprintf(sendMsg, payloadSize, "%d<%d,%hu>", getCode("LOG_OFF"), htonl(client->sin_addr.s_addr), htons(pNumU));
		printf("Sending %s\n", sendMsg);
		send(sockId, sendMsg, payloadSize, 0);
		free(sendMsg);
		free(ipStr);
	}
	fprintf(stdout, "Info: Sent LOG_OGG, now waiting for ok response\n");
	char* readbuf = malloc(3);
	read(sockId, readbuf, 3);
	fprintf(stdout, "Info: Closing server connection\n");
	close(sockId);
	int i, threaderr;
	for (i=0; i<workThreads; i++) {
		if (threaderr = pthread_kill(*(tids+i), 9)) {
			fprintf(stderr, "Error: Error killing thread #%d\n", i);
			continue;
		}
	}
	semctl(semId, 0, IPC_RMID, 0);
	semctl(semId, 1, IPC_RMID, 0);
}

int parseArgs(int argc, char* argv[], short* portNum, char** sip, short* spNum, char** dirName, int* bufSize, int* workThreads) {
	int nextArg;
	int portFlag = 1, sipFlag = 1, sportFlag = 1, dirFlag = 1, bufFlag = 1, threadFlag = 1;
	for (nextArg = 1; nextArg < argc; nextArg++) {
		if (strcmp(argv[nextArg], "-p") == 0 && portFlag) {
			if (++nextArg < argc) {
				*portNum = atoi(argv[nextArg]);
				portFlag = 0;
			} else {
				fprintf(stderr, "Error: Bad parameters format\n");
				return -1;
			}
		} else if (strcmp(argv[nextArg], "-sp") == 0 && sportFlag) {
			if (++nextArg < argc) {
				*spNum = atoi(argv[nextArg]);
				sportFlag = 0;
			} else {
				fprintf(stderr, "Error: Bad parameters format\n");
				return -1;
			}
		} else if (strcmp(argv[nextArg], "-w") == 0 && threadFlag) {
			if (++nextArg < argc) {
				*workThreads = atoi(argv[nextArg]);
				threadFlag = 0;
			} else {
				fprintf(stderr, "Error: Bad parameters format\n");
				return -1;
			}
		} else if (strcmp(argv[nextArg], "-b") == 0 && bufFlag) {
			if (++nextArg < argc) {
				*bufSize = atoi(argv[nextArg]);
				bufFlag = 0;
			} else {
				fprintf(stderr, "Error: Bad parameters format\n");
				return -1;
			}
		} else if (strcmp(argv[nextArg], "-sip") == 0 && sipFlag) {
			if (++nextArg < argc) {
				*sip = argv[nextArg];
				sipFlag = 0;
			} else {
				fprintf(stderr, "Error: Bad parameters format\n");
				return -1;
			}
		} else if (strcmp(argv[nextArg], "-d") == 0 && dirFlag) {
			if (++nextArg < argc) {
				*dirName = argv[nextArg];
				dirFlag = 0;
			} else {
				fprintf(stderr, "Error: Bad parameters format\n");
				return -1;
			}
		}
	}
	return 0;
}

void* thread_f(void* argp) { /* Thread function */
	BElem** belemp = (BElem**) argp;
	fprintf(stdout, "Info: New thread opened\n");
	while (1) {
		// todo
		fprintf(stdout, "INFO: Will now wait for synchronized access in buffer\n");
		struct sembuf semR = {1, -1, 0}; // get a read pos
		struct sembuf semW = {0, 1, 0}; // add a write pos
		semop(semId, &semR, 1);
		pthread_mutex_lock(&crmutex); //blocks concurrent access in bufNext pos
		fprintf(stdout, "INFO: Lock acquired, reading buffer\n");
		//get next message
		char* ip = malloc(strlen((belemp[bufNext])->ip) + 1);
		memcpy(ip, (belemp[bufNext])->ip, strlen((belemp[bufNext])->ip) + 1);
		int port = (belemp[bufNext])->port;
		char* fpath = malloc(strlen((belemp[bufNext])->pathname) + 1);
		snprintf(fpath, strlen((belemp[bufNext])->pathname)+1, "%s", (belemp[bufNext])->pathname);
		long version = (belemp[bufNext])->version;
		fprintf(stdout, "INFO: Waiting to acquire client list lock\n");
		pthread_mutex_lock(&clmutex);
		int exists = existsCList(clientList, port, ip);
		pthread_mutex_unlock(&clmutex);
		free((belemp[bufNext])->ip);
		free(belemp[bufNext]);
		bufNext += 1;
		pthread_mutex_unlock(&crmutex);
		//we can now unlock, all crucial structs are done being used
		fprintf(stdout, "INFO: A writing position will be freed\n");
		semop(semId, &semW, 1);
		if (exists == 1) {
			fprintf(stdout, "Info: Found client with <%s,%d>\n", ip, port);
			//check if contains pathname and version (it will in this first case)
			if (strlen(fpath) > 0) {
				fprintf(stdout, "INFO: Message contains filename %s\n", fpath);
				char* idfpath = malloc(strlen(dirName) + 1 + strlen(fpath) + 1 + strlen(ip) + 1 + 32 + 1);
				fprintf(stdout, "DEBUG: Malloc'd position for total file path\n");
				snprintf(idfpath, strlen(fpath) + 1 + strlen(ip) + 1 + 32 + 1, "%s/%s_%d/%s", dirName, ip, port, fpath);
				fprintf(stdout, "INFO: Client and dirname info appended, total file path %s\n", idfpath);
				long curV = -1;
				if (!fileExists(idfpath)) {
					fprintf(stdout, "INFO: File %s does not exist, asking for it\n", idfpath);
				} else {
					fprintf(stdout, "INFO: File %s exists, asking with current version\n", idfpath);
					curV = getFTime(idfpath);
				}
				//get connection and ask getfile
				int subcomSock = getCSocket();
				struct sockaddr_in subcom;
				subcom.sin_family = AF_INET;
				if (inet_pton(AF_INET, ip, &subcom.sin_addr) <= 0) {
					fprintf(stderr, "Error: Bad server ip address %s, terminating attempt\n", ip);
					continue;
				}
				subcom.sin_port = htons(port);
				struct sockaddr_in* subcl = conCToS(subcomSock, (struct sockaddr*)&subcom);
				if (subcl == NULL) {
					fprintf(stderr, "Error: Connection couldn't be established, terminating attempt\n");
					continue;
				} else {
					char* sendMsg = malloc(500);
					int payloadSize = sizeof(int) + 1 + 32 + 1 + 16 + 1 + strlen(fpath) + 1 + 32 + 1;
					snprintf(sendMsg, payloadSize, "%d<%d,%hu>%s,%ld", getCode("GET_FILE"), htonl(subcl->sin_addr.s_addr), htons((unsigned short)port), fpath, curV);
					fprintf(stdout, "Info: Sending getfile request %s\n", sendMsg);
					send(subcomSock, sendMsg, payloadSize, 0);
					fprintf(stdout, "Info: Reading getfile response\n");
					char* buf = malloc(16000); //msg format 200 v bytes OR 801 OR 802
					int rv = read(subcomSock, buf, 16000);
					buf[rv] = '\0';
					int tokindex = strchr(buf, ' ') - buf;
					buf[tokindex] = '\0'; //this is done to split the strings
					int codeInt = atoi(buf);
					if (codeInt == 801) {
						fprintf(stdout, "WARN: File was not found by the requested client\n");
					} else if (codeInt == 802) {
						fprintf(stdout, "INFO: File is up to date\n");
					} else if (codeInt == 200) {
						fprintf(stdout, "INFO: Writing file at %s\n", idfpath);
						fprintf(stdout, "DEBUG: File data received %s\n", &(buf[tokindex+1]));
						rAction(idfpath, &(buf[tokindex+1])); //move after the whitespace
					} else {
						fprintf(stdout, "WARN: Unknown response code\n");
					}
					fprintf(stdout, "INFO: Finished reading file\n");
					free(sendMsg);
					free(buf);
					// free(idfpath); it is freed by the rAction
				}
			} else {
				int subcomSock = getCSocket();
				struct sockaddr_in subcom;
				subcom.sin_family = AF_INET;
				if (inet_pton(AF_INET, ip, &subcom.sin_addr) <= 0) {
					fprintf(stderr, "Error: Bad server ip address %s, terminating attempt\n", ip);
					continue;
				}
				subcom.sin_port = htons(port);
				struct sockaddr_in* subcl = conCToS(subcomSock, (struct sockaddr*)&subcom);
				fprintf(stdout, "INFO: File name not contained in message\n");
				char* sendMsg = malloc(500);
				int payloadSize = sizeof(int)+1+32+1+16+1;
				snprintf(sendMsg, payloadSize, "%d<%d,%hu>", getCode("GET_FILE_LIST"), htonl(subcl->sin_addr.s_addr), htons((unsigned short)port));
				fprintf(stdout, "Info: Sending get all files request %s\n", sendMsg);
				send(subcomSock, sendMsg, payloadSize, 0);
				fprintf(stdout, "Info: Reading get_file_list response\n");
				char* buf = malloc(10000);
				read(subcomSock, buf, 500);
				fprintf(stdout, "Debug: get_file_list response: %s\n", buf);
				//todo extract by ','
				char* subtok = strtok(buf, ",");
				while (subtok != NULL) {
					fprintf(stdout, "INFO: File name retrieved: %s\n", subtok);
					//write a message to buffer, with this token, the 'port' and the 'ip'
					BElem* newQ = malloc(sizeof(BElem));
					snprintf(newQ->pathname, 128, "%s", subtok);
					newQ->version = -1;
					newQ->ip = malloc(INET_ADDRSTRLEN);
					snprintf(newQ->ip, INET_ADDRSTRLEN, "%s", ip);
					newQ->port = port;
					fprintf(stdout, "INFO: Structure created, will now save into buffer\n");
					struct sembuf semW = {0, -1, 0}; //get a write pos
					struct sembuf semR = {1, 1, 0}; //enable a read pos
					semop(semId, &semW, 1);
					pthread_mutex_lock(&cwmutex); //block concurrent access to bufWrite
					belemp[bufWrite] = newQ;
					bufWrite++;
					pthread_mutex_unlock(&cwmutex);
					semop(semId, &semR, 1);
					subtok = strtok(NULL, ",");
				}
				fprintf(stdout, "Info: Finished reading get_file_list\n");
			}
		} else {
			fprintf(stdout, "WARN: Client <%s,%d> was not found\n", ip, port);
			continue;
		}
		free(ip);
		free(fpath);
	}
	pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
	signal(SIGINT, handleSig);
	signal(SIGUSR1, handleSig);
	signal(SIGQUIT, handleSig);
	short pNum = -1;
	short spNum = -1;
	char* sip;
	bufSize = 1000;
	workThreads = 1;
	if (parseArgs(argc, argv, &pNum, &sip, &spNum, &dirName, &bufSize, &workThreads) == -1) {
		fprintf(stderr, "Error: Invalid parameters, terminating\n");
		return -11;
	}
	if (pNum < 0) {
		fprintf(stderr, "Error: Invalid port, terminating\n");
		return -1;
	}
	if (spNum < 0) {
		fprintf(stderr, "Error: Invalid server port, terminating\n");
		return -2;
	}
	pNumU = pNum;
	// create incoming client socket
	int csocket = getSSocket(pNumU);
	unsigned short spNumU = spNum;
	//create server socket
	int sockId = getCSocket();
	//connect to server socket
	server.sin_family = AF_INET;
	if (inet_pton(AF_INET, sip, &server.sin_addr) <= 0) {
		fprintf(stderr, "Error: Bad server ip address %s, terminating\n", sip);
		return -3;
	}
	server.sin_port = htons(spNumU);
	//connect to server and log on
	struct sockaddr_in* client;
	client = conCToS(sockId, (struct sockaddr*)&server);
	if (client != NULL) {
		char* ipStr = getIpStr(client);
		char* sendMsg = malloc(500);
		int payloadSize = sizeof(int) + 1 + 32 + 1 + 16 + 1 + 1;
		snprintf(sendMsg, payloadSize, "%d<%d,%hu>", getCode("LOG_ON"), htonl(client->sin_addr.s_addr), htons(pNumU));
		printf("Sending %s\n", sendMsg);
		send(sockId, sendMsg, payloadSize, 0);
		free(sendMsg);
		free(ipStr);
	}
	fprintf(stdout, "Info: Closing server connection\n");
	close(sockId);
	free(client);
	sockId = getCSocket(pNumU);
	//reconnect to server and ask for clients
	client = conCToS(sockId, (struct sockaddr*)&server);
	if (client != NULL) {
		char* ipStr = getIpStr(client);
		char* sendMsg = malloc(500);
		char* buf = malloc(500);
		int payloadSize = sizeof(int) + 1 + 32 + 1 + 16 + 1 + 1;
		snprintf(sendMsg, payloadSize, "%d<%d,%hu>", getCode("GET_CLIENTS"), htonl(client->sin_addr.s_addr), htons(pNumU));
		fprintf(stdout, "Info: Sending getclients request %s\n", sendMsg);
		send(sockId, sendMsg, payloadSize, 0);
		read(sockId, buf, 500);
		fprintf(stdout, "Info: Response from getclients request%s\n", buf);
		// tokenize by > and save
		char* nextTok = strtok(buf, ">");
		while (nextTok != NULL) {
			fprintf(stdout, "Debug: Token %s\n", nextTok);
			// swap , with \0 to get 2 strings
			int swapPos = strchr(nextTok, ',') - nextTok; // a hack to reduce pointer to offset
			nextTok[swapPos] = '\0';
			char* ipTemp = &(nextTok[1]); // skips <
			fprintf(stdout, "Debug: Ip subtoken %s\n", ipTemp);
			char* portTemp = &(nextTok[swapPos+1]);
			fprintf(stdout, "Debug: Port subtoken %s\n", portTemp);
			// ip
			char* ipStr = malloc(INET_ADDRSTRLEN);
			struct sockaddr_in tempAddrStruct;
			tempAddrStruct.sin_addr.s_addr = ntohl(atoi(ipTemp));
			inet_ntop(AF_INET, &(tempAddrStruct.sin_addr), ipStr, INET_ADDRSTRLEN);
			// port
			unsigned short cpNum = ntohs(atoi(portTemp));
			fprintf(stdout, "Info: Saving client info <%s,%hu>\n", ipStr, cpNum);
			addCList(&clientList, (int)cpNum, ipStr);
			free(ipStr);
			nextTok = strtok(NULL, ">");
		}
		free(buf);
		free(sendMsg);
		free(ipStr);
	}
	fprintf(stdout, "Info: Finished initial server communication\n");

	fprintf(stdout, "Info: Creating semaphores for synchronous communication\n");
	//make sems
	semId = semget((key_t) getpid(), 2, IPC_CREAT | 0660);
	union semun arg;
	arg.val = bufSize;
	semctl(semId, 0, SETVAL, arg); // 0 is used to write
	arg.val = 0;
	semctl(semId, 1, SETVAL, arg); // 1 is used to read

	fprintf(stdout, "Info: Creating buffer for thread communication\n");
	// make buffer
	BElem* cBuffer[bufSize];

	int j;
	for (j=0;j<bufSize;j++) {
		cBuffer[j] = NULL;
	}

	// start threads
	tids = malloc(workThreads * sizeof(pthread_t));
	int i, threaderr;
	for (i=0; i<workThreads; i++) {
		if (threaderr = pthread_create(tids+i, NULL, thread_f, &cBuffer)) {
			fprintf(stderr, "Error: Error creating thread #%d\n", i);
			exit(1);
		}
	}

	int sd, max_sd;
	fd_set readfds;
	int client_socket[30];
	for (j=0; j<30; j++) {
		client_socket[j] = 0;
	}

	//before accepting anything, ask workers to request files (for clients already active)
	CList* clientListCp = clientList;
	pthread_mutex_lock(&clmutex);
	while (clientListCp != NULL) {
		BElem* newQ = malloc(sizeof(BElem));
		(newQ->pathname)[0] = '\0';
		newQ->version = -1;
		newQ->ip = malloc(INET_ADDRSTRLEN);
		snprintf(newQ->ip, INET_ADDRSTRLEN, "%s", clientListCp->ip);
		newQ->port = clientListCp->port;
		fprintf(stdout, "INFO: Structure created,will now save into buffer\n");
		struct sembuf semW = {0, -1, 0}; //get a write pos
		struct sembuf semR = {1, 1, 0}; //enable a read pos
		semop(semId, &semW, 1);
		pthread_mutex_lock(&cwmutex); //block concurrent access to bufWrite
		cBuffer[bufWrite] = newQ;
		bufWrite++;
		pthread_mutex_unlock(&cwmutex);
		semop(semId, &semR, 1);
		clientListCp = clientListCp->next;
	}
	pthread_mutex_unlock(&clmutex);

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(csocket, &readfds);
		max_sd = csocket;
		int i;
		for (i=0; i<30; i++) {
			sd = client_socket[i];
			if (sd > 0) {
				FD_SET(sd, &readfds);
			}
			if (sd > max_sd) {
				max_sd = sd;
			}
		}
		int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		if (activity < 0) {
			fprintf(stderr, "Error: Error accepting connection\n");
			continue;
		}
		struct sockaddr_in address;
		int addrlen = sizeof(address);
		// csocket event
		if (FD_ISSET(csocket, &readfds)) {
			fprintf(stdout, "INFO: Accepted connection in master socket\n");
			int newsoc;
			if ((newsoc = accept(csocket, (struct sockaddr*)&address, &addrlen)) < 0) {
				fprintf(stderr, "Error: Error accepting connection\n");
				continue;
			} /*else {
				fprintf(stdout, "Info: Accepted connection in master client socket\n");
				//todo ask workers to send getfiles
				BElem* newQ = malloc(sizeof(BElem));
				newQ->pathname = NULL;
				newQ->version = -1;
				newQ->ip = malloc(INET_ADDRSTRLEN);
				snprintf(newQ->ip, INET_ADDRSTRLEN, "%s", ip);
				newQ->port = port;
				fprintf(stdout, "INFO: Structure created,will now save into buffer\n");
				struct sembuf semW = {0, -1, 0}; //get a write pos
				struct sembuf semR = {1, 1, 0}; //enable a read pos
				semop(semId, &semW, 1);
				pthread_mutex_lock(&cwmutex); //block concurrent access to bufWrite
				belemp[bufWrite] = newQ;
				bufWrite++;
				pthread_mutex_unlock(&cwmutex);
				semop(semId, &semR, 1);
			}*/
			for (i=0;i<30;i++) {
				if (client_socket[i] == 0) {
					client_socket[i] = newsoc;
					fprintf(stdout, "INFO: New monitored socket in position %d\n", i);
					break;
				}
			}
		}
			// client_socket event
		fprintf(stdout, "INFO: Accepted connection in one of the monitored sockets\n");
		for (i=0;i<30;i++) {
			sd = client_socket[i];
			if (FD_ISSET(sd, &readfds)) {
				fprintf(stdout, "INFO: The connection is at monitored socket %d\n", sd);
				int valr;
				char* buf = malloc(5000);
				if ((valr = read(sd, buf, 5000)) == 0) {
					fprintf(stdout, "WARN: User disconnected\n");
					close(sd);
					client_socket[i] = 0;
				} else {
					if (strlen(buf) == 0) {
						fprintf(stdout, "WARN: Invalid message\n");
						continue;
					}
					fprintf(stdout, "INFO: Will now extract info of message\n");
					fprintf(stdout, "DEBUG: Message content: %s\n", buf);
					int codeInt = 1;
					char* ipStr = malloc(INET_ADDRSTRLEN);
					ipStr[0] = '\0'; //guard segfault
					int cpNum = 0;
					char* tempbuf = malloc(5000);
					memcpy(tempbuf, buf, 5000);
					extractHead(tempbuf, &codeInt, &ipStr, &cpNum);
					free(tempbuf);
					if (codeInt == 604) {
						fprintf(stdout, "Info: USER_OFF received with <%s,%hu>\n", ipStr, cpNum);
						fprintf(stdout, "Info: Removing ip-port pair from structure\n");
						pthread_mutex_lock(&clmutex);
						removeCList(&clientList, (int)cpNum, ipStr);
						pthread_mutex_unlock(&clmutex);
					} else if (codeInt == 603) {
						fprintf(stdout, "Info: USER_ON received with <%s,%hu>\n", ipStr, cpNum);
						fprintf(stdout, "Info: Adding ip-port to structure\n");
						pthread_mutex_lock(&clmutex);
						addCList(&clientList, (int)cpNum, ipStr);
						pthread_mutex_unlock(&clmutex);
						BElem* newQ = malloc(sizeof(BElem));
						(newQ->pathname)[0] = '\0';
						newQ->version = -1;
						newQ->ip = malloc(INET_ADDRSTRLEN);
						snprintf(newQ->ip, INET_ADDRSTRLEN, "%s", ipStr);
						newQ->port = cpNum;
						fprintf(stdout, "INFO: Structure created,will now save into buffer\n");
						struct sembuf semW = {0, -1, 0}; //get a write pos
						struct sembuf semR = {1, 1, 0}; //enable a read pos
						semop(semId, &semW, 1);
						pthread_mutex_lock(&cwmutex); //block concurrent access to bufWrite
						cBuffer[bufWrite] = newQ;
						bufWrite++;
						pthread_mutex_unlock(&cwmutex);
						semop(semId, &semR, 1);
					} else if (codeInt == 702) {
						fprintf(stdout, "Info: GET_FILE_LIST received from <%s,%hu>\n", ipStr, cpNum);
						fprintf(stdout, "Info: Getting file list from system\n");
						char* myfiles = getMyFiles(dirName);
						send(sd, myfiles, 10000, 0);
					} else if (codeInt == 703) {
						fprintf(stdout, "Info: GET_FILE received from <%s,%hu>\n", ipStr, cpNum);
						fprintf(stdout, "DEBUG: buffer is now at %s\n", buf);
						int postHead = strchr(buf, '>') - buf + 1;
						fprintf(stdout, "DEBUG: postHead index value %d\n", postHead);
						int postName = strrchr(buf, ',') - buf;
						fprintf(stdout, "DEBUG: postName index value %d\n", postName);
						fprintf(stdout, "DEBUG: Finished with the token positions\n");
						fprintf(stdout, "DEBUG: Tokens %s and %s\n", &(buf[postHead]), &(buf[postName]));
						buf[postName] = '\0';
						fprintf(stdout, "DEBUG: post head %s, post name %s\n", &(buf[postHead]), &(buf[postName]));
						if (!fileExists(&(buf[postHead]))) {
							fprintf(stdout, "INFO: File does not exist\n");
							char* respbuf = malloc(500);
							snprintf(respbuf, 500, "%d ", getCode("FILE_NOT_FOUND"));
							send(sd, respbuf, 500, 0);
						} else if (strtol(&(buf[postName+1]), NULL, 10) >= getFTime(&(buf[postHead]))) {
							fprintf(stdout, "INFO: File up to date\n");
							char* respbuf = malloc(500);
							snprintf(respbuf, 500, "%d ", getCode("FILE_UP_TO_DATE"));
							send(sd, respbuf, 500, 0);
						} else {
							char* fData = getFData(&(buf[postHead]));
							char* respbuf = malloc(16000);
							snprintf(respbuf, 16000, "200 %s", fData);
							send(sd, respbuf, 16000, 0);
							free(fData);
						}
					} else {
						fprintf(stdout, "Warn: Unrecognized command code %d\n", codeInt);
					}
					free(ipStr);
				}
			}
		}
	}


	for (i=0; i<workThreads; i++) {
		if (threaderr = pthread_join(*(tids+i), NULL)) {
			fprintf(stderr, "Error: Error joining thread #%d\n", i);
			exit(1);
		}
	}

	fprintf(stdout, "Info: All threads have terminated\n");
	free(tids);

	sockId = getCSocket(pNumU);
	client = conCToS(sockId, (struct sockaddr*)&server);
	if (client != NULL) {
		char* ipStr = getIpStr(client);
		printf("%s %hu\n", ipStr, pNumU);

		char* sendMsg = malloc(500);
		int payloadSize = sizeof(int) + 1 + 32 + 1 + 16 + 1 + 1;
		snprintf(sendMsg, payloadSize, "%d<%d,%hu>", getCode("LOG_OFF"), htonl(client->sin_addr.s_addr), htons(pNumU));
		printf("Sending %s\n", sendMsg);
		send(sockId, sendMsg, payloadSize, 0);
		free(sendMsg);
		free(ipStr);
	}
	fprintf(stdout, "Info: Closing server connection\n");
	close(sockId);

	clearCList(clientList);
	//todo free args if possible
	return 0;
}

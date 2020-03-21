#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include "ds_structs.h"
#include "socket_utils.h"
#include "msgs.h"

int parseArgs(int argc, char* argv[], short* portNum) {
	int nextArg;
	int portFlag = 1;
	for (nextArg = 1; nextArg < argc; nextArg++) {
		if (strcmp(argv[nextArg], "-p") == 0 && portFlag) {
			if (++nextArg < argc) {
				*portNum = atoi(argv[nextArg]);
				portFlag = 0;
			} else {
				fprintf(stderr, "Error: Bad parameters format\n");
				return -1;
			}
		}
	}
	return 0;
}

int main(int argc, char* argv[]) {
	short pNum = -1;
	if (parseArgs(argc, argv, &pNum) == -1) {
		fprintf(stderr, "Error: Invalid parameters, terminating\n");
		return -11;
	}
	if (pNum < 0) {
		fprintf(stderr, "Error: Invalid port, terminating\n");
		return -1;
	}
	unsigned short pNumU = pNum;
	SList* clientList = NULL;
	int sockId;
	if ((sockId = getSSocket(pNumU)) < 0) {
		fprintf(stderr, "Error: Unexpected error on socket creation, terminating\n");
	}

	fd_set readfds;
	int client_socket[30];
	int i, sd, max_sd;
	for (i=0; i<30; i++) {
		client_socket[i] = 0;
	}

	while (1) {
		struct sockaddr_in client = {0};
		int clilen = sizeof(client);
			int accRes = accept(sockId, (struct sockaddr *) &client, &clilen);
			if (accRes < 0) {
				fprintf(stderr, "Error: Error accepting connection\n");
				continue;
			}
		if (accRes < 0) {
			fprintf(stderr, "Error: Culdn't accept connection\n");
		} else {
			char* cIpStr = malloc(sizeof(INET_ADDRSTRLEN));
			unsigned short cPort = client.sin_port;
			inet_ntop(AF_INET, &(client.sin_addr), cIpStr, INET_ADDRSTRLEN);
			fprintf(stdout, "Info: Accepted connection from <%s, %hu>\n", cIpStr, cPort);
			char buf[500] = {0};
			int payloadSize = sizeof(int) + 1 + 32 + 1 + 16 + 1 + 1;
			read(accRes, buf, payloadSize);
			fprintf(stdout, "Info: Server received: %s|\n", buf);
			//separate code from payload using atoi
			int codeInt;
			char* ipStr = malloc(INET_ADDRSTRLEN);
			int cpNum;
			extractHead(buf, &codeInt, &ipStr, &cpNum);
			if (codeInt == 601) {
				fprintf(stdout, "Info: LOG_ON received with <%s,%hu>\n", ipStr, cpNum);
				fprintf(stdout, "Info: Adding ip-port pair to structure\n");
				addSList(&clientList, (int)cpNum, ipStr);
				//todo send to all clients USER_ON
				char* sendMsg = malloc(500);
				struct sockaddr_in tempAStruct;
				inet_pton(AF_INET, ipStr, &(tempAStruct.sin_addr));
				snprintf(sendMsg, sizeof(int)+1+32+1+16+1, "%d<%d,%hu>", getCode("USER_ON"), htonl(tempAStruct.sin_addr.s_addr), htons(cpNum));
				fprintf(stdout, "Debug: USER_ON message: %s\n", sendMsg);
				SList* clientsGet = getSListClients(clientList, cpNum, ipStr);
				SList* clientsGetCp = clientsGet;
				while (clientsGetCp != NULL) {
					int nestSockId;
					if ((nestSockId = getCSocket()) < 0) {
						fprintf(stderr, "Error: Unexpected error on socket creation for <%s,%hu>\n", ipStr, cpNum);
					}
					struct sockaddr_in clRes;
					clRes.sin_family = AF_INET;
					if (inet_pton(AF_INET, clientsGetCp->ip, &clRes.sin_addr) <= 0) {
						fprintf(stderr, "Error: Bad ip\n");
					}
					clRes.sin_port = htons(clientsGetCp->port);
					struct sockaddr_in* clientConnection;
					clientConnection = conCToS(nestSockId, (struct sockaddr*)&clRes);
					if (clientConnection != NULL) {
						fprintf(stdout, "Info: Sending USER_ON message\n");
						send(nestSockId, sendMsg, 500, 0);
						free(clientConnection);
					}
					clientsGetCp = clientsGetCp->next;
				}
				free(sendMsg);
				clearSList(clientsGet);
				//free(ipStr);
			} else if (codeInt == 602) {
				fprintf(stdout, "Info: LOG_OFF received with <%s,%hu>\n", ipStr, cpNum);
				fprintf(stdout, "Info: Removing ip-port pair from structure\n");
				int removed = removeSList(&clientList, (int)cpNum, ipStr);
				if (removed == 0) {
					fprintf(stdout, "WARN: Ip-port pair not found\n");
					char* badresponse = malloc(sizeof(int)+1);
					snprintf(badresponse, sizeof(int)+1, "%d", getCode("ERROR_IP_PORT_NOT_FOUND_IN_LIST"));
					send(accRes, badresponse, sizeof(int)+1, 0);
					continue;
				}
				fprintf(stdout, "INFO: Log off ok\n");
				send(accRes, "OK", 3, 0);
				//todo send to all clients USER_OFF
				char* sendMsg = malloc(500);
				struct sockaddr_in tempAStruct;
				inet_pton(AF_INET, ipStr, &(tempAStruct.sin_addr));
				snprintf(sendMsg, sizeof(int)+1+32+1+16+1, "%d<%d,%hu>", getCode("USER_OFF"), htonl(tempAStruct.sin_addr.s_addr), htons(cpNum));
				fprintf(stdout, "Debug: USER_OFF message: %s\n", sendMsg);
				SList* clientsGet = getSListClients(clientList, cpNum, ipStr);
				SList* clientsGetCp = clientsGet;
				while (clientsGetCp != NULL) {
					int nestSockId;
					if ((nestSockId = getCSocket(cpNum)) < 0) {
						fprintf(stderr, "Error: Unexpected error on socket creation for <%s,%hu>\n", ipStr, cpNum);
					}
					struct sockaddr_in clRes;
					clRes.sin_family = AF_INET;
					if (inet_pton(AF_INET, clientsGetCp->ip, &clRes.sin_addr) <= 0) {
						fprintf(stderr, "Error: Bad ip\n");
					}
					clRes.sin_port = htons(clientsGetCp->port);
					struct sockaddr_in* clientConnection;
					clientConnection = conCToS(nestSockId, (struct sockaddr*)&clRes);
					if (clientConnection != NULL) {
						fprintf(stdout, "Info: Sending USER_OFF message\n");
						send(nestSockId, sendMsg, 500, 0);
						free(clientConnection);
					}
					clientsGetCp = clientsGetCp->next;
				}
				free(sendMsg);
				clearSList(clientsGet);
			} else if (codeInt == 701) {
				fprintf(stdout, "Info: GET_CLIENTS received with <%s,%hu>\n", ipStr, cpNum);
				char* sendMsg = malloc(500);
				sendMsg[0] = '\0'; //making sure for empty string fallback
				int payloadSize = 500; //lets use it all but \0 will take care
				int currSendPtr = 0; //offset ptr for snprintf
				SList* clientsGet = getSListClients(clientList, cpNum, ipStr);
				SList* clientsGetCp = clientsGet;
				while (clientsGetCp != NULL) {
					//get size, replace \0 and continue
					currSendPtr = strlen(sendMsg);
					// write over \0 on next write
					fprintf(stdout, "INFO: Client: <%s, %hu>\n", clientsGetCp->ip, clientsGetCp->port);
					struct sockaddr_in tempAStruct;
					inet_pton(AF_INET, clientsGetCp->ip, &(tempAStruct.sin_addr));
					snprintf(&(sendMsg[currSendPtr]), 1 + 32 + 1 + 16 + 1, "<%d,%hu>", htonl(tempAStruct.sin_addr.s_addr), htons(clientsGetCp->port));
					clientsGetCp = clientsGetCp->next;
				}
				fprintf(stdout, "Message:%s\n", sendMsg);
				send(accRes, sendMsg, 500, 0);
				free(sendMsg);
				clearSList(clientsGet);
			}
			else {
				fprintf(stdout, "Warn: Unknown message code\n");
			}
			free(ipStr);
			free(cIpStr);
			//}
		}
		//close(sockId);
		//return 0;
		//client thread should close the socket, process and exit
		//server thread should close the accept and continue looping
	}
	clearSList(clientList);
	return 0;
}

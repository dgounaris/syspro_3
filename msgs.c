#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "msgs.h"

int getCode(char* code) {
	//normal codes
	if (strcmp(code, "OK") == 0) {
		return 200;
	}
	//connection msgs
	if (strcmp(code, "LOG_ON") == 0) {
		return 601;
	}
	if (strcmp(code, "LOG_OFF") == 0) {
		return 602;
	}
	if (strcmp(code, "USER_ON") == 0) {
		return 603;
	}
	if (strcmp(code, "USER_OFF") == 0) {
		return 604;
	}
	//get messages
	if (strcmp(code, "GET_CLIENTS") == 0) {
		return 701;
	}
	if (strcmp(code, "GET_FILE_LIST") == 0) {
		return 702;
	}
	if (strcmp(code, "GET_FILE") == 0) {
		return 703;
	}
	//files
	if (strcmp(code, "FILE_NOT_FOUND") == 0) {
		return 801;
	}
	if (strcmp(code, "FILE_UP_TO_DATE") == 0) {
		return 802;
	}
	//errors
	if (strcmp(code, "ERROR_IP_PORT_NOT_FOUND_IN_LIST") == 0) {
		return 901;
	}
	fprintf(stdout, "Warn: no message code found for %s\n", code);
	return -1;
}

void extractHead(char* msg, int* codeInt, char** ipStr, int* port) {
	fprintf(stdout, "Info: Message extraction start\n");
	*codeInt = atoi(strtok(msg, "<"));
	char* pretok = strtok(NULL, "<");
	char* ipTemp = strtok(pretok, ",");
	char* posttok = strtok(NULL, ",");
	char* portTemp = strtok(posttok, ">");
	//ipstr must be malloc'd
	struct sockaddr_in tempAddrStruct;
	tempAddrStruct.sin_addr.s_addr = ntohl(atoi(ipTemp));
	inet_ntop(AF_INET, &(tempAddrStruct.sin_addr), *ipStr, INET_ADDRSTRLEN);
	*port = (int)ntohs(atoi(portTemp));
	fprintf(stdout, "Info: Message extraction ended\n");
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dc_structs.h"

void addCList(CList** cList, unsigned short port, char* ip) {
	if (*cList == NULL) {
		*cList = malloc(sizeof(CList));
		(*cList)->port = port;
		(*cList)->ip = malloc(strlen(ip)+1);
		memcpy((*cList)->ip, ip, strlen(ip)+1);
		(*cList)->next = NULL;
		return;
	}
	CList* temp = *cList;
	while (temp->next != NULL) {
		if (strcmp(temp->ip, ip) == 0 && port == temp->port) {
			fprintf(stdout, "Info: Pair <%s, %d> already exists\n", ip, port);
			return;
		}
		temp = temp->next;
	}
	if (strcmp(temp->ip, ip) == 0 && port == temp->port) {
		fprintf(stdout, "Info: Pair <%s, %d> already exists\n", ip, port);
		return;
	}
	temp->next = malloc(sizeof(CList));
	temp->next->port = port;
	temp->next->ip = malloc(strlen(ip)+1);
	memcpy(temp->next->ip, ip, strlen(ip)+1);
	temp->next->next = NULL;
}

//exclude self port and ip
CList* getCListClients(CList* cList, unsigned short port, char* ip) {
	if (cList == NULL) {
		return NULL;
	}
	CList* cListOri = cList;
	CList* cListCp = NULL;
	do {
		if (cListOri->port != port || strcmp(ip, cListOri->ip) != 0) {
			addCList(&cListCp, cListOri->port, cListOri->ip);
		}
		cListOri = cListOri->next;
	} while (cListOri != NULL);
	return cListCp;
}

int existsCList(CList* cList, unsigned short port, char* ip) {
	CList* cpList = cList;
	while (cpList != NULL) {
		if (cpList->port == port && strcmp(ip, cpList->ip) == 0) {
			return 1;
		}
		cpList = cpList->next;
	}
	return 0;
}

void removeCList(CList** cList, unsigned short port, char* ip) {
	CList* temp = *cList;
	CList* prev = NULL;
	while (temp != NULL) {
		if (temp->port == port && strcmp(temp->ip, ip) == 0) {
			if (prev != NULL) {
				prev->next = temp->next;
				free(temp);
				return;
			} else {
				*cList = (*cList)->next;
				free(temp);
				return;
			}
		}
		prev = temp;
		temp = temp->next;
	}
}

void clearCList(CList* cList) {
	if (cList == NULL) {
		return;
	}
	if (cList->next != NULL) {
		clearCList(cList->next);
	}
	free(cList->ip);
	free(cList);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ds_structs.h"

void addSList(SList** sList, unsigned short port, char* ip) {
	if (*sList == NULL) {
		*sList = malloc(sizeof(SList));
		(*sList)->port = port;
		(*sList)->ip = malloc(strlen(ip)+1);
		memcpy((*sList)->ip, ip, strlen(ip)+1);
		(*sList)->next = NULL;
		return;
	}
	SList* temp = *sList;
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
	temp->next = malloc(sizeof(SList));
	temp->next->port = port;
	temp->next->ip = malloc(strlen(ip)+1);
	memcpy(temp->next->ip, ip, strlen(ip)+1);
	temp->next->next = NULL;
}

//exclude self port and ip
SList* getSListClients(SList* sList, unsigned short port, char* ip) {
	if (sList == NULL) {
		return NULL;
	}
	SList* sListOri = sList;
	SList* sListCp = NULL;
	do {
		if (sListOri->port != port || strcmp(ip, sListOri->ip) != 0) {
			addSList(&sListCp, sListOri->port, sListOri->ip);
		}
		sListOri = sListOri->next;
	} while (sListOri != NULL);
	return sListCp;
}

int existsSList(SList* sList, unsigned short port, char* ip) {
	SList* cpList = sList;
	while (cpList != NULL) {
		if (cpList->port == port && strcmp(ip, cpList->ip) == 0) {
			return 1;
		}
		cpList = cpList->next;
	}
	return 0;
}

int removeSList(SList** sList, unsigned short port, char* ip) {
	SList* temp = *sList;
	SList* prev = NULL;
	while (temp != NULL) {
		if (temp->port == port && strcmp(temp->ip, ip) == 0) {
			if (prev != NULL) {
				prev->next = temp->next;
				free(temp);
				return 1;
			} else {
				*sList = (*sList)->next;
				free(temp);
				return 1;
			}
		}
		prev = temp;
		temp = temp->next;
	}
	return 0;
}

void clearSList(SList* sList) {
	if (sList == NULL) {
		return;
	}
	if (sList->next != NULL) {
		clearSList(sList->next);
	}
	free(sList->ip);
	free(sList);
}

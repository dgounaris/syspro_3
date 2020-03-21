#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include "dirutils.h"

char* getMyFiles(char* rootpath) {
	//final format %s,%s,...,%s
	DIR* dir;
	struct dirent* ent;
	if ((dir = opendir(rootpath)) != NULL) {
		char* returned = NULL;
		while ((ent = readdir(dir)) != NULL) {
			if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
				continue;
			}
			int pathLen = strlen(rootpath) + strlen(ent->d_name);
			char* nextPath = malloc(pathLen+2);
			snprintf(nextPath, pathLen+2, "%s/%s", rootpath, ent->d_name);
			if (ent->d_type == DT_DIR) {
				fprintf(stdout, "INFO: Directory %s found, writing included file names recursively\n", nextPath);
				char* internalFiles = getMyFiles(nextPath);
				if (internalFiles != NULL && strlen(internalFiles) > 0) {
					//append to returned
					if (returned == NULL) {
						returned = malloc(strlen(internalFiles)+1);
						snprintf(returned, strlen(internalFiles)+1, "%s", internalFiles);
					} else {
						char* oldRet = returned;
						returned = malloc(strlen(oldRet) + strlen(internalFiles) + 2);
						snprintf(returned, strlen(oldRet) + strlen(internalFiles) + 2, "%s,%s", oldRet, internalFiles);
						free(oldRet);
					}
				}
			} else {
				fprintf(stdout, "INFO: File %s found, writing to total files\n", nextPath);
				if (returned == NULL) {
					returned = malloc(strlen(nextPath) + 1);
					snprintf(returned, strlen(nextPath)+1, "%s", nextPath);
				} else {
					char* oldRet = returned;
					returned = malloc(strlen(oldRet) + strlen(nextPath) + 2);
					snprintf(returned, strlen(oldRet) + strlen(nextPath) + 2, "%s,%s", oldRet, nextPath);
					free(oldRet);
				}
			}
		}
		return returned;
	} else {
		fprintf(stderr, "ERROR: Could not open directory %s\n", rootpath);
	}
}

int rAction(char* path, char* fdata) {
	char* pathcp = malloc(strlen(path)+1);
	snprintf(pathcp, strlen(path)+1, "%s", path);
	createRDir(path);
	fprintf(stdout, "INFO: Opening file %s to copy...\n", pathcp);
	FILE* fp = fopen(pathcp, "w");
	fwrite(fdata, sizeof(char), strlen(fdata), fp);
	fclose(fp);
	free(path);
	free(pathcp);
}

void createRDir(char* path) {
	char* token = strtok(path, "/");
	if (token == NULL) {
		fprintf(stdout, "WARN: Empty filepath passed to reader for dir %s\n", path);
		return;
	}
	char* lookuph = strtok(NULL, "/");
	char* newdirname = malloc(strlen(token) + 1);
	snprintf(newdirname, strlen(token) + 1, "%s", token);
	while (lookuph != NULL) {
		DIR* dir = opendir(newdirname);
		if (dir) {
			fprintf(stdout, "INFO: Directory %s already exists\n", newdirname);
		} else if (ENOENT == errno) {
			if (mkdir(newdirname, 0777) == 0) {
				fprintf(stdout, "INFO: Created directory %s\n", newdirname);
			} else {
				fprintf(stderr, "ERROR: Error creating directory %s\n", newdirname);
			}
		} else {
			fprintf(stderr, "ERROR: Directory %s could not be opened\n", newdirname);
		}
		char* newdirnameold = newdirname;
		newdirname = malloc(strlen(newdirnameold) + strlen(lookuph) + 2);
		snprintf(newdirname, strlen(newdirnameold) + strlen(lookuph) + 2, "%s/%s", newdirnameold, lookuph);
		free(newdirnameold);
		lookuph = strtok(NULL, "/");
	}
	free(newdirname);
}

int fileExists(char* filepath) {
	struct stat buffer;
	return (stat(filepath, &buffer) == 0);
}

char* getFData(char* filepath) {
	fprintf(stdout, "INFO: Opening file %s to get file data\n", filepath);
	FILE* fp = fopen(filepath, "r");
	fseek(fp, 0L, SEEK_END);
	int fsize = ftell(fp);
	rewind(fp);
	char* fdata = malloc(fsize + 1);
	fread(fdata, sizeof(char), fsize, fp);
	fdata[fsize] = '\0';
	fprintf(stdout, "INFO: File data written successfully\n");
	return fdata;
}

long getFTime(char* filepath) {
	struct stat attr;
	if (stat(filepath, &attr) != 0) {
		fprintf(stdout, "WARN: File does not exist\n");
		return -1; //the minimum possible version
	}
	fprintf(stdout, "DEBUG: Last modification time: %ld\n", attr.st_mtime);
	return attr.st_mtime;
}

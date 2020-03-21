char* getMyFiles(char* rootpath);

int rAction(char* path, char* fdata);

void createRDir(char* path);

int fileExists(char* filepath);

char* getFData(char* filepath);

// get time of last modification
long getFTime(char* filepath);

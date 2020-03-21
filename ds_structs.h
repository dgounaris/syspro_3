typedef struct SList {
	unsigned short port;
	char* ip;
	struct SList* next;
} SList;

void addSList(SList** sList, unsigned short port, char* ip);

SList* getSListClients(SList* sList, unsigned short port, char* ip);
//todo free the above!!

int removeSList(SList** sList, unsigned short port, char* ip);

void clearSList(SList* sList);



typedef struct CList {
	unsigned short port;
	char* ip;
	struct CList* next;
} CList;

void addCList(CList** cList, unsigned short port, char* ip);

CList* getCListClients(CList* sList, unsigned short port, char* ip);
//todo free the above!!

int existsCList(CList* cList, unsigned short port, char* ip);

void removeCList(CList** cList, unsigned short port, char* ip);

void clearCList(CList* cList);

typedef struct BElem {
	char pathname[128];
	long version;
	char* ip;
	unsigned short port;
} BElem;

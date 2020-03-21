int getSSocket(unsigned short port);

int getCSocket();

struct sockaddr_in* conCToS(int sockId, struct sockaddr* server);

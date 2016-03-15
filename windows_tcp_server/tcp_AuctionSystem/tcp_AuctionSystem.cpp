#include <io.h>
#include <stdio.h>
#include <winsock2.h>
#include <iostream>
#include <Windows.h>
#include <sstream>
#include <string.h>
using namespace std;

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define MAX_CLIENTS 5
#define BUFFER_SIZE 1
#define MSG_SIZE 64
#define ITEMS 15
SOCKET clientSockets[MAX_CLIENTS];
HANDLE clientThreads[MAX_CLIENTS];
HANDLE hMutex;
char password[] = "boss";

int currItems = 0;
int currentClient = 0;


struct item_info {
	char name[MSG_SIZE];
	int id;
	double price;
	int currClient;
};


struct socket_info {
	int id;
	SOCKET socket;
};

struct client_info {
	int id;
	char* logname;
	SOCKET socket;
	int status; // 2 - provider, 1 - customer, 0 - connected,  -1 - disconnected

};

struct packet_structure {
	int reclen;
	char *buf;
};

struct item_info items[ITEMS];
struct client_info clients[MAX_CLIENTS];

/* readn - read exactly n bytes */
int readn(SOCKET fd, char *bp, size_t len)
{
	int cnt;
	int rc;

	cnt = len;
	while (cnt > 0)
	{
		rc = recv(fd, bp, cnt, 0);
		printf("zalupa is %s\n", bp);
		/*@.bp*/
		if (rc < 0)				/* read error? */
		{
			//			if (errno == EINTR)	/* interrupted? */
			//				continue;			/* restart the read */
			return -1;				/* return error */
		}
		if (rc == 0)				/* EOF? */
			return len - cnt;		/* return short count */
		bp += rc;
		cnt -= rc;
	}
	return len;
}

/* readvrec - read a variable record */
int readvrec(SOCKET fd, char *bp, size_t len)
{
	int reclen;
	int rc;

	/* Retrieve the length of the record */

	rc = readn(fd, (char *)&reclen, sizeof(int));
	
	//printf("reclen = %d \n", reclen);
	//puts((char*)&reclen);
	if (rc != sizeof(size_t))
		return rc < 0 ? -1 : 0;
	//reclen = ntohl(reclen);

	if (reclen > len)
	{
		/*
		*  Not enough room for the record--
		*  discard it and return an error.
		*/

		while (reclen > 0)
		{
			rc = readn(fd, bp, len);
			if (rc != len)
				return rc < 0 ? -1 : 0;
			reclen -= len;
			if (reclen < len)
				len = reclen;
		}
		//set_errno(EMSGSIZE);
		return -1;
	}

	/* Retrieve the record itself */

	rc = readn(fd, bp, reclen);
	if (rc != reclen)
		return rc < 0 ? -1 : 0;
	return rc;
}

void enroll(struct client_info *c) {
// 1) получить пароль; 2) проверить полученный с настоящим; 3) из п.2 - присвоить нужный статус; 4) выслать клиенту его статус
	int r;
	char buf_pass[MSG_SIZE];
	SOCKET s = c->socket;
	r = readvrec(s, buf_pass, sizeof(buf_pass));
	if (r < 0) {
		puts("readvrec error");
		exit(1);
	}

	c->status = (!strncmp(buf_pass, password, sizeof(password) - 1)) ? 2 : 1;
	clients[c->id].status = c->status;
	char status[MSG_SIZE];
	itoa(c->status, status, 10);
	if (send(s, status, sizeof(status), 0) < 0) 
		puts("send status error");

	printf("status = %s \n", status);

}

void sendvar(int s, char* buf) {
	struct packet_structure pack;
	pack.buf = buf;
	int n = strlen(pack.buf);
	pack.reclen = n;
	if (send(s, (char*)&pack, n + sizeof(pack.reclen), 0) < 0)
		puts("sendvar error");
}


void sendAll(struct client_info *c) {
	SOCKET s = c->socket;
	char no_items[] = "There are no items.";

	if (currItems == 0)
		if (send(s, no_items, MSG_SIZE, 0) < 0)
			puts("send error");
	
	for (int i = 0; i < currItems; i++) {

		char id[MSG_SIZE];
		char name[MSG_SIZE];
		char price[MSG_SIZE];
		char currClient[MSG_SIZE];

		char divider[MSG_SIZE] = "--------------------------";

		memset(id, '\0', MSG_SIZE);
		memset(name, '\0', MSG_SIZE);
		memset(price, '\0', MSG_SIZE);
		memset(currClient, '\0', MSG_SIZE);

		//for (int j = 0; j < strlen(items[i].name); j++)
		//	name[j] = items[i].name[j];

	
		sprintf(id, "Item's ID -> #%d", items[i].id);
		sprintf(name, "name - %s", items[i].name);
		sprintf(price, "price = %f", items[i].price);
		if (items[i].currClient < 0)
			sprintf(currClient, "current client - none");
		else
			sprintf(currClient, "current client - %d", items[i].currClient);

		if (send(s, id, MSG_SIZE, 0) < 0) // ID
			puts("send item id error");
		if (send(s, name, MSG_SIZE, 0) < 0) // NAME
			puts("send item id error");
		if (send(s, price, MSG_SIZE, 0) < 0) // CURRENT PRICE
			puts("send item price error");
		if (send(s, currClient, MSG_SIZE, 0) < 0) // CURRENT CLIENT
			puts("send item currItem error");
		
		if (send(s, divider, MSG_SIZE, 0) < 0)
			puts("send divider error");
	}
	
}

void raise(struct client_info *c, char *buf) {
	//FORMAT: raise ITEM_ID NEW_PRICE 
	SOCKET s = c->socket;
	char res[MSG_SIZE];
	if (buf == NULL)	return;
	if (strtok(buf, " \n") != NULL) {

		char *id_value = strtok(NULL, " \n");
		if (id_value == NULL) {
			sprintf(res, "Incorrect format. Check out: raise <item_id> <new_price>");
			if (send(s, res, MSG_SIZE, 0) < 0)
				puts("send error");
			return;
		}
		int item_id = (int)strtod(id_value, NULL);

		char *price_value = strtok(NULL, " \n");
		if (price_value == NULL) {
			sprintf(res, "Incorrent item ID or value of price. Try again.");
			if (send(s, res, MSG_SIZE, 0) < 0)
				puts("send error");
			return;
		}

		double price = strtod(price_value, NULL);
		if (price != 0.0 && items[item_id].price < price) {
			items[item_id].price = price;
			items[item_id].currClient = c->id;
			sprintf(res, "Item #%d updated. Price raised up to %f \n", item_id, price);
		}
		else sprintf(res, "Not enough money.");
	}
	else
		sprintf(res, "Incorrect format. Check out: raise <item_id> <new_price>");

	if (send(s, res, MSG_SIZE, 0) < 0)
		puts("send error");
}



void addItem(struct client_info *c, char* buf) {
	//FORMAT: add NAME START_PRICE 
	SOCKET s = c->socket;
	char res[MSG_SIZE];
	if (buf == NULL)	return;
	if (strtok(buf, " \n") != NULL) {
		//char name[MSG_SIZE];
		//sprintf(name, "%s", strtok(NULL, " "));
		char *name = strtok(NULL, " \n");
		if (name == NULL) {
			sprintf(res, "Incorrect format. Check out: add <name> <price>");
			if (send(s, res, MSG_SIZE, 0) < 0)
				puts("send error");
			return;
		}

		char *value = strtok(NULL, " \n");
		if (value == NULL) {
			sprintf(res, "Incorrent value of price. Try again.");
			if (send(s, res, MSG_SIZE, 0) < 0)
				puts("send error");
			return;
		}

		double price = strtod(value, NULL);
		if (price != 0.0 && price > 0) {
			items[currItems].id = currItems;
			for (int i = 0; i < strlen(name); i++)
				items[currItems].name[i] = name[i];
			items[currItems].price = price;
			++currItems;
			sprintf(res, "New item successfully added.");
		}
		else sprintf(res, "Incorrent value of price. Try again.");
	}
	else 
		sprintf(res, "Incorrect format. Check out: add <name> <price>");

	if (send(s, res, MSG_SIZE, 0) < 0)
		puts("send error");
}

void printClients() {
	int cnt = 0;
	for (int i = 0; i < currentClient; i++)
		if (clients[i].socket != -1) {
			printf("Client #%d : IP = %s , socket = %d\n", clients[i].id, clients[i].logname, clients[i].socket);
			cnt++;
		}
	if (cnt == 0)
		puts("There are no connected clients.");
		
}
void kill(int k) {
	if (closesocket(clients[k].socket))
		printf("Error = %d", WSAGetLastError());
	clients[k].socket = -1;
	printf("Client #%d with IP %s was disconnected \n", clients[k].id, clients[k].logname);
	clients[k].logname = "--- --- --- ---";
	
}

void finish() {
	char buf[] = "Auctions is closed!";
	for (int i = 0; i < currentClient; i++) {
		sendAll(&clients[i]);
		if (send(clients[i].socket, buf, sizeof(buf), 0) < 0)
			puts("send error");
		kill(i);
	}
	//for (int i = 0; i < currentClient; i++)
	//	kill(i);
}

void messaging1(struct client_info *c) {
	SOCKET s = c->socket;
	char buf[MSG_SIZE];
	char cmd1[] = "showall";
	char cmd2[] = "raise";
	char error_cmd[] = "Unknown user command, try again.";
	
	while (1) {
		
		memset(buf, '\0', MSG_SIZE);
		

		int r = readvrec(s, buf, sizeof(buf));
		if (r <= 0) {
			puts("readvrec error");
			break;
		}
		else {
			WaitForSingleObject(hMutex, INFINITE);

			if (!strncmp(buf, cmd1, sizeof(cmd1) - 1) )
				sendAll(c);
			else if (!strncmp(buf, cmd2, sizeof(cmd2) - 1) )
				raise(c, buf);
		}

		ReleaseMutex(hMutex);
		
		//memset(buf, '\0', MSG_SIZE);
		//readvrec(s, buf, sizeof(buf));
		//puts(buf);

	//	if (send(s, buf, sizeof(buf), 0) < 0)
		//	puts("send error");
	}
	

}

void messaging2(struct client_info *c) {
	SOCKET s = c->socket;
	char cmd1[] = "add";
	char cmd2[] = "finish";
	char buf[MSG_SIZE];
	
	while (1) {
		memset(buf, '\0', MSG_SIZE);
	

		int r = readvrec(s, buf, MSG_SIZE);
		if (r <= 0) {
			puts("readvrec error");
			break;
		}
		else {
			WaitForSingleObject(hMutex, INFINITE);

			if (!strncmp(buf, cmd1, sizeof(cmd1) - 1))
				addItem(c, buf);
			else if (!strncmp(buf, cmd2, sizeof(cmd2) - 1))
				finish();
		}
		ReleaseMutex(hMutex);
	}
}

void init() {
	for (int i = 0; i < ITEMS; i++) {
		items[i].currClient = -1;
		items[i].price = -1;
		items[i].id = i;
	}
}
DWORD WINAPI clientThread(LPVOID pParam) {
	struct client_info ci = *((struct client_info*)pParam);
	char buf[MSG_SIZE];
	printf("clientThread #%d started\n", ci.id);
	SOCKET s = ci.socket;

	enroll(&ci);
	int status = ci.status;

	if (status == 1)	messaging1(&ci);
	else	messaging2(&ci);
	//messaging1(&ci);

	shutdown(s, SD_BOTH);
	closesocket(s);

	printf("clientThread #%d finished", ci.id);
	return 0;

}

DWORD WINAPI acceptThread(LPVOID pParam) {
	sockaddr_in client_addr;
	int i;
	SOCKET client_socket;
	//SOCKET s = (SOCKET)pParam;
	SOCKET s = *(SOCKET*)pParam;
	int client_addr_size = sizeof(client_addr);

	for (i = 0; i < MAX_CLIENTS; i++) {
		client_socket = accept(s, (struct sockaddr *)&client_addr, &client_addr_size);
		clientSockets[i] = client_socket;
		currentClient++;
		if (client_socket == INVALID_SOCKET) {
			puts("accept error");
			shutdown(s, SD_BOTH);
			closesocket(s);
			break;
		}
		char* client_IP = inet_ntoa(client_addr.sin_addr);
		printf("%s connected \n", client_IP);
		
		struct client_info ci;
		ci.id = i;
		ci.socket = client_socket;
		ci.status = 0;
		ci.logname = client_IP;
		
		clients[i].id = ci.id;
		clients[i].socket = ci.socket;
		clients[i].status = ci.status;
		clients[i].logname = client_IP;
		
		clientThreads[i] = CreateThread(NULL, 0, clientThread, &ci, 0, NULL);

	}

	WaitForMultipleObjects(currentClient, clientThreads, TRUE, INFINITE);

	for (int j = 0; j < currentClient; j++)
		CloseHandle(clientThreads[j]);

	CloseHandle(hMutex);

	return 0;
}


int main(int argc, char *argv[])
{
	printf("size if %d\n", sizeof(size_t));
	WSADATA wsa;
	SOCKET s, new_socket;
	struct sockaddr_in server;
	int c;
	//char buf[1];
	//int cnt = 3;
	int n;
	int curr = 0;
	//Initialize
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		return 1;
	}

	printf("Initialised.\n");

	hMutex = CreateMutex(NULL, FALSE, "mutex");

	//Create socket
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		printf("Could not create socket : %d", WSAGetLastError());
	else
		printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(27011);

	//Bind
	if (bind(s, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
		printf("Bind failed with error code : %d", WSAGetLastError());
	else
		printf("Bind done \n");
	//puts("Bind done");

	//Listen to incoming connections
	listen(s, MAX_CLIENTS);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");

	init();

	HANDLE acceptHandle = CreateThread(NULL, 0, acceptThread, &s, 0, NULL);
	
	char cmd1[] = "print";
	char cmd2[] = "kill";
	char cmd3[] = "quit";
	char buf[MSG_SIZE];
	memset(buf, '\0', MSG_SIZE);
	while (fgets(buf, MSG_SIZE, stdin) != NULL) {
		if (!strncmp(buf, cmd1, sizeof(cmd1) - 1))
			printClients();
		else if (!strncmp(buf, cmd2, sizeof(cmd2) - 1)) {
			strtok(buf, " ");
			int num = (int)strtod(strtok(NULL, " "), NULL);
			kill(num);
		}
		else if (!strncmp(buf, cmd3, sizeof(cmd3) - 1)) {
			for (int i = 0; i < currentClient; i++)
				if (closesocket(clients[i].socket))
					printf("closescoket error - %d", WSAGetLastError());
			getchar();
			exit(1);
		}
	}

	getchar();
	
	closesocket(s);
	WSACleanup();

	return 0;
}



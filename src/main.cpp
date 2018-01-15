#include <arpa/inet.h>
#include <iostream>
#include <mysql.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

typedef struct AUTH
{
	char User[32];
	char Key[256];
} AUTH_T;

void *ConnectionWorker(void*);
int Authenticate(string User, string PasswordHash);

int main(int argc, char *argv[])
{
	cout << "INFO: Running..." << endl;

	int SocketDesc;
	SocketDesc = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in Server, Client;
	Server.sin_family = AF_INET;
	Server.sin_addr.s_addr = INADDR_ANY;
	Server.sin_port = htons(8888);

	cout << "INFO: Attempting to bind port 8888" << endl;
	bind(SocketDesc, (struct sockaddr*)&Server, sizeof(Server));
	listen(SocketDesc, 3);
	cout << "INFO: Successfully bound to port 8888" << endl;

	int IncomingSocket;
	int SocketSize = sizeof(struct sockaddr_in);
	int *ThreadedSocket;

	cout << "INFO: Waiting for connections..." << endl;
	while ((IncomingSocket = accept(SocketDesc, (struct sockaddr*)&Client,
		(socklen_t*)&SocketSize)))
	{
		pthread_t sniffer_thread;

		ThreadedSocket = (int*)malloc(1);
		*ThreadedSocket = IncomingSocket;
		
		pthread_create(&sniffer_thread, NULL, ConnectionWorker,
			(void*)ThreadedSocket);
		cout << "INFO: Connection accepted from " << inet_ntoa(Client.sin_addr) << "NOT authenticated" << endl;
	}

	return 0;
}

void *ConnectionWorker(void *SocketDesc)
{
	int Socket = *(int*)SocketDesc;

	int c;
	char IncomingMessage[512];
	char *Response;

	while ((c = recv(Socket, &IncomingMessage, 512, 0)) > 0)
	{
		int ResponseLength = 0;
		// AUTH <user> <password_hash> EOM
		if (strncmp(&IncomingMessage[0], "AUTH", strlen("AUTH")) == 0)
		{
			int argc = 0;
			for (int i = 0; i < strlen(&IncomingMessage[0]); i++)
			{
				if (IncomingMessage[i] == ' ')
				{
					argc++;
				}
			}

			if (argc != 3)
			{
				Response = "AUTH BAD";
			}
			else
			{
				strtok(&IncomingMessage[0], " ");
				string User(strtok(NULL, " "));
				string PasswordHash(strtok(NULL, " "));

				if (Authenticate(User, PasswordHash))
				{
					Response = "AUTH GOOD";
				}
				else
				{
					Response = "AUTH BAD";
				}
			}

			ResponseLength = strlen(Response);
		}

		if (ResponseLength > 0)
		{
			write(Socket, Response, ResponseLength);
		}

		memset(&IncomingMessage[0], 0, 512);
	}

	if (c == 0)
	{
		fflush(stdout);
	}

	free(SocketDesc);

	return 0;
}

int Authenticate(string User, string PasswordHash)
{
	MYSQL *MysqlConn = mysql_init(NULL);
	MysqlConn = mysql_real_connect(MysqlConn, "localhost",
		"username", "password", "database", 0, NULL, 0);

	string q("SELECT * FROM users WHERE User='");
	q += User + "' AND PassHash='" + PasswordHash + "' LIMIT 1";

	mysql_query(MysqlConn, q.c_str());
	MYSQL_RES *r = mysql_store_result(MysqlConn);
	unsigned int n = mysql_num_rows(r);

	mysql_close(MysqlConn);

	return n;
}

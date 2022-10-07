#include <stdio.h>
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")
#include <sys/types.h>
#include <sys/stat.h>

#define PRINT(str) printf("[%s-%d] "#str" = %s\n", __func__, __LINE__, str);

// ������
void ErrorDie(const char* str)
{
	perror(str);
	exit(1);
}

// ��ʼ�����粢��������˵��׽���
int Startup(unsigned short* port)
{
	// ����ͨ�ŵĳ�ʼ��
	WSADATA data;
	int ret = WSAStartup(MAKEWORD(1, 1), &data);
	if (ret) {
		ErrorDie("����ͨ�ŵĳ�ʼ��ʧ��");
	}

	// �����׽���
	int serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == -1) {
		ErrorDie("�����׽���ʧ��");
	}

	// �����׽�������
	int opt = 1;
	ret = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	if (ret == -1) {
		ErrorDie("�����׽�������ʧ��");
	}

	// ���÷������˵������ַ
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(*port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// ���׽���
	ret = bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (ret < 0) {
		ErrorDie("���׽���ʧ��");
	}

	// ��̬����һ���˿�
	int nameLen = sizeof(serverAddr);
	if (*port == 0) {
		ret = getsockname(serverSocket, (struct sockaddr*)&serverAddr, &nameLen);
		if (ret < 0) {
			ErrorDie("��̬����һ���˿�ʧ��");
		}
		*port = serverAddr.sin_port;
	}

	// ������������
	ret = listen(serverSocket, 5);
	if (ret < 0) {
		ErrorDie("������������ʧ��");
	}

	return serverSocket;
}

// ��ָ���Ŀͻ����׽��ֶ�ȡһ�����ݣ����浽buff��
// ���ض������ٸ��ֽ�
int GetLine(int sock, char* buff, int size)
{
	char c = '\0';
	int i = 0;

	while (i < size - 1 && c != '\n') {
		int n = recv(sock, &c, 1, 0);
		if (n > 0) {
			if (c == '\r') {
				n = recv(sock, &c, 1, MSG_PEEK);
				if (n > 0 && c == '\n') {
					recv(sock, &c, 1, 0);
				}
				else {
					c = '\n';
				}
			}
			buff[i++] = c;
		}
		else {
			c = '\n';
		}
	}

	buff[i] = '\0';
	return 0;
}

// ��ָ�����׽��ַ���һ����ʾû��ʵ�ֵĴ���ҳ��
void Unimplement(int client)
{

}

// �Ҳ���ҳ��
void NotFound(int client)
{
	
}

// ������Ӧ����ͷ��Ϣ
void Headers(int client)
{
	printf("Headers start\n");
	char buff[1024];

	strcpy(buff, "HTTP/1.0 200 OK\r\n");
	send(client, buff, strlen(buff), 0);
	PRINT(buff);

	strcpy(buff, "Server: HllogHttp/0.1\r\n");
	send(client, buff, strlen(buff), 0);
	PRINT(buff);

	strcpy(buff, "Content-type:text/html\n");
	send(client, buff, strlen(buff), 0);
	PRINT(buff);

	strcpy(buff, "\r\n");
	send(client, buff, strlen(buff), 0);
	PRINT(buff);

	printf("Headers end\n");
}

void Cat(int client, FILE* resource)
{
	char buff[4096];
	int count = 0;
	while (1) {
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if (ret <= 0) {
			break;
		}
		send(client, buff, ret, 0);
		count += ret;
	}
	printf("һ��������[%d]���ֽ�\n", count);
}

// ����ҳ��
void ServerFile(int client, const char* fileName)
{
	int numChars = 1;
	char buff[1024];
	while (numChars > 0 && strcmp("\n", buff)) {
		numChars = GetLine(client, buff, sizeof(buff));
		PRINT(buff);
	}

	FILE* resource = nullptr;
	if (strcmp(fileName, "htdocs/index.html") == 0) {
		resource = fopen(fileName, "r");
		printf("open with r\n");
	}
	else {
		resource = fopen(fileName, "rb");
		printf("open with rb\n");
	}
	if (resource == nullptr) {
		NotFound(client);
	}
	else {
		Headers(client);
		Cat(client, resource);
		printf("��Դ������ϣ�\n");
	}	
	
	fclose(resource);
}

// �����û�������̺߳���
DWORD WINAPI AcceptRequest(LPVOID arg)
{
	char buff[1024];
	int client = (SOCKET)arg;

	int numChars = GetLine(client, buff, sizeof(buff));
	PRINT(buff);

	char method[255];
	int j = 0;
	int i = 0;
	while (!isspace(buff[j]) && i < sizeof(method) - 1) {
		method[i] = buff[j];
		i++;
		j++;
	}
	method[i] = '\0';
	PRINT(method);

	// �������ķ��������������Ƿ�֧��
	if (stricmp(method, "GET") && stricmp(method, "POST")) {
		Unimplement(client);
		return 0;
	}

	// ������Դ�ļ���·��
	char url[255];
	i = 0;
	while (isspace(buff[j]) && j < sizeof(buff)) {
		j++;
	}
	while (!isspace(buff[j]) && i < sizeof(url) - 1 && j < sizeof(buff)) {
		url[i] = buff[j];
		i++;
		j++;
	}
	url[i] = '\0';
	PRINT(url);

	char path[512] = "";
	sprintf(path, "htdocs%s", url);
	if (path[strlen(path) - 1] == '/') {
		strcat(path, "index.html");
	}
	PRINT(path);

	struct stat status;
	if (stat(path, &status) == -1) {
		while (numChars > 0 && strcmp("\n", buff)) {
			numChars = GetLine(client, buff, sizeof(buff));
		}
		NotFound(client);
	}
	else {
		if ((status.st_mode & S_IFMT) == S_IFDIR) {
			strcat(path, "/index.html");
		}
		ServerFile(client, path);
	}
	//closesocket(client);

	return 0;
}

int  main()
{
	unsigned short port = 8000;
	int serverSock = Startup(&port);
	printf("http�����Ѿ����������ڼ��� %d �˿�...\n", port);

	struct sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	while (1) {
		// ����ʽ�ȴ��û�����
		int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
		if (clientSock == -1) {
			ErrorDie("����ʽ�ȴ��û�����ʧ��");
		}

		DWORD threadId = 0;
		CreateThread(0, 0, AcceptRequest, (void*)clientSock, 0, &threadId);
	}

	return 0;
}
#include <stdio.h>
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")
#include <sys/types.h>
#include <sys/stat.h>

#define PRINT(str) printf("[%s-%d] "#str" = %s\n", __func__, __LINE__, str);

// 错误处理
void ErrorDie(const char* str)
{
	perror(str);
	exit(1);
}

// 初始化网络并创建服务端的套接字
int Startup(unsigned short* port)
{
	// 网络通信的初始化
	WSADATA data;
	int ret = WSAStartup(MAKEWORD(1, 1), &data);
	if (ret) {
		ErrorDie("网络通信的初始化失败");
	}

	// 创建套接字
	int serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == -1) {
		ErrorDie("创建套接字失败");
	}

	// 设置套接字属性
	int opt = 1;
	ret = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	if (ret == -1) {
		ErrorDie("设置套接字属性失败");
	}

	// 配置服务器端的网络地址
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(*port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 绑定套接字
	ret = bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (ret < 0) {
		ErrorDie("绑定套接字失败");
	}

	// 动态分配一个端口
	int nameLen = sizeof(serverAddr);
	if (*port == 0) {
		ret = getsockname(serverSocket, (struct sockaddr*)&serverAddr, &nameLen);
		if (ret < 0) {
			ErrorDie("动态分配一个端口失败");
		}
		*port = serverAddr.sin_port;
	}

	// 创建监听队列
	ret = listen(serverSocket, 5);
	if (ret < 0) {
		ErrorDie("创建监听队列失败");
	}

	return serverSocket;
}

// 从指定的客户端套接字读取一行数据，保存到buff中
// 返回读到多少个字节
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

// 向指定的套接字发送一个提示没有实现的错误页面
void Unimplement(int client)
{

}

// 找不到页面
void NotFound(int client)
{
	
}

// 发送响应包的头信息
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
	printf("一共发送了[%d]个字节\n", count);
}

// 服务页面
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
		printf("资源发送完毕！\n");
	}	
	
	fclose(resource);
}

// 处理用户请求的线程函数
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

	// 检查请求的方法，本服务器是否支持
	if (stricmp(method, "GET") && stricmp(method, "POST")) {
		Unimplement(client);
		return 0;
	}

	// 解析资源文件的路径
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
	printf("http服务已经启动，正在监听 %d 端口...\n", port);

	struct sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	while (1) {
		// 阻塞式等待用户访问
		int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
		if (clientSock == -1) {
			ErrorDie("阻塞式等待用户访问失败");
		}

		DWORD threadId = 0;
		CreateThread(0, 0, AcceptRequest, (void*)clientSock, 0, &threadId);
	}

	return 0;
}
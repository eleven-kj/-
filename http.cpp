#include<stdio.h>
#include<stdlib.h>
#include<WinSock2.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#pragma comment(lib, "WS2_32.lib")

#define PRINTF(str) printf("[%s - %d]"#str"=%s\n", __func__, __LINE__, str); 

int startup(unsigned short* port)
{
	WSADATA data;
	int ret = WSAStartup(MAKEWORD(1,1), &data);
	if (ret) {
		perror("初始化失败");
		exit(1);
	}
	int server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket == -1)
	{
		perror("socket() failed");
		exit(1);
	}
	int opt = 1, len = sizeof(opt);
	ret = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,(const char *) & opt, len);
	if (ret == -1)
	{
		perror("setsockopt() failed");
		exit(1);
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(*port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("bind() failed");
		exit(1);
	}

	if (listen(server_socket, 5) < 0)
	{
		perror("listen() failed");
		exit(1);
	}
	
	//动态分配端口
	int namelen = sizeof(server_addr);
	if (*port == 0)
	{
		if (getsockname(server_socket, (struct sockaddr*)&server_addr, &namelen) < 0)
		{
			perror("getsockname() failed");
			exit(1);
		}
		*port = server_addr.sin_port;
	}
	return server_socket;
}

int get_line(int sock, char* buff, int size)
{
	char c = 0;
	int i = 0;
	while (i<size-1&&c!='\n')
	{
		int n = recv(sock, &c, 1, 0);
		if (n > 0)
		{
			if (c == '\r')
			{
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
		else
		{
			c = '\n';
		}
	}

	buff[i] = 0;
	return i;
}

void unimplement(int client)
{
	//向指定套接字，发送一个提示还没有实现的错误页面
}

void not_found(int client)
{
	//发送网页不存在  发送404响应
}

void headers(int client)
{
	//发送响应包的头信息
	char buff[1024];

	strcpy(buff, "HTTP/1.0 200 OK\r\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Server: ZJH/0.1\r\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Content-type:text/html\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "\r\n");
	send(client, buff, strlen(buff), 0);
}

void cat(int client, FILE* resource)
{
	char buff[4096];
	int cnt = 0;

	while (true)
	{
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if (ret <= 0)
		{
			break;
		}
		send(client, buff, ret, 0);
		cnt += ret;
	}
	printf("一共发送cnt=%d\n", cnt);
}

void server_file(int client, const char* filename)
{
	int numchars = 1;
	char buff[1024];
	while (numchars > 0 && strcmp(buff, "\n"))
	{
		numchars = get_line(client, buff, sizeof(buff));
		PRINTF(buff);
	}
	FILE* resource = NULL;
	if (strcmp(filename,"html作品/index.html")==0)
	{
		resource = fopen(filename, "r");
	}
	else {
		resource = fopen(filename, "rb");
	}
	if (resource == NULL) {
		not_found(client);
	}
	else
	{
		//发送资源给浏览器
		headers(client);

		//发送请求资源信息
		cat(client, resource);

		printf("资源发送完毕！\n");
	}

	fclose(resource);
}

//处理用户请求的线程函数
DWORD WINAPI accept_request(LPVOID arg)
{
	char buff[1024];

	int client = (SOCKET)arg;//客户端套接字


	//读取一行数据
	int numchars = get_line(client, buff, sizeof(buff));
	PRINTF(buff);//哪个函数在哪一行打印了什么

	char method[255];
	int j = 0,i = 0;
	while (!isspace(buff[j])&&i<sizeof(method)-1)
	{
		method[i++] = buff[j++];
	}
	method[i] = 0;  //字符串结束 '\0'
	PRINTF(method);

	//检查请求的方法，本服务器是否支持
	if (stricmp(method, "GET") && stricmp(method, "POST"))
	{
		//向浏览器返回一个错误提示页面
		unimplement(client);
		return 0;
	}


	//GET /abc/index.html HTTP/1.1\n
	//解析资源文件路径
	char url[255];
	i = 0;
	while (isspace(buff[j]) && j < sizeof(buff))
	{
		j++;
	}
	while (!isspace(buff[j]) && i < sizeof(url) - 1 && j < sizeof(buff))
	{
		url[i++] = buff[j++];
	}
	url[i] = 0;
	PRINTF(url);

	char path[512] = "";
	sprintf(path, "html作品%s", url);
	if (path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}
	PRINTF(path);

	struct stat status;
	if (stat(path, &status) == -1)
	{
		//请求报的剩余数据读取完毕
		while (numchars > 0 && strcmp(buff, "\n"))
		{
			numchars = get_line(client, buff, sizeof(buff));
		}
		not_found(client);
	}
	else {
		if ((status.st_mode & S_IFMT) == S_IFDIR)
		{
			strcat(path, "/index.html");
		}

		server_file(client, path);
	}

	closesocket(client);

	return 0;
}

int main(void)
{
	unsigned short port = 8000;
	int server_sock = startup(&port);
	printf("http服务已经启动，正在监听 %d 端口...", port);


	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);
	while (true)
	{
		int client_sock = accept(server_sock, (sockaddr*)&client_addr, &client_addr_len);
		if (client_sock == -1)
		{
			perror("accpet() failed");
			exit(1);
		}

		DWORD threadId = 0;
		CreateThread(0, 0, accept_request, (void*)client_sock, 0, &threadId);
	}

	return 0;
}
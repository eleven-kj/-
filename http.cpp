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
		perror("��ʼ��ʧ��");
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
	
	//��̬����˿�
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
	//��ָ���׽��֣�����һ����ʾ��û��ʵ�ֵĴ���ҳ��
}

void not_found(int client)
{
	//������ҳ������  ����404��Ӧ
}

void headers(int client)
{
	//������Ӧ����ͷ��Ϣ
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
	printf("һ������cnt=%d\n", cnt);
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
	if (strcmp(filename,"html��Ʒ/index.html")==0)
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
		//������Դ�������
		headers(client);

		//����������Դ��Ϣ
		cat(client, resource);

		printf("��Դ������ϣ�\n");
	}

	fclose(resource);
}

//�����û�������̺߳���
DWORD WINAPI accept_request(LPVOID arg)
{
	char buff[1024];

	int client = (SOCKET)arg;//�ͻ����׽���


	//��ȡһ������
	int numchars = get_line(client, buff, sizeof(buff));
	PRINTF(buff);//�ĸ���������һ�д�ӡ��ʲô

	char method[255];
	int j = 0,i = 0;
	while (!isspace(buff[j])&&i<sizeof(method)-1)
	{
		method[i++] = buff[j++];
	}
	method[i] = 0;  //�ַ������� '\0'
	PRINTF(method);

	//�������ķ��������������Ƿ�֧��
	if (stricmp(method, "GET") && stricmp(method, "POST"))
	{
		//�����������һ��������ʾҳ��
		unimplement(client);
		return 0;
	}


	//GET /abc/index.html HTTP/1.1\n
	//������Դ�ļ�·��
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
	sprintf(path, "html��Ʒ%s", url);
	if (path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}
	PRINTF(path);

	struct stat status;
	if (stat(path, &status) == -1)
	{
		//���󱨵�ʣ�����ݶ�ȡ���
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
	printf("http�����Ѿ����������ڼ��� %d �˿�...", port);


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
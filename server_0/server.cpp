# include <stdio.h>
# include <stdlib.h>
# include <io.h>
# include <errno.h>
# include <string.h>
# include <sys/types.h>
# include <winsock2.h>
# include <signal.h>
# include <stdint.h>
# include "mh5.h"
# include <regex>
# include <iostream>
# include <fstream>
# include <thread>
# include <windows.h>
# include <ws2tcpip.h>

# pragma comment (lib, "ws2_32.lib")

#define PORT "17777"  // порт, на который будут приходить соединения 

#define BACKLOG 10     // как много может быть ожидающих соединений

#define MAXDATASIZE 64

#define HEADERSIZE 4

#define HASHSIZE 16

bool CheckMsg(char *msg, size_t msglen) {
    bool flag = true;
    uint32_t *arr = md5(msg, msglen);
    size_t k = 1;
    for (int i = 3; i >= 0; i--) {
        uint8_t *q = (uint8_t *) &arr[i];
        for (int j = 3; j >= 0; j--) {
            if ((msg[msglen + HASHSIZE - k] + 256) % 256 != q[j]) {
                flag = false;
                break;
            }
            k++;
        }
    }
    return flag;
}

void ListeningSocket(int new_fd) {
	std::string output;
    char headBuf[HEADERSIZE + HASHSIZE];
    char buf[MAXDATASIZE + HASHSIZE];
    memset(headBuf, 0, HEADERSIZE + HASHSIZE);
    int packages = 0;
	char flag_file_format[1];
	recv(new_fd, flag_file_format, 1, 0);
	if (flag_file_format[0] == '2') {
		printf("client disconnected\n");
		printf("server: waiting for connections...\n");
		return;
	}
    while (true) {
        recv(new_fd, headBuf, HEADERSIZE + HASHSIZE, 0);
        std::string head(headBuf);
        packages = atoi(head.substr(0, HEADERSIZE - 1).c_str());
		std::cout<<packages;
        if (CheckMsg(headBuf, HEADERSIZE)) {
            std::cout << "Packages: " << packages << std::endl;
            send(new_fd, "1", 1, 0);
            break;
        } else {
            if (send(new_fd, "0", 1, 0) == -1)
                perror("send");
        }
    }
    for (int i = 0; i < packages; i++) {
        while (true) {
            memset(headBuf, 0, HEADERSIZE + HASHSIZE);
            if (recv(new_fd, headBuf, HEADERSIZE + HASHSIZE, 0) == -1) {
                perror("recv");
            }
            if (CheckMsg(headBuf, HEADERSIZE)) {
                if (send(new_fd, "1", 1, 0) == -1)
                    perror("send");
                break;
            } else {
                if (send(new_fd, "0", 1, 0) == -1)
                    perror("send");
            }
        }
        int length = atoi(headBuf);
        std::cout << "Package length: " << length << std::endl;
        while (true) {
            memset(buf, 0, MAXDATASIZE + HASHSIZE);
            if (recv(new_fd, buf, length + HASHSIZE, 0) == -1) { 
                perror("recv");
                break;
            }
            if (CheckMsg(buf, length)) { 
                if (send(new_fd, "1", 1, 0) == -1) 
					perror("send");
				Sleep(500);
			
                break;

            } else { 
                if (send(new_fd, "0", 1, 0) == -1)
                    perror("send");
				
            }
        }
        std::cout << "Package: ";
        for (int i = 0; i < length; i++) {
            std::cout << buf[i];
			output.push_back(buf[i]);
        }
        std::cout << std::endl;
    }
	
	if (flag_file_format[0] == '0') {
	std::ofstream outfile;
	outfile.open("output.txt", std::ios::out);
	outfile << output.c_str();
	}
	else {
	std::ofstream outfile;
	outfile.open("output_binary.png", std::ios::binary | std::ios::out);
	outfile.write(output.c_str(), output.length());
	}
    closesocket(new_fd);
	
}

// получаем адрес сокета, ipv4 или ipv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int main(void) {
	const char bOptVal = TRUE;
	int bOptLen = sizeof(BOOL);
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
	{
		std::cout << "WSAStartup faild. Error:" << WSAGetLastError();
		return FALSE;
	}
    int sockfd, new_fd;  // слушаем на sock_fd, новые соединения - на new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // информация об адресе клиента
    socklen_t sin_size;
   
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %sn", gai_strerror(rv));
		
        return 1;
    }
    // смотрим все результаты, чтобы забиндиться на первом возможном
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

   		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &bOptVal,
				       bOptLen) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            closesocket(sockfd);
            perror("server: bind");
            continue;
        }
		
        break;
    }

    if (!p) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); 

    if (listen(sockfd, BACKLOG) == -1) { // ждем подключения
        perror("listen");
		//WSACleanup();
        exit(1);
    }

   
    printf("server: waiting for connections...\n");

    while (true) {
        sin_size = sizeof their_addr;
        if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);
        
			std::thread thread(ListeningSocket, new_fd);
			thread.detach();
        
    }
	WSACleanup();
    return 0;
}


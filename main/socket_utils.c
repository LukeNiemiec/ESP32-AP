#include "lwip/sockets.h"

// creates and returns a server socket
int create_server_socket(int port) {

	struct sockaddr_in server_addr;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	
	int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

	if(server_sock < 0) {
		return 0;
	} else {
		bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr));
	}

	
	return server_sock;	
}

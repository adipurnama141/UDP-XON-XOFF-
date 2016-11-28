#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSIZE 2048

int main (int argc , char **argv){
	struct sockaddr_in receiver_address;
	struct sockaddr_in sender_address;
	socklen_t addrlen = sizeof(sender_address);
	int recvlen;
	int receiver_socket;
	int msgcount = 0;
	unsigned char buf[BUFSIZE];


	//Buat sebuah socket
	receiver_socket = socket(AF_INET, SOCK_DGRAM , 0);
	if (receiver_socket == -1) {
		printf("Socket gagal dibuat.\n");
	}
	else {
		printf("Socket berhasil dibuat.");
	}

	//Susun alamat internet kita
	memset((char *)&receiver_address , 0 , sizeof(receiver_address));				//Clear memory sender_address jadi nol
	receiver_address.sin_family = AF_INET;										//Setting AF_INET (IPv4)
	receiver_address.sin_addr.s_addr = htonl(INADDR_ANY);							//IP address local (0)
	receiver_address.sin_port = htons(8081);

	//bind socket ke address kita
	if(bind(receiver_socket, (struct sockaddr *)&receiver_address , sizeof(receiver_address) ) <0) {
		perror("bind failed");
		return 0;
	}	

	for(;;) {
		printf("Waiting on port %d\n", 999);
		recvlen = recvfrom(receiver_socket , buf , BUFSIZE , 0 , (struct sockaddr *)&sender_address , &addrlen);
		if (recvlen > 0){
			buf[recvlen] = 0;
			printf("Received message: \"%s\" (%d bytes) \n", buf, recvlen );
		}
		else {
			printf("Something went wrong...\n");
		}

	}




}

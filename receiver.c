#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <pthread.h>


#define CONSUMETIME 500 	//Delay buffer consumption
#define	BUFFSIZE 8 			//Buffer size
#define UPLIMIT 6			//Upper Limit
#define LOWLIMIT 2			//Lower Limit

#define XON (0x11)
#define XOFF (0x13)	

int isXON = 1;
static char buffer[BUFFSIZE];
static int bufferCount = 0;
int receiver_socket;
struct sockaddr_in receiver;
struct sockaddr_in sender;
socklen_t addrlen = sizeof(receiver);
int recvlen;
char buf[BUFFSIZE];

int isEmpty(){
	return (bufferCount == 0);
}

void add(char newentry){

	buffer[bufferCount] = newentry;
	bufferCount = bufferCount + 1;
}

static void consume(){
	static int bufferCount;
	if (!isEmpty()){
		bufferCount = bufferCount - 1;
	}
}

void *thread_func(){
	for (;;) { 
		if (bufferCount > 0){
			bufferCount = bufferCount - 1;
		}
		printf("1 Buffer Consumed (%d left)\n" , bufferCount);
		if ((bufferCount < LOWLIMIT) && (!isXON)) {
			memset((char *)&buf , 0 , sizeof(buf));
			buf[0] = XON;
			printf("Mengirim XON\n");
			if (sendto(receiver_socket, buf, strlen(buf), 0, (struct sockaddr *)&sender, addrlen) < 0) {
				perror("sendto");
			}
			isXON = 1;
		}
		sleep(2);
	}
}


int main(){
	

	//Buat Soket
	receiver_socket = socket(AF_INET, SOCK_DGRAM , 0);
	if (receiver_socket == -1) {
		printf("Socket gagal dibuat.\n");
	}
	else {
		printf("Socket berhasil dibuat.");
	}


	//Susun alamat internet kita
	memset((char *)&receiver , 0 , sizeof(receiver));				//Clear memory sender_address jadi nol
	receiver.sin_family = AF_INET;									//Setting AF_INET (IPv4)
	receiver.sin_addr.s_addr = htonl(INADDR_ANY);					//IP address local (0)
	receiver.sin_port = htons(8081);

	//Binding Soket
	if(bind(receiver_socket, (struct sockaddr *)&receiver , sizeof(receiver) ) <0) {
		perror("bind failed");
		return 0;
	}

	pthread_t pth;

	//do receive
	pthread_create(&pth,NULL,thread_func,"consumer");
	for (;;){
		recvlen = recvfrom(receiver_socket , buf , BUFFSIZE , 0 , (struct sockaddr *)&sender , &addrlen);
		if (recvlen > 0){
			printf("Received message: \"%s\" \n", buf);
			add(buf[1]);
			if (bufferCount > UPLIMIT) {
				memset((char *)&buf , 0 , sizeof(buf));
				buf[0] = XOFF;
				printf("Mengirim XOFF\n");
				if (sendto(receiver_socket, buf, strlen(buf), 0, (struct sockaddr *)&sender, addrlen) < 0) {
					perror("sendto");
				}
				isXON = 0;
			} 			
		}
		

	}
	
		
	


	//Receiving




}
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#define BUFSIZE 2048
#define XON (0x11)
#define XOFF (0x13)

int port;
int isXON;
int tsocket;
int pid;
int recvlen;
int filelen;
char * ip_address;
char * filename;
struct sockaddr_in receiver;
int receiverlen;
unsigned int buf[BUFSIZE];
char buf2[BUFSIZE];
FILE *fileptr;
char * buffer;



void *thread_func(){
	for(;;){
				recvlen = recvfrom(tsocket , buf , BUFSIZE , 0 , (struct sockaddr *)&receiver , &receiverlen);
				if (recvlen > 0){
					if( buf[0] == XON){
						isXON = 1;
					}
					else if  (buf[0] == XOFF) {
						isXON = 0;
					}
				}
	}
}



int main(int argc , char *argv[]){
	if (argc < 4) {
		printf("Usage : ./transmit ip_address port filename\n");
	}
	else {
		


		//Baca input parameter
		ip_address = argv[1];
		port = atoi(argv[2]);
		filename = argv[3];

		//Set status jadi XON
		isXON = 1;

		//Membuat Socket
		tsocket = socket(AF_INET, SOCK_DGRAM , 0);
		if (tsocket == -1){
			perror("Socket creation failed");
		}
		else {
			printf("Socket berhasil dibuat\n");
		}

		//Buat struct alamat penerima
		memset((char *)&receiver , 0 , sizeof(receiver));
		receiver.sin_family = AF_INET;
		receiver.sin_port = htons(port);
		if (inet_aton(ip_address , &receiver.sin_addr)==0){
			fprintf(stderr, "inet_aton() failed \n");
			exit(1);
		}
		receiverlen = sizeof(receiver);

		

		//parent
		pthread_t pth;
		pthread_create(&pth,NULL,thread_func,"xonxoffguard");

		fileptr = fopen(filename, "rb");
		fseek(fileptr,0,SEEK_END);
		filelen = ftell(fileptr);
		rewind(fileptr);
		buffer = (char *) malloc( (filelen+1) * sizeof(char));
		fread(buffer, filelen, 1 , fileptr);

		int i = 0;
		while ( i < filelen) {
			if(isXON == 1){
				sprintf(buf2, "%c" , buffer[i]);
				printf("Mengirim %s\n", buf2);
				if (sendto(tsocket, buf2, strlen(buf2) , 0 , (struct sockaddr *)&receiver , receiverlen) == -1){
					perror("Send to");
					exit(1);
				}
				i++;
			}
			else {
				printf("Menunggu XON..~\n");
			}
			sleep(1);
		}
		
		close(tsocket);
		fclose(fileptr);

	}
}










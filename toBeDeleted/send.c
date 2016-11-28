#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>

#define BUFLEN 2048
#define MSGS 5

int main(void)
{
	struct sockaddr_in sender_address;			//Struct alamat internet pengirim (kita)
	struct sockaddr_in receiver_address;		//Struct alamat internet penerima
	int slen = sizeof(receiver_address);		//Ukuran struct alamat internet penerima
	int sender_socket;							//Socket pengirim
	int i;
	char buf[BUFLEN];							//message buffer
	char *server = "127.0.0.1";					//String , IP tujuan
	int recvlen;			

	FILE *fileptr;
	char *buffer;
	long filelen;
	int j;
	
	fileptr = fopen("a.png","rb");
	fseek(fileptr,0,SEEK_END);
	filelen = ftell(fileptr);
	rewind(fileptr);
	buffer = (char *) malloc( (filelen+1) * sizeof(char));

	fread(buffer, filelen, 1 , fileptr);
	printf("%c\n",buffer[0]);


	//Buat sebuah socket
	sender_socket = socket(AF_INET, SOCK_DGRAM , 0);
	if (sender_socket == -1) {
		printf("Socket gagal dibuat.\n");
	}
	else {
		printf("Socket berhasil dibuat.");
	}

	//Susun address kita
	memset((char *)&sender_address , 0 , sizeof(sender_address));				//Clear memory sender_address jadi nol
	sender_address.sin_family = AF_INET;										//Setting AF_INET (IPv4)
	sender_address.sin_addr.s_addr = htonl(INADDR_ANY);							//IP address local (0)
	sender_address.sin_port = htons(9090);										//Port kita

	//bind socket ke address kita
	if(bind(sender_socket, (struct sockaddr *)&sender_address , sizeof(sender_address) ) <0) {
		perror("bind failed");
		return 0;
	}




	//Susun address tujuan
	memset((char *)&receiver_address ,0 , sizeof(receiver_address));
	receiver_address.sin_family = AF_INET;
	receiver_address.sin_port = htons(8081);
	if (inet_aton(server , &receiver_address.sin_addr)==0){
		fprintf(stderr, "inet_aton() failed \n");
		exit(1);
	}

	//Kirim pesan
	for(i=0; i <filelen ; i++) {
				//variabel buf berisi string "This is Packet"
		sprintf(buf , "%c", buffer[i]);
		printf("%s | packet no %d to %s port %d\n", buf , i, server , 999);


		//kirim ke address tujuan (localhost, port 999)									
		if (sendto(sender_socket,buf,strlen(buf),0, (struct sockaddr *)&receiver_address , slen)== -1){
			perror("send to");
			exit(1);
		}
	}

	close(sender_socket);
	return 0;

}

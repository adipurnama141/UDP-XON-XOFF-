#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFSIZE 2048
#define XON 0x11
#define XOFF 0x13
#define EndFile 0x1a

int isXON = 1;
int lenSleep = 250 * 1000;
int port;
int tsocket;
int pid;
int recvlen;
int filelen;
int receiverlen;
unsigned int buf[BUFSIZE];
char buf2[BUFSIZE];
char* ip_address;
char* filename;
char* buffer;
FILE* fileptr;

struct sockaddr_in receiver;

void *thread_func() {
    for (;;) {
        recvlen = recvfrom(tsocket, buf, BUFSIZE, 0, (struct sockaddr*)& receiver, &receiverlen);
        if (recvlen > 0) {
            if (buf[0] == XON) {
                isXON = 1;
            } else if (buf[0] == XOFF) {
                isXON = 0;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc == 4) {
        //baca input parameter
        ip_address = argv[1];
        port = atoi(argv[2]);
        filename = argv[3];
        //membuat Socket
        tsocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (tsocket == -1) {
            printf("Socket creation failed\n");
            exit(1);
        } else {
            printf("Socket berhasil dibuat\n");
        }
        //buat struct alamat penerima
        memset((char*)& receiver, 0, sizeof(receiver));
        receiver.sin_family = AF_INET;
        receiver.sin_port = htons(port);
        if (inet_aton(ip_address, &receiver.sin_addr) == 0) {
        	printf("inet_aton() failed\n");
            exit(1);
        }
        receiverlen = sizeof(receiver);
        //parent
        pthread_t pth;
        pthread_create(&pth, NULL, thread_func, "XonXoffGuard");
        //data yang dikirim
        fileptr = fopen(filename, "rb");
        fseek(fileptr, 0, SEEK_END);
        filelen = ftell(fileptr);
        rewind(fileptr);
        buffer = (char*) malloc((filelen + 2) * sizeof(char));
        fread(buffer, filelen, 1, fileptr);
        buffer[filelen + 1] = EndFile;
        //kirim
        int i = 0;
        while (i < filelen) {
            if (isXON == 1) {
                sprintf(buf2, "%c" , buffer[i]);
                printf("Mengirim %s\n", buf2);
                if (sendto(tsocket, buf2, strlen(buf2), 0, (struct sockaddr*)& receiver, receiverlen) == -1) {
                    printf("sendto() failed\n");
                    exit(1);
                }
                i++;
            } else {
                printf("Menunggu XON..~\n");
            }
            usleep(lenSleep);
        }
        //selesai
        close(tsocket);
        fclose(fileptr);
    } else {
    	printf("Usage : ./transmitter ip_address port filename\n");
    	exit(1);
    }
    return 0;
}

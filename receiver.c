#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define CONSUMETIME 500 //Delay buffer consumption
#define BUFFSIZE 8 //Buffer size
#define UPLIMIT 6 //Upper Limit
#define LOWLIMIT 2 //Lower Limit
#define XON 0x11
#define XOFF 0x13
#define EndFile 0x1a

static char buffer[BUFFSIZE];
static int bufferCount = 0;
int isXON = 1;
int lenSleep = 500 * 1000;
int receiver_socket;
int recvlen;
char buf[BUFFSIZE];

struct sockaddr_in receiver;
struct sockaddr_in sender;
socklen_t addrlen = sizeof(receiver);

int isEmpty() {
    return bufferCount == 0 ? 1 : 0;
}

void add(char newentry) {
    buffer[bufferCount++] = newentry;
}

static void consume() {
    if (isEmpty() == 1) {
        bufferCount--;
    }
}

void *thread_func() {
    for (;;) {
        if (bufferCount > 0) {
            bufferCount--;
        }
        printf("1 Buffer Consumed (%d left)\n" , bufferCount);
        if ((bufferCount < LOWLIMIT) && (isXON == 0)) {
            memset((char*)& buf, 0, sizeof(buf));
            buf[0] = XON;
            printf("Mengirim XON\n");
            if (sendto(receiver_socket, buf, strlen(buf), 0, (struct sockaddr*)& sender, addrlen) < 0) {
                perror("sendto() failed\n");
                exit(1);
            }
            isXON = 1;
        }
        usleep(lenSleep);
    }
}

int main() {
    //buat Soket
    receiver_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver_socket == -1) {
        printf("Socket creation failed\n");
        exit(1);
    } else {
        printf("Socket berhasil dibuat\n");
    }
    //susun alamat internet kita
    memset((char*)& receiver, 0, sizeof(receiver)); //clear memory sender_address jadi nol
    receiver.sin_family = AF_INET; //Setting AF_INET (IPv4)
    receiver.sin_addr.s_addr = htonl(INADDR_ANY); //IP address local (0)
    receiver.sin_port = htons(8081);
    //binding Soket
    if(bind(receiver_socket, (struct sockaddr*)& receiver, sizeof(receiver)) < 0) {
        perror("bind() failed\n");
        exit(1);
    }
    //do receive
    pthread_t pth;
    pthread_create(&pth, NULL, thread_func, "consumer");
    for (;;) {
        recvlen = recvfrom(receiver_socket, buf, BUFFSIZE, 0, (struct sockaddr*)& sender, &addrlen);
        if (recvlen > 0) {
            printf("Received message: \"%s\"\n", buf);
            add(buf[1]);
            if (bufferCount > UPLIMIT) {
                memset((char*)& buf, 0, sizeof(buf));
                buf[0] = XOFF;
                printf("Mengirim XOFF\n");
                if (sendto(receiver_socket, buf, strlen(buf), 0, (struct sockaddr*)& sender, addrlen) < 0) {
                    perror("sendto() failed\n");
                    exit(1);
                }
                isXON = 0;
            }             
        }
    }
}

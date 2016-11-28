#include "connect.h"
#include <net/if.h>
#include <sys/ioctl.h>

#define BUFFSIZE 8
#define UPLIMIT 6
#define LOWLIMIT 2

int isON = 1; //status receiver ON
int sock; //socket
int headQueue = 0; //antrian pertama siap dikonsum
int tailQueue = 0; //space kosong setelah antrian terakhir
char queue[BUFFSIZE]; //antrian karakter sirkular

struct ifreq ifr; //gatau, dapet dari internet
struct sockaddr_in myaddr; //address program ini sebaga receiver
struct sockaddr_in trnsmtaddr; //address transmitter
socklen_t myaddrlen = sizeof(myaddr); //ukuran socket ini

int isEmpty() {
    return headQueue == tailQueue ? 1 : 0;
}

int isLower() {
    if (tailQueue >= headQueue) {
        return tailQueue - headQueue < LOWLIMIT ? 1 : 0;
    } else {
        return tailQueue + BUFFSIZE - headQueue < LOWLIMIT ? 1 : 0;
    }
}

int isUpper() {
    if (tailQueue >= headQueue) {
        return tailQueue - headQueue > UPLIMIT ? 1 : 0;
    } else {
        return tailQueue + BUFFSIZE - headQueue > UPLIMIT ? 1 : 0;
    }
}

void add(char c) {
    queue[tailQueue++] = c;
    tailQueue %= BUFFSIZE;
}

char delete() {
    char hasil = queue[headQueue++];
    headQueue %= BUFFSIZE;
    return hasil;
}

void sendX(char buffX) {
    //kirim 1 karakter
    if (sendto(sock, &buffX, 1, 0, (struct sockaddr*)& trnsmtaddr, myaddrlen) < 0) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

void *q_get() {
    int countByte = 0; //counter karakter
    int lenSleep = 100000; //jeda saat mengkonsumsi data, 100 ms
    //usleep(lenSleep);
    sleep(2);
    //loop selama antrian masih ada
    while (isEmpty() == 0) {
        //usleep(lenSleep);
        printf("Mengkonsumsi byte ke-%d: '%c'\n", ++countByte, delete());
        if (isLower() && isON == 0) { //transmitter bisa lanjutin
            printf("Buffer < lowerlimit\n");
            printf("Mengirim XON.\n");
            sendX(XON);
            isON = 1;
        }
        sleep(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            printf("socket() failed.\n");
            exit(1);
        }
        //susun alamat internet kita
        memset((char*)& myaddr, 0, myaddrlen); //clear memory jadi nol
        myaddr.sin_family = AF_INET; //setting IPv4
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY); //IP address local
        myaddr.sin_port = htons(atoi(argv[1]));
        //detect ip wireless, http://stackoverflow.com/questions/2283494/get-ip-address-of-an-interface-on-linux
        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ - 1);
        ioctl(sock, SIOCGIFADDR, &ifr);
        printf("Binding pada %s:%s ...\n", inet_ntoa(((struct sockaddr_in*)& ifr.ifr_addr) -> sin_addr), argv[1]);
        if(bind(sock, (struct sockaddr*)& myaddr, myaddrlen) < 0) {
            printf("bind() failed.\n");
            exit(1);
        }
        //receiving
        pthread_t pth;
        int countByte = 0; //counter karakter
        char buff; //tempat menampung karakter yang diterima
        //loop sampai break di ENDFILE
        while (1) {
            //menerima 1 karakter
            if (recvfrom(sock, &buff, 1, 0, (struct sockaddr*)& trnsmtaddr, &myaddrlen) > 0) {
                if (buff == STARTFILE) {
                    pthread_create(&pth, NULL, q_get, "q_get"); //jalankan thread anak
                } else if (buff == ENDFILE) {
                    pthread_join(pth, NULL); //tunggu thread anak selesai
                    break;
                } else {
                    printf("Menerima byte ke-%d.\n", ++countByte);
                    add(buff);
                    if (isUpper()) { //transmit menunda pengiriman
                        printf("Buffer > minimum upperlimit\n");
                        printf("Mengirim XOFF.\n");
                        sendX(XOFF);
                        isON = 0;
                    }
                }
            }
        }
        close(sock);
    } else { //arg invalid
        printf("Usage : ./receiver port\n");
        exit(1);
    }
    return 0;
}

#include "connect.h"

int isON = 1; //status receiver ON
int sock; //socket
struct sockaddr_in recvaddr; //address receiver
int recvlen = sizeof(recvaddr); //ukuran socket receiver

void *xHandler() {
    char buffX; //karakter XON XOFF
    //loop sampai terminate
    while (1) {
        //terima 1 karakter (XON atau XOFF)
        if (recvfrom(sock, &buffX, 1, 0, (struct sockaddr*)& recvaddr, &recvlen) > 0) {
            if (buffX == XON) {
                printf("XON diterima.\n");
                isON = 1;
            } else if (buffX == XOFF) {
                printf("XOFF diterima.\n");
                isON = 0;
            }
        }
    }
}

void sendChar(char buff) {
    //kirim 1 karakter
    if (sendto(sock, &buff, 1, 0, (struct sockaddr*)& recvaddr, recvlen) == -1) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc == 4) {
        printf("Membuat socket untuk koneksi ke %s:%s ...\n", argv[1], argv[2]);
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            printf("socket() failed.\n");
            exit(1);
        }
        //setting address receiver
        memset((char*)& recvaddr, 0, recvlen); //zero-ing
        recvaddr.sin_family = AF_INET;
        recvaddr.sin_port = htons(atoi(argv[2]));
        if (inet_aton(argv[1], &recvaddr.sin_addr) == 0) {
            printf("inet_aton() failed.\n");
            exit(1);
        }
        //akses file yang akan dikirim
        FILE* file = fopen(argv[3], "rb");
        fseek(file, 0, SEEK_END);
        int filelen = ftell(file); //besar file
        rewind(file);
        char* buffFile = (char*) malloc(filelen + 1); //file dipindah ke memori
        fread(buffFile, filelen, 1, file);
        fclose(file);
        int idxFile = 0; //index byte file
        //jalankan xon xoff handler
        pthread_t pth;
        pthread_create(&pth, NULL, xHandler, "xHandler");
        //kirim data file
        sendChar(STARTFILE);
        while (idxFile < filelen) {
            if (isON == 1) {
                printf("Mengirim byte ke-%d: '%c'\n", idxFile + 1, buffFile[idxFile]);
                sendChar(buffFile[idxFile++]);
            } else { //receiver lagi OFF
                printf("Menunggu XON...\n");
            }
            usleep(50000); //50 ms
        }
        sendChar(ENDFILE);
        //selesai
        pthread_cancel(pth); //terminate thread anak
        close(sock);
    } else { //arg invalid
        printf("Usage : ./transmitter ip_address port filename\n");
        exit(1);
    }
    return 0;
}

#include "array.c"
#include <net/if.h>
#include <sys/ioctl.h>

#define BUFFSIZE 256
#define UPLIMIT 254
#define LOWLIMIT 2

int isON = 1; //status receiver ON
/*socket*/
int sock; //socket
struct ifreq ifr; //gatau, dapet dari internet
struct sockaddr_in myaddr; //address program ini sebaga receiver
struct sockaddr_in trnsmtaddr; //address transmitter
socklen_t myaddrlen = sizeof(myaddr); //ukuran socket ini
/*buffer*/
char queue[BUFFSIZE]; //antrian karakter sirkular
int headQueue = 0; //antrian pertama siap dikonsum
int tailQueue = 0; //space kosong setelah antrian terakhir
/*array*/
Array receivedBytes;
ArrayFrame receivedFrames;
int isFrameComplete = 0;
int processedIncomingBytes;

unsigned char countChecksum(unsigned char* ptr, size_t sz) {
    unsigned char chk = 0;
    while (sz-- != 0) {
        chk -= *ptr++;
    }
    ///*debug*/printf("[COUNTCHECKSUM] %x\n", chk);
    return chk;
}

unsigned char getByte() {
    while(processedIncomingBytes >= receivedBytes.used); //wait until new incoming byte
    unsigned char result = receivedBytes.data[processedIncomingBytes++];
    ///*debug*/printf("[GETBYTE] %x\n", result);
    return result;
}

void sendByte(char buffX) {
    ///*debug*/printf("[SENDBYTE] %x\n", buffX);
    if (sendto(sock, &buffX, 1, 0, (struct sockaddr*)& trnsmtaddr, myaddrlen) < 0) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

void sendACK(int fnum, unsigned char nack) {
    //build ACK packet
    unsigned char ackpacket[6];
    ackpacket[0] = nack; //nack bernilai ACK atau NAK
    ackpacket[4] = (fnum >> 24) & 0xFF;
    ackpacket[3] = (fnum >> 16) & 0xFF;
    ackpacket[2] = (fnum >> 8) & 0xFF;
    ackpacket[1] = fnum & 0xFF;
    ackpacket[5] = countChecksum(ackpacket , 5);
    //send
    if (nack == ACK) {
        printf("Mengirim ACK frame ID = %d.\n", fnum);
    } else if (nack == NAK) {
        printf("Mengirim NAK frame ID = %d.\n", fnum);
    }
    int i = 0;
    while (i < 6) {
        sendByte(ackpacket[i++]);
    }
}

/* ===== ADT QUEUE ===== */
int isBufferEmpty() {
    return headQueue == tailQueue ? 1 : 0;
}

int isBufferLowLimit() {
    if (tailQueue >= headQueue) {
        return tailQueue - headQueue < LOWLIMIT ? 1 : 0;
    } else {
        return tailQueue + BUFFSIZE - headQueue < LOWLIMIT ? 1 : 0;
    }
}

int isBufferUpLimit() {
    if (tailQueue >= headQueue) {
        return tailQueue - headQueue > UPLIMIT ? 1 : 0;
    } else {
        return tailQueue + BUFFSIZE - headQueue > UPLIMIT ? 1 : 0;
    }
}

void addToBuffer(char c) {
    ///*debug*/printf("[ADDTOBUFFER] %x\n", c);
    queue[tailQueue++] = c;
    tailQueue %= BUFFSIZE;
}

char delFromBuffer() {
    char result = queue[headQueue++];
    headQueue %= BUFFSIZE;
    ///*debug*/printf("[DELFROMBUFFER] %x\n", result);
    return result;
}

/* ===== THREAD ===== */
void* byteHandler() {
    int lenSleep = 100000; //jeda saat mengkonsumsi data, nanosecond
    //loop selama antrian masih ada
    while (1) {
        while (isBufferEmpty() == 0) {
            unsigned char rcvchar = delFromBuffer();
            insertArray(&receivedBytes, rcvchar);
            ///*debug*/printf("[BYTEHANDLER] %x\n", rcvchar);
            if (isBufferLowLimit() && isON == 0) { //transmitter bisa lanjutin
                printf("Buffer < lowerlimit\n");
                printf("Mengirim XON.\n");
                sendByte(XON);
                isON = 1;
            }
        }
    }
}

void* frameHandler() {
    FrameData thisFrame;
    Array tempFrame;
    Array tempText;
    initArray(&tempFrame, 5);
    initArray(&tempText, 5);
    //loop sampai paket habis
    while (1) {
        if (getByte() == SOH) {
            isFrameComplete = 0;
            insertArray(&tempFrame, SOH);
            printf("\nSOH.\n");
            //ambil 4 byte lalu jadikan integer
            unsigned char frameID[4];
            int i;
            for (i = 0; i < 4; i++) {
                frameID[i] = getByte();
                insertArray(&tempFrame, frameID[i]);
                ///*debug*/printf("[FRAMEHANDLER] ID %d %x\n", i, frameID[i]);
            }
            int t_num = *((int*) frameID);
            /*debug*/printf("Frame ID = %d.\n", t_num);
            if (getByte() == STX ) {
                insertArray(&tempFrame, STX);
                /*debug*/printf("STX.\n");
                //ambil data
                unsigned char realText;
                do {
                    realText = getByte();
                    if (realText == ETX) {
                        insertArray(&tempFrame, ETX);
                        /*debug*/printf("ETX.\n");
                        if (getByte() == countChecksum(tempFrame.data, tempFrame.used)) {
                            thisFrame.id = t_num;
                            thisFrame.length = tempText.used;
                            int i;
                            for (i = 0; i < tempText.used; i++) {
                                thisFrame.text[i] = tempText.data[i];
                            }
                            insertArrayFrame(&receivedFrames, thisFrame);
                            sendACK(t_num, ACK);
                        } else { //checksum berbeda
                            sendACK(t_num, NAK);
                        }
                        break;
                    } else {
                        insertArray(&tempFrame, realText);
                        insertArray(&tempText, realText);
                    }
                } while(1);
            } else { //setelah ID bukan STX
                sendACK(t_num, NAK);
            }
            freeArray(&tempText);
            freeArray(&tempFrame);
            isFrameComplete = 1;
        }
    }
}

/* ===== MAIN PROGRAM ===== */
int main(int argc, char* argv[]) {
    int mulai = 0;
    if (argc == 2) {
        //inisialisasi
        initArray(&receivedBytes, 5);
        initArrayFrame(&receivedFrames,5);
        processedIncomingBytes = 0;
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
        pthread_t tByteHandler;
        pthread_t tFrameHandler;
        int countByte = 0; //counter karakter
        char rcvchar; //tempat menampung karakter yang diterima
        //loop sampai break di ENDFILE
        while (1) {
            //menerima 1 karakter
            if (recvfrom(sock, &rcvchar, 1, 0, (struct sockaddr*)& trnsmtaddr, &myaddrlen) > 0) {
                addToBuffer(rcvchar);
                if (rcvchar == STARTFILE && !mulai) {
                    mulai = 1;
                    pthread_create(&tByteHandler, NULL, byteHandler, "byteHandler"); //jalankan thread 
                    pthread_create(&tFrameHandler, NULL, frameHandler, "frameHandler"); //jalankan thread
                } else if (rcvchar == ENDFILE && isFrameComplete) {
                    pthread_cancel(tByteHandler);
                    pthread_cancel(tFrameHandler);
                    //print data
                    printf("\nData diterima:\n");
                    int i;
                    int j;
                    for (i = 0; i < receivedFrames.used; i++) {
                        for (int j = 0; j < receivedFrames.data[i].length; j++) {
                            printf("%c", receivedFrames.data[i].text[j]);
                        }
                    }
                    printf("\n");
                    break;
                } else {
                    //printf("Menerima byte ke-%d : %x.\n", ++countByte , rcvchar);
                    if (isBufferUpLimit()) { //transmit menunda pengiriman
                        printf("Buffer > minimum upperlimit\n");
                        printf("Mengirim XOFF.\n");
                        sendByte(XOFF);
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

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
Array receivedBytes; //seluruh byte yg diterima disimpan di 1 string panjang
ArrayFrame receivedFrames; //frame-frame yg benar dan terurut
int isFrameComplete = 0; //apakah frame sedang di proses
int bytesProcessed = 0; //sudah berapa byte yang di proses
pthread_mutex_t lock; //pengiriman ACK/NAK di lock agar data terurut

unsigned char countChecksum(unsigned char* ptr, size_t sz) {
    unsigned char chk = 0;
    while (sz-- != 0) {
        chk -= *ptr++; //dari internet, ikutin aja
    }
    return chk;
}

unsigned char getByte() {
    while(bytesProcessed >= receivedBytes.used); //wait until new incoming byte
    unsigned char result = receivedBytes.data[bytesProcessed++];
    return result;
}

void sendByte(char buffX) {
    usleep(10000); //jeda supaya gak balapan
    if (sendto(sock, &buffX, 1, 0, (struct sockaddr*)& trnsmtaddr, myaddrlen) < 0) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

void sendNACK(int fnum, unsigned char nack) {
    pthread_mutex_lock(&lock);
    //build packet
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
        printf("mengirim NAK frame ID = %d.\n", fnum);
    }
    int i = 0;
    while (i < 6) {
        sendByte(ackpacket[i++]);
    }
    pthread_mutex_unlock(&lock);
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
    queue[tailQueue++] = c;
    tailQueue %= BUFFSIZE;
}

char delFromBuffer() {
    char result = queue[headQueue++];
    headQueue %= BUFFSIZE;
    return result;
}

/* ===== THREAD ===== */
void* byteHandler() {
    while (1) {
        //loop selama antrian masih ada
        while (isBufferEmpty() == 0) {
            insertArray(&receivedBytes, delFromBuffer()); //konsumsi: masukkan ke array
            if (isBufferLowLimit() && (isON == 0)) { //transmitter bisa lanjutin
                printf("Buffer < lowerlimit\n");
                printf("Mengirim XON.\n");
                sendByte(XON);
                isON = 1;
            }
        }
    }
}

void* frameHandler() {
    Array tempFrame;
    FrameData thisFrame;
    int dataLength;
    int frameID;
    int i;
    unsigned char byteID[4];
    unsigned char cs;
    //jika paket frame sudah habis, di-interrupt sama main program
    while (1) {
        if (getByte() == SOH) {
            isFrameComplete = 0; //proses identifikasi 1 frame dimulai, tidak boleh diganggu
            initArray(&tempFrame);
            insertArray(&tempFrame, SOH);
            //ambil 4 byte lalu jadikan integer
            for (i = 0; i < 4; i++) {
                byteID[i] = getByte();
                insertArray(&tempFrame, byteID[i]);
            }
            frameID = *((int*) byteID);
            printf("\nFrame ID = %d diterima.\n", frameID);
            if (getByte() == STX ) {
                insertArray(&tempFrame, STX);
                //ambil data
                unsigned char rcvchar;
                dataLength = 0;
                do {
                    rcvchar = getByte();
                    if (rcvchar == ETX) {
                        insertArray(&tempFrame, ETX);
                        cs = getByte();
                        if (cs == countChecksum(tempFrame.data, tempFrame.used)) {
                            //frame sudah benar, gabungkan ke frame pusat untuk diurutkan
                            thisFrame.id = frameID;
                            thisFrame.length = dataLength;
                            for (i = 0; i < dataLength; i++) {
                                thisFrame.text[i] = tempFrame.data[i + 6]; //data pertama ada di indeks 6
                            }
                            insertArrayFrame(&receivedFrames, thisFrame);
                            sendNACK(frameID, ACK);
                        } else { //checksum berbeda
                            printf("Invalid checksum, ");
                            sendNACK(frameID, NAK);
                        }
                        break;
                    } else {
                        dataLength++;
                        insertArray(&tempFrame, rcvchar);
                    }
                } while(1); //data bisa sebanyak apapun, berhenti hanya jika sudah menerima ETX
            } else { //setelah ID bukan STX
                printf("Invalid STX, ");
                sendNACK(frameID, NAK);
            }
            isFrameComplete = 1;
        }
    }
}

/* ===== MAIN PROGRAM ===== */
int main(int argc, char* argv[]) {
    int mulai = 0;
    if (argc == 2) {
        if (pthread_mutex_init(&lock, NULL) != 0) {
            printf("mutex_init() failed.\n");
            exit(1);
        }
        //inisialisasi
        initArray(&receivedBytes);
        initArrayFrame(&receivedFrames);
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
                } else if ((rcvchar == ENDFILE) && isFrameComplete) {
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

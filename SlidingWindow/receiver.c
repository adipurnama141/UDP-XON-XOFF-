#include "array.c"
#include <net/if.h>
#include <sys/ioctl.h>

#define BUFFSIZE 256
#define UPLIMIT 254
#define LOWLIMIT 2

struct ifreq ifr; //gatau, dapet dari internet
struct sockaddr_in myaddr; //address program ini sebaga receiver
struct sockaddr_in trnsmtaddr; //address transmitter

Array receivedBytes;
ArrayFrame receivedFrames;
char queue[BUFFSIZE]; //antrian karakter sirkular
int headQueue = 0; //antrian pertama siap dikonsum
int isON = 1; //status receiver ON
int processedIncomingBytes;
int sensitiveReading;
int sock; //socket
int tailQueue = 0; //space kosong setelah antrian terakhir
socklen_t myaddrlen = sizeof(myaddr); //ukuran socket ini

/* ADT QUEUE */
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

/* PENGAKSESAN ARRAY */
unsigned char countChecksum(unsigned char* ptr, size_t sz) {
    unsigned char chk = 0;
    while (sz-- != 0) {
        chk -= *ptr++;
    }
    /*debug*/ printf("[COUNT CHECKSUM] %x\n", chk);
    return chk;
}

unsigned char getByte() {
    while(processedIncomingBytes >= receivedBytes.used) {
        //wait until new incoming byte
    }
    unsigned char result = receivedBytes.data[processedIncomingBytes++];
    /*debug*/ printf("[GET BYTE] %x\n", result);
    return result;
}

void sendByte(char buffX) {
    //kirim 1 karakter
    if (sendto(sock, &buffX, 1, 0, (struct sockaddr*)& trnsmtaddr, myaddrlen) < 0) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

void sendACK(int fnum) {
    //build ACK packet
    unsigned char ackpacket[6];
    ackpacket[0] = ACK;
    ackpacket[4] = (fnum >> 24) & 0xFF;
    ackpacket[3] = (fnum >> 16) & 0xFF;
    ackpacket[2] = (fnum >> 8) & 0xFF;
    ackpacket[1] = fnum & 0xFF;
    ackpacket[5] = countChecksum(ackpacket , 5);
    //send
    int i = 0;
    while (i < 6) {
    	/*debug*/ printf("[SEND ACK] %d %x\n", i, ackpacket[i]);
        sendByte(ackpacket[i++]);
    }
}

void* frameIdentifier() {
	FrameData thisFrame;
    Array tempFrame;
    Array tempText;
    initArray(&tempFrame, 5);
    initArray(&tempText, 5);
    //loop sampai paket habis
    while (1) {
        if (getByte() == SOH) {
            insertArray(&tempFrame, SOH);
            /*debug*/ printf("[FRAME IDENTIFIER] SOH\n");
            //ambil 4 byte lalu jadikan integer
            unsigned char frameID[4];
            frameID[0] = getByte();
            frameID[1] = getByte();
            frameID[2] = getByte();
            frameID[3] = getByte();
            int i;
            for (i = 0; i < 4; i++) {
                /*debug*/ printf("[FRAME IDENTIFIER] ID %d %x\n", i, frameID[i]);
            }
            insertArray(&tempFrame, frameID[0]);
            insertArray(&tempFrame, frameID[1]);
            insertArray(&tempFrame, frameID[2]);
            insertArray(&tempFrame, frameID[3]);
            int t_num = *((int*) frameID);
            /*debug*/ printf("[FRAME IDENTIFIER] ID %d\n", t_num);
            if (getByte() == STX ) {
                insertArray(&tempFrame, STX);
                /*debug*/ printf("[FRAME IDENTIFIER] STX\n");
                //ambil data
                int counter = 0;
                unsigned char realText;
                do {
                    realText = getByte();
                    if (realText == ETX) {
                        insertArray(&tempFrame, ETX);
                        /*debug*/ printf("[FRAME IDENTIFIER] ETX\n");
                        if (getByte() == countChecksum(tempFrame.data, tempFrame.used)) {
                            thisFrame.id = t_num;
                            thisFrame.length = counter;
                            int i;
                            printf("[FRAME IDENTIFIER] data received: ");
                            for (i = 0; i < tempText.used; i++) {
                                printf("%c", tempText.data[i]);
                                thisFrame.text[i] = tempText.data[i];
                            }
                            printf("\n");
                            insertArrayFrame(&receivedFrames, thisFrame);
                            sendACK(t_num);
                        }
                        break;
                    }
                    //bukan ETX
                    insertArray(&tempFrame, realText);
                    insertArray(&tempText, realText);
                    printf("[FRAMEIDENTIFIER] Isi data frame no %d : %c \n", ++counter, realText);
                } while(counter < FRAMEDATASIZE);
            }
        }
        freeArray(&tempText);
        freeArray(&tempFrame);
    }
}

void *q_get() {
    int countByte = 0; //counter karakter
    int lenSleep = 100000; //jeda saat mengkonsumsi data, 100 ms
    //usleep(lenSleep);
    //sleep(2);
    //loop selama antrian masih ada
    for(;;){
        while (isEmpty() == 0) {
            //usleep(lenSleep);
            unsigned char getfromstack = delete();
            
           

            //printf("Mengkonsumsi byte ke-%d: '%x'\n", ++countByte, getfromstack);
            insertArray(&receivedBytes, getfromstack);

            printf("RECEIVED BYTES! :  %x \n", getfromstack);
            /*
            int k = 0;
            for ( k = 0 ; k < receivedBytes.used ; k++) {
                printf("%x " , receivedBytes.data[k]);
            }
            printf("\n");
            */
    
            if (isLower() && isON == 0) { //transmitter bisa lanjutin
                printf("Buffer < lowerlimit\n");
                printf("Mengirim XON.\n");
                sendByte(XON);
                isON = 1;
            }
            //sleep(1);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            printf("socket() failed.\n");
            exit(1);
        }

        initArray(&receivedBytes, 5);
        initArrayFrame(&receivedFrames,5);

        processedIncomingBytes = 0;

        //sendACK(8);        

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
        pthread_t pth2;
        int countByte = 0; //counter karakter
        char buff; //tempat menampung karakter yang diterima


        //loop sampai break di ENDFILE
        while (1) {
            //menerima 1 karakter
            if (recvfrom(sock, &buff, 1, 0, (struct sockaddr*)& trnsmtaddr, &myaddrlen) > 0) {
                 add(buff);

                if (buff == STARTFILE) {
                    pthread_create(&pth, NULL, q_get, "q_get"); //jalankan thread 
                    pthread_create(&pth2, NULL, frameIdentifier, "fid"); //jalankan thread anak
                    printf("Thread started!\n");

                } else if (buff == ENDFILE) {
                    printf("END BRO!");
                    //pthread_join(pth, NULL); //tunggu thread anak selesai
                    printf("Done?!\n");

                    /*
                    int k = 0;
                    for ( k = 0 ; k < receivedBytes.used ; k++) {
                        printf("%x " , receivedBytes.data[k]);
                    }
                    */

                    break;
                } else {
                    //printf("Menerima byte ke-%d : %x.\n", ++countByte , buff);
                   
                    if (isUpper()) { //transmit menunda pengiriman
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

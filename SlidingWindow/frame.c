#include "array.c"

unsigned char** frames;
int sock; //socket
struct sockaddr_in recvaddr; //address receiver
int recvlen = sizeof(recvaddr); //ukuran socket receiver
int isON = 1; //status receiver ON
int bottomPointer;
int nFrame;
int topPointer;
int* statusFrame;
Array receivedBytes;
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
    if (sendto(sock, &buffX, 1, 0, (struct sockaddr*)& recvaddr, recvlen) < 0) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

void sendFrame(int framenumber) {
    unsigned char* frame = frames[framenumber];
    int i = 0;
    unsigned char sendchar;
    /*debug*/printf("Mengirim frame ID = %d.\n", framenumber);
    while (i < FRAMESIZE) {
        if (isON) {
            sendByte(frame[i++]);
        } else {
            printf("Menunggu XON...\n");
        }
    }
    statusFrame[framenumber] = 1;
}

int isDone() {
    int i = 0;
    while (i < nFrame) {
        if (statusFrame[i] == 2) {
            i++;
        } else {
            return 0;
        }
    }
    return 1;
}

/* ===== FILE ===== */
unsigned char** createArray(int sizeX, int sizeY) {
    int i;
    unsigned char** result = (unsigned char**) malloc(sizeX * sizeof(unsigned char*));
    for (i = 0; i < sizeX; i++) {
        result[i] = (unsigned char*) malloc(sizeY * sizeof(unsigned char));
    }
    return result;
}

int getNumFrame(int filelen){
    if ((filelen % FRAMEDATASIZE) > 0) {
        return (filelen / FRAMEDATASIZE) + 1;
    } else {
        return filelen / FRAMEDATASIZE;
    }
}

unsigned char** parseToFrames(char* buffFile, int filelen) {
    int nFrame = 0;
    int datapointer = 0;
    int bytesLeft = 0;
    int numframe = getNumFrame(filelen);
    int n = FRAMESIZE;

    unsigned char ** frames = createArray(numframe,n);

    int i = 0;
    while ( i < filelen) {

        //Start of header
        frames[nFrame][0] = SOH;

        //Frame number
        frames[nFrame][4] = (nFrame >> 24) & 0xFF;
        frames[nFrame][3] = (nFrame >> 16) & 0xFF;
        frames[nFrame][2] = (nFrame >> 8) & 0xFF;
        frames[nFrame][1] = nFrame & 0xFF;

        //Start of text
        frames[nFrame][5] = STX;

        //Text
        while ((datapointer < FRAMEDATASIZE) && (i < filelen)) {
            frames[nFrame][6+datapointer] = buffFile[i];
            i++;
            datapointer++;
        }

        //End of text
        frames[nFrame][6+datapointer] = ETX;

        //Cheksum
        unsigned char csum = countChecksum(frames[nFrame] , 6+datapointer+1);
        frames[nFrame][6+datapointer+1] = csum;

        //Reset pointer
        nFrame++;
        datapointer = 0;
    }
    return frames;
}

/* ===== THREAD ===== */
void* byteHandler() {
    char rcvchar;
    //loop sampai terminate
    while (1) {
        if (recvfrom(sock, &rcvchar, 1, 0, (struct sockaddr*)& recvaddr, &recvlen) > 0) {
            if (rcvchar == XON) {
                printf("XON diterima.\n");
                isON = 1;
            } else if (rcvchar == XOFF) {
                printf("XOFF diterima.\n");
                isON = 0;
            } else { //ACK atau NAK
                ///*debug*/printf("[BYTEHANDLER] %x\n", rcvchar);
                insertArray(&receivedBytes, rcvchar);
            }
        }
    }
}

void* ACKHandler() {
    Array tempFrame;
    initArray(&tempFrame, 5);
    //loop sampai terminate
    while (1) {
        unsigned char nack = getByte();
        if ((nack == ACK) || (nack == NAK)) {
            insertArray(&tempFrame, nack);
            ///*debug*/printf("[ACKHANDLER] ACK\n");
            //ambil 4 byte lalu jadikan integer
            unsigned char frameID[4];
            int i;
            for (i = 0; i < 4; i++) {
                frameID[i] = getByte();
                insertArray(&tempFrame, frameID[i]);
                ///*debug*/printf("[ACKHANDLER] ID %d %x\n", i, frameID[i]);
            }
            int t_num = *((int*) frameID);
            ///*debug*/printf("[ACKHANDLER] ID %d\n", t_num);
            if (getByte() == countChecksum(tempFrame.data, tempFrame.used)) {
                if (nack == ACK) {
                    statusFrame[t_num] = 2;
                    printf("ACK frame ID = %d diterima.\n", t_num);
                } else if (nack == NAK) {
                    printf("NAK frame ID = %d diterima.\n", t_num);
                    sendFrame(t_num);
                }
            }
            freeArray(&tempFrame);
        }
    }
}

void* slidingWindow(){
    bottomPointer = 0;
    topPointer = 0 + WINDOWSIZE;
    int i;
    //loop sampai semua frame terkirim
    while (1) {
        //sending frames to receiver
        i = bottomPointer;
        while ((i < topPointer) && (i < nFrame) && (!isDone())) {
            if (statusFrame[i] != 2) {
                sendFrame(i);
            }
            i++;
        }
        usleep(500000); //timeout
        if (!isDone()) {
            //adjust pointer
            i = bottomPointer;
            while (statusFrame[i++] == 2) {
                bottomPointer++;
                topPointer++;
            }
        } else {
            break;
        }
    }
}

/* ===== MAIN PROGRAM ===== */
int main(int argc, char* argv[]) {
    if (argc == 4) {
        initArray(&receivedBytes, 5);
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
        FILE* file = fopen(argv[3],"rb");
        fseek(file, 0, SEEK_END);
        int filelen = ftell(file);
        rewind(file);
        char* buffFile = (char*) malloc(filelen + 1); //file dipindah ke memori
        fread(buffFile, filelen, 1, file);
        fclose(file);
        //ubah data menjadi frame-frame
        frames = parseToFrames(buffFile, filelen);
        nFrame = getNumFrame(filelen);
        statusFrame = (int*) malloc(nFrame * sizeof(int));
        int i;
        for(i = 0; i < nFrame; i++) {
            statusFrame[i] = 0;
        }
        //jalankan handler
        pthread_t tByteHandler;
        pthread_t tACKHandler;
        pthread_t tSlidingWindow;
        pthread_create(&tByteHandler, NULL, byteHandler, "byteHandler");
        pthread_create(&tACKHandler, NULL, ACKHandler, "ACKHandler");
        pthread_create(&tSlidingWindow, NULL, slidingWindow, "slidingWindow");
        sendByte(STARTFILE);
        pthread_join(tSlidingWindow, NULL);
        pthread_cancel(tACKHandler);
        pthread_cancel(tByteHandler);
        sendByte(ENDFILE);
    } else { //arg invalid
        printf("Usage : ./transmitter ip_address port filename\n");
        exit(1);
    }
}
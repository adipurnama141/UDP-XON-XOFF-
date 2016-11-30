#include "array.c"

int isNACKCompleted = 0; //apakah ACK/NAK sedang di proses;
int isON = 1; //status receiver ON
/*socket*/
int sock; //socket
struct sockaddr_in recvaddr; //address receiver
int recvlen = sizeof(recvaddr); //ukuran socket receiver
/*frame*/
int nFrame; //banyaknya frame yang harus dikirim
int* statusFrame; //status tiap frame, belum dikirim, sudah dikirim, sudah diterima
pthread_mutex_t lock; //pengiriman frame di lock agar data terurut
unsigned char** frames; //data dari file yang telah dipecah ke frame-frame
/*array*/
Array receivedBytes; //seluruh byte yg diterima disimpan di 1 string panjang
int bytesProcessed; //sudah berapa byte yang di proses

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
    if (sendto(sock, &buffX, 1, 0, (struct sockaddr*)& recvaddr, recvlen) < 0) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

void sendEndConnection(){
    sendByte(ENDFILE);
    sendByte(ENDFILE);
    sendByte(ENDFILE);
    sendByte(ENDFILE);
}

void sendFrame(int fnum) {
    pthread_mutex_lock(&lock);
    printf("Mengirim frame ID = %d.\n", fnum);
    unsigned char* frame = frames[fnum];
    int i = 0;
    while (i < FRAMESIZE) {
        if (isON) {
            sendByte(frame[i++]);
        } else {
            printf("Menunggu XON...\n");
        }
    }
    statusFrame[fnum] = 1;
    pthread_mutex_unlock(&lock);
}

int isDone() {
    int i = 0;
    //cek seluruh status apakah sudah bernilai 2 semuanya
    while (i < nFrame) {
        if (statusFrame[i++] != 2) {
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
    //banyaknya frame yang harus dikirim
    if ((filelen % FRAMEDATASIZE) > 0) {
        return (filelen / FRAMEDATASIZE) + 1;
    } else {
        return filelen / FRAMEDATASIZE;
    }
}

void parseToFrames(char* buffFile, int filelen) {
    //data pada file dipecah menjadi frame-frame, tiap frame memuat beberapa karakter data
    int datapointer;
    int i = 0;
    int idxFrame = 0;
    frames = createArray(getNumFrame(filelen), FRAMESIZE);
    while (i < filelen) {
        datapointer = 0;
        //SOH + frame ID + STX
        frames[idxFrame][0] = SOH;
        frames[idxFrame][4] = (idxFrame >> 24) & 0xFF;
        frames[idxFrame][3] = (idxFrame >> 16) & 0xFF;
        frames[idxFrame][2] = (idxFrame >> 8) & 0xFF;
        frames[idxFrame][1] = idxFrame & 0xFF;
        frames[idxFrame][5] = STX;
        //datanya
        while ((datapointer < FRAMEDATASIZE) && (i < filelen)) {
            frames[idxFrame][6+datapointer] = buffFile[i];
            i++;
            datapointer++;
        }
        //ETX + checksum
        frames[idxFrame][6+datapointer] = ETX;
        frames[idxFrame][6+datapointer+1] = countChecksum(frames[idxFrame] , 6+datapointer+1);
        idxFrame++;
    }
}

/* ===== THREAD ===== */
void* byteHandler() {
    char rcvchar;
    //loop sampai terminate
    while (1) {
        if (recvfrom(sock, &rcvchar, 1, 0, (struct sockaddr*)& recvaddr, &recvlen) > 0) {
            //printf("0x%x\n", rcvchar);
            if ((rcvchar == ACK) || (rcvchar == NAK)) {
                isNACKCompleted = 0; //proses identifikasi ACK/NAK dimulai, tidak boleh diganggu
                insertArray(&receivedBytes, rcvchar);
            } else if (!isNACKCompleted) {
                insertArray(&receivedBytes, rcvchar);
            } else if (rcvchar == XON) {
                printf("XON diterima.\n");
                //isON = 1;
            } else if (rcvchar == XOFF) {
                printf("XOFF diterima.\n");
                //isON = 0;
            }
        }
    }
}

void* NACKHandler() {
    Array tempFrame;
    int i;
    int frameID;
    unsigned char byteID[4];
    unsigned char cs;
    unsigned char nack;
    //loop sampai terminate
    while (1) {
        nack = getByte();
        if ((nack == ACK) || (nack == NAK)) {
            initArray(&tempFrame);
            insertArray(&tempFrame, nack);
            //ambil 4 byte lalu jadikan integer
            for (i = 0; i < 4; i++) {
                byteID[i] = getByte();
                insertArray(&tempFrame, byteID[i]);
            }
            frameID = *((int*) byteID);
            cs = getByte();
            isNACKCompleted = 1;
            if (cs == countChecksum(tempFrame.data, tempFrame.used)) {
                if (nack == ACK) {
                    statusFrame[frameID] = 2;
                    printf("\nACK frame ID = %d diterima.\n\n", frameID);
                } else if (nack == NAK) {
                    if ((frameID >= 0) && (frameID < nFrame)) {
                        printf("\nNAK frame ID = %d diterima.\n\n", frameID);
                        //sendFrame(frameID);
                    } else {
                        printf("\nInvalid frame ID.\n\n");
                    }
                }
            } else {
                printf("\nInvalid checksum.\n\n");
            }
        }
    }
}

void* slidingWindow(){
    int bottomPointer = 0;
    int i;
    int topPointer = 0 + WINDOWSIZE;
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
        usleep(150000); //timeout
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
        initArray(&receivedBytes);
        if (pthread_mutex_init(&lock, NULL) != 0) {
            printf("mutex_init() failed.\n");
            exit(1);
        }
        //buat socket
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
        parseToFrames(buffFile, filelen);
        nFrame = getNumFrame(filelen);
        statusFrame = (int*) malloc(nFrame * sizeof(int));
        int i;
        for(i = 0; i < nFrame; i++) {
            statusFrame[i] = 0;
        }
        //jalankan handler
        pthread_t tByteHandler;
        pthread_t tNACKHandler;
        pthread_t tSlidingWindow;
        pthread_create(&tByteHandler, NULL, byteHandler, "byteHandler");
        pthread_create(&tNACKHandler, NULL, NACKHandler, "NACKHandler");
        pthread_create(&tSlidingWindow, NULL, slidingWindow, "slidingWindow");
        sendByte(STARTFILE);
        pthread_join(tSlidingWindow, NULL);
        pthread_cancel(tNACKHandler);
        pthread_cancel(tByteHandler);
        sendEndConnection();
        //sendByte(ENDFILE);
    } else { //arg invalid
        printf("Usage : ./transmitter ip_address port filename\n");
        exit(1);
    }
}

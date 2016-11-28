#include "array.c"
#include <time.h>

unsigned char ** frames;
int sock; //socket
struct sockaddr_in recvaddr; //address receiver
int recvlen = sizeof(recvaddr); //ukuran socket receiver
int isON = 1; //status receiver ON
int bottomPointer;
int nFrame;
int topPointer;
int* sentFrames;
Array receivedBytes;
int processedIncomingBytes;

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

void sendByte(char buff) {
    //kirim 1 karakter
    if (sendto(sock, &buff, 1, 0, (struct sockaddr*)& recvaddr, recvlen) == -1) {
        printf("sendto() failed.\n");
        exit(1);
    }
}

void sendFrameData(unsigned char* frame) {
    //kirim 1 karakter
    int i = 0;
    unsigned char buf;
    while (i < FRAMESIZE) {
        if (isON) {
            buf = frame[i];
            //printf("Sending %x\n", buf);
            if (sendto(sock, &buf, 1, 0, (struct sockaddr*)& recvaddr, recvlen) == -1) {
                printf("sendto() failed.\n");
                //exit(1);
            }
            usleep(500);
            i++;
        } else {
            printf("Waiting for XON!\n");
        }
    }
}

void sendFrame(int framenumber) {
    sendFrameData(frames[framenumber]);
    sentFrames[framenumber] = 1;
    printf("Send frame %d selesai\n", framenumber);
    int k =0;
    for (k = 0; k < nFrame; k++) {
        printf("%d ", sentFrames[k]);
    }
    printf("\n");
}

int isDone() {
    int i = 0;
    while (i < nFrame) {
        if (sentFrames[i] == 2) {
            i++;
        } else {
            return 0;
        }
    }
    return 1;
}

void waitFor(unsigned int secs) {
    unsigned int retTime = time(0) + secs; //get finishing time
    while (time(0) < retTime) {
        //loop until it arrives
    }
}

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


//FRAMER
//Menerima file dan panjang file
//Menghasilkan array of frame
unsigned char** framer(char * buffFile , int filelen)
{
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

        int j;
        for (j = 0 ; j < FRAMESIZE ;j++){
            printf("%x ",frames[nFrame-1][j]);
        }
        printf("\n");   

    }
    return frames;
}

void *ackIdentifier(){
    Array tempFrame;
    initArray(&tempFrame, 5);
    unsigned char currentByteCheck;
    //printf("[FRAMEIDENTIFIER]  SAYA AKTIFF!!\n");
    for(;;){

        if (getByte() == ACK) {
            insertArray(&tempFrame, ACK);
            //printf("[FRAMEIDENTIFIER]   SOH detected!\n");

            unsigned char frameID[4];
            frameID[0] = getByte();
            frameID[1] = getByte();
            frameID[2] = getByte();
            frameID[3] = getByte();

            insertArray(&tempFrame, frameID[0]);
            insertArray(&tempFrame, frameID[1]);
            insertArray(&tempFrame, frameID[2]);
            insertArray(&tempFrame, frameID[3]);

            int t_num = *((int*) frameID);
            //printf("[FRAMEIDENTIFIER]  lengkap! %x %x %x %x \n", frameID[0], frameID[1], frameID[2], frameID[3]);
            //printf("[FRAMEIDENTIFIER]  NOMOR SAYA : %d \n" , t_num);

            unsigned char csum = getByte();
            unsigned char computed_csum = countChecksum(tempFrame.array , tempFrame.used);
            if (csum == computed_csum) {
                printf("ACK %d diterima \n" , t_num);
                sentFrames[t_num] = 2;

                int k =0;
                for (k = 0 ; k < nFrame ; k++){
                    printf("%d ", sentFrames[k]);
                };
                printf("\n");

                printf("is done? : %d\n", isDone());

                

            }


            }
                
        freeArray(&tempFrame);

    }
}






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
            else {
                //printf("Receiving : %x \n" , buffX);
                insertArray(&receivedBytes, buffX);

            }
        }
    }
}

void *slidingWindowMgr(){
    bottomPointer = 0;
    topPointer = 0 + WINDOWSIZE;
    int i;
    int moveCounter;

    while (!isDone()) {

        i = bottomPointer;
        printf("Cold booting\n");
        //sending frames to receiver
        while (( i < topPointer) && (isDone() == 0) && (i < nFrame)) {
            if (sentFrames[i] != 2) {
                sendFrame(i);
                printf("%d - %d Mengirim frame %d\n" ,bottomPointer, topPointer, i);
            }
            else {
                printf("Tidak mengirim frame %d\n" , i);
            }
            printf("i : %d",i);
            i++;
        }

        printf("KLOTER TERKIRIM!  %d - %d \n", bottomPointer, topPointer );
        printf("Sleep for 1sec\n");
        //timeout
        waitFor(1);
        printf("Wake up!\n");

        if (isDone()){
            printf("Sudah selesai :)\n");
            sendByte(ENDFILE);
            break;
        }

        //adjusting bottomPointer;
        i = bottomPointer;
        moveCounter = 0;
        while (sentFrames[i] == 2){
            i++;
            moveCounter++;
        }

        //adjusting both pointer;
        bottomPointer += moveCounter;
        topPointer += moveCounter;
    }
}





int main(int argc, char* argv[]){

    int i;
    initArray(&receivedBytes, 5);


    //buka file
    FILE* file = fopen("tes.txt","rb");

    //cari panjang file
    fseek(file, 0, SEEK_END);
    int filelen = ftell(file);
    rewind(file);

    //pindah file ke memori
    char* buffFile = (char*) malloc(filelen + 1); 
    fread(buffFile, filelen, 1, file);

    //ubah file jadi frame
    frames = framer(buffFile,filelen);

    //persiapan kirim broh!
  


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

    nFrame = getNumFrame(filelen);
    int k = 0;

    sentFrames = (int *)malloc(nFrame * sizeof(int));

    for(i = 0 ; i < nFrame ; i++){
        sentFrames[i] = 0;
    }



    pthread_t pth;
    pthread_t pth2;
    pthread_t pth3;
    sendByte(STARTFILE);
    pthread_create(&pth, NULL, xHandler, "xHandler");
    pthread_create(&pth2, NULL, ackIdentifier, "fid");
    pthread_create(&pth3, NULL, slidingWindowMgr, "slidingwindow");
    



   

    /*
    for ( i = 0 ; i < nFrame; i++ ){
        sendFrame(frames[i]);
    }
    */

    
   

    


    printf("kirim end!");

    for (;;){

        i = 0;
        for(i = 0 ; i < nFrame ; i++){
            printf("%d ", sentFrames[i]);
        }
        printf("\n");
        sleep(1);

    }
    //sendByte(ENDFILE);
    /*
    while (k < nFrame){
        sendFrame(frames[k]);

    }
    k++;
    */
            
    

   
   

}
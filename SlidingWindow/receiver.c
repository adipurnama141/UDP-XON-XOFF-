#include "connect.h"
#include <net/if.h>
#include <sys/ioctl.h>


#define BUFFSIZE 256
#define UPLIMIT 254
#define LOWLIMIT 2


int sensitiveReading;



unsigned char checksum (unsigned char *ptr, size_t sz) {
    unsigned char chk = 0;
    while (sz-- != 0) {
        chk -= *ptr++;
    }
    return chk;
}

typedef struct {
  unsigned char *array;
  size_t used;
  size_t size;
} Array;



typedef struct 
{
    int id;
    unsigned char text[FRAMEDATASIZE];
    int length;
} FrameData;

typedef struct {
  FrameData *array;
  size_t used;
  size_t size;
} ArrayFrame;



Array receivedBytes;
ArrayFrame receivedFrames;



//ADT ARRAY

void initArray(Array *a, size_t initialSize) {
  a->array = (unsigned char *)malloc(initialSize * sizeof(unsigned char));
  a->used = 0;
  a->size = initialSize;
}

void insertArray(Array *a, unsigned char element) {
  if (a->used == a->size) {
    a->size *= 2;
    a->array = (unsigned char *)realloc(a->array, a->size * sizeof(unsigned char));
  }
  a->array[a->used++] = element;
}

void freeArray(Array *a) {
  free(a->array);
  a->array = 0;
  a->used = a->size = 0;
}


void initArrayFrame(ArrayFrame *a, size_t initialSize) {
  a->array = (FrameData *)malloc(initialSize * sizeof(FrameData));
  a->used = 0;
  a->size = initialSize;
}

void insertArrayFrame(ArrayFrame *a, FrameData element) {
  if (a->used == a->size) {
    a->size *= 2;
    a->array = (FrameData *)realloc(a->array, a->size * sizeof(FrameData));
  }
  a->array[a->used++] = element;
}

void freeArrayFrame(ArrayFrame *a) {
  free(a->array);
  a->array = 0;
  a->used = a->size = 0;
}
















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




//Pengaksesan Array
int processedIncomingBytes;


unsigned char getByte() {
    unsigned char temp;
    //penahan
    while(processedIncomingBytes >= receivedBytes.used) {
    }
    temp = receivedBytes.array[processedIncomingBytes];
    //printf("Get Byte Called : Number %d returning %x\n", processedIncomingBytes , temp);
    processedIncomingBytes++;
    return temp;
}





void sendX(char buffX) {
    //kirim 1 karakter
    if (sendto(sock, &buffX, 1, 0, (struct sockaddr*)& trnsmtaddr, myaddrlen) < 0) {
        printf("sendto() failed.\n");
        exit(1);
    }
}




void sendACK(int fnum){
    //printf("SENDING ACK :')) \n");
    unsigned char ackpacket[6];

    ackpacket[0] = ACK;
    ackpacket[4] = (fnum >> 24) & 0xFF;
    ackpacket[3] = (fnum >> 16) & 0xFF;
    ackpacket[2] = (fnum >> 8) & 0xFF;
    ackpacket[1] = fnum & 0xFF;
    ackpacket[5] = checksum(ackpacket , 5);

    
    int i = 0;
    /*
    for (i =0 ; i <6 ;i++){
        printf("%x ", ackpacket[i]);
    }
    printf("/n");
    */

    i = 0;
    unsigned char buf;
    while (i < 6){
        buf = ackpacket[i];
        sendX(buf);
        i++;

    }

}




void *frameIdentifier(){
    Array tempFrame;
    Array tempText;
    initArray(&tempFrame, 5);
    initArray(&tempText, 5);

    unsigned char currentByteCheck;
    //printf("[FRAMEIDENTIFIER]  SAYA AKTIFF!!\n");
    for(;;){

        if (getByte() == SOH) {
            insertArray(&tempFrame, SOH);
            printf("[FRAMEIDENTIFIER]   SOH detected!\n");

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
            printf("[FRAMEIDENTIFIER]  lengkap! %x %x %x %x \n", frameID[0], frameID[1], frameID[2], frameID[3]);
            printf("[FRAMEIDENTIFIER]  NOMOR SAYA : %d \n" , t_num);

            if (getByte() == STX ) {
                printf("[FRAMEIDENTIFIER]   STX detected!\n");
                insertArray(&tempFrame, STX);
                unsigned char realText = getByte();
                int counter  = 0;
                while ((realText != ETX) && (counter < FRAMEDATASIZE)) {
                    printf("[FRAMEIDENTIFIER]  Isi data frame no %d :  %c \n" , counter + 1 , realText);
                    insertArray(&tempFrame, realText);
                    insertArray(&tempText, realText);
                    counter++;
                    realText = getByte();
                }

                if(realText ==  ETX){
                    //printf("[FRAMEIDENTIFIER]   ETX detected!\n");
                    insertArray(&tempFrame, ETX);

                    unsigned char csum = getByte();
                    unsigned char computed_csum = checksum(tempFrame.array , tempFrame.used);

                    if (csum == computed_csum){
                        FrameData thisFrame;
                        printf("Frame %d diterimaa. \n", t_num);
                        printf("Counter : %d \n", counter);
                        thisFrame.id = t_num;
                        thisFrame.length = counter;

                        int i = 0;
                        for ( i = 0 ; i < tempText.used ; i++) {
                            printf("%c", tempText.array[i]);
                            thisFrame.text[i] = tempText.array[i];
                        }
                        printf("\n");

                        insertArrayFrame(&receivedFrames,thisFrame);
                        printf("Trying to send ACK%d\n",t_num);

                        sendACK(t_num);

                        /*
                        int o = 0;
                        for (o = 0 ; o < receivedFrames.used ; o++){
                            printf("Frame no : %d \n", receivedFrames.array[o].id);

                        }
                        */

                    }
                   

                }


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
                printf("%x " , receivedBytes.array[k]);
            }
            printf("\n");
            */
    
            if (isLower() && isON == 0) { //transmitter bisa lanjutin
                printf("Buffer < lowerlimit\n");
                printf("Mengirim XON.\n");
                sendX(XON);
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
                        printf("%x " , receivedBytes.array[k]);
                    }
                    */

                    break;
                } else {
                    //printf("Menerima byte ke-%d : %x.\n", ++countByte , buff);
                   
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

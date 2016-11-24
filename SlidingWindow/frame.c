#include "connect.h"

unsigned char ** frames;
int sock; //socket
struct sockaddr_in recvaddr; //address receiver
int recvlen = sizeof(recvaddr); //ukuran socket receiver
int isON = 1; //status receiver ON

//CHECKSUM
//Menerima pointer data dan panjang data yang akan diproses
//Menghasilkan checksum yang sesuai
//Apabila di ujung data sudah ditempatkan checksum yang benar, fungsi ini return nol
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


Array receivedBytes;

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











//CREATEARRAY
//Menghasilkan array 2 dimensi dari ukuran baris dan kolom
unsigned char** createArray(int arraySizeX, int arraySizeY) {
    unsigned char** theArray;
    int i;
    theArray = (unsigned char**) malloc(arraySizeX*sizeof(unsigned char*));
    for (i = 0; i < arraySizeX; i++)
       theArray[i] = (unsigned char*) malloc(arraySizeY*sizeof(unsigned char));
       return theArray;
} 


//GETNUMFRAME
//Menerima panjang file, mengeluarkan jumlah frame
int getNumFrame(int filelen){
    int numframe = filelen / FRAMEDATASIZE;
    if ((filelen % FRAMEDATASIZE) > 0) {
        numframe++;
    }
    return numframe;
}


//FRAMER
//Menerima file dan panjang file
//Menghasilkan array of frame
unsigned char** framer(char * buffFile , int filelen)
{
    int nframe = 0;
    int datapointer = 0;
    int bytesLeft = 0;
    int numframe = getNumFrame(filelen);
    int n = FRAMESIZE;

    unsigned char ** frames = createArray(numframe,n);

    int i = 0;
    while ( i < filelen) {

        //Start of header
        frames[nframe][0] = SOH;

        //Frame number
        frames[nframe][4] = (nframe >> 24) & 0xFF;
        frames[nframe][3] = (nframe >> 16) & 0xFF;
        frames[nframe][2] = (nframe >> 8) & 0xFF;
        frames[nframe][1] = nframe & 0xFF;

        //Start of text
        frames[nframe][5] = STX;

        //Text
        while ((datapointer < FRAMEDATASIZE) && (i < filelen)) {
            frames[nframe][6+datapointer] = buffFile[i];
            i++;
            datapointer++;
        }

        //End of text
        frames[nframe][6+datapointer] = ETX;

        //Cheksum
        unsigned char csum = checksum(frames[nframe] , 6+datapointer+1);
        frames[nframe][6+datapointer+1] = csum;

        //Reset pointer
        nframe++;
        datapointer = 0;

        int j;
        for (j = 0 ; j < FRAMESIZE ;j++){
            printf("%x ",frames[nframe-1][j]);
        }
        printf("\n");   

    }
    return frames;
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
            unsigned char computed_csum = checksum(tempFrame.array , tempFrame.used);
            if (csum == computed_csum) {
                printf("ACK %d diterima \n" , t_num);

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


void sendChar(char buff) {
    //kirim 1 karakter
    if (sendto(sock, &buff, 1, 0, (struct sockaddr*)& recvaddr, recvlen) == -1) {
        printf("sendto() failed.\n");
        exit(1);
    }
}


void sendFrame(unsigned char * frame) {
    //kirim 1 karakter
    int i = 0;
    unsigned char buf;
    while ( i < FRAMESIZE ) {
        if (isON){
            buf = frame[i];
            //printf("Sending %x\n", buf);
            if (sendto(sock, &buf, 1, 0, (struct sockaddr*)& recvaddr, recvlen) == -1) {
                printf("sendto() failed.\n");
                //exit(1);
            }
            usleep(500);
            i++;
        }
        else {
            printf("Waiting for XON!\n");
        }
    }
}




int main(int argc, char* argv[]){


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

    int nframe = getNumFrame(filelen);
    int k = 0;


    pthread_t pth;
    pthread_t pth2;
    pthread_create(&pth, NULL, xHandler, "xHandler");
    pthread_create(&pth2, NULL, ackIdentifier, "fid");
    sendChar(STARTFILE);



    int i = 0;

    /*
    for ( i = 0 ; i < nframe; i++ ){
        sendFrame(frames[i]);
    }
    */

    
    sendFrame(frames[0]);
    sendFrame(frames[1]);
    sendFrame(frames[2]);
    sendFrame(frames[3]);
    


    printf("kirim end!");

    for (;;){

    }
    //sendChar(ENDFILE);
    /*
    while (k < nframe){
        sendFrame(frames[k]);

    }
    k++;
    */
            
    

   
   

}
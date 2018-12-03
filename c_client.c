#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/time.h>



#include "utils.h"


#define BUFLEN 1024 // Max length of buffer
#define MAXFILESIZE 104857600 //100 MiB
#define CONTROL 8 //4 bytes position in file //4bytes crc
#define CHUNKSIZE BUFLEN - CONTROL
#define PORT 9999// The port on which to send data

#define TIMEOUT 500

//TODO: user input arguments
//
//
//



int socketFd; 
struct sockaddr_in myAddress, servaddr;
socklen_t myAddressLength= sizeof(myAddress);
socklen_t serverAddressLength= sizeof(servaddr);


int getFileSize(FILE *fp) {
    fseek(fp, 0L, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return size;
}

int sendPacket(uint8_t *buffer, int bufferLength){

    int bytesSend;	
    if (bytesSend = sendto(socketFd, buffer, bufferLength, 0, (struct sockaddr *) &servaddr, (socklen_t) serverAddressLength) == -1 ) {
        return -1;
    }
    return bytesSend;
}
int receivePacket(uint8_t *buffer, int bufferLength){
    int bytesReceived = recvfrom(socketFd, buffer, bufferLength, 0, (struct sockaddr *)&servaddr,(socklen_t *) &myAddressLength);
    if ( bytesReceived == -1) {
        return -1;
    }
    printf("DEBUG: Received bytes are %d Packet %d string %s\n",bytesReceived, buffer[1], buffer);
    return bytesReceived;

}

//OUTPUT VALUE :
//  timeout expired
//  ACK received
int waitForACK(int packetId){
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT * 1000;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socketFd, &readSet);

    int event = select(socketFd+1, &readSet, NULL, NULL, &tv);

    if (event == -1){
        forceExit("Error happend in wait for sockets");
    }
    else if (event == 0){
        printf("TIMEOUT: in receiving ACK for packet %d\n", packetId);
        return -1;
    }
    else if (FD_ISSET(socketFd, &readSet)){
        uint8_t ACKBuffer;
        if (receivePacket(&ACKBuffer, sizeof(uint8_t)) == 1){
            if (ACKBuffer == 0x31){
                printf("SUCCESS: packet with chunkID %d sent\n", packetId);
                return 0;
            }
            else{  //ACK was not received or was 0
                printf("ERROR: ACK %c was not 1\n",ACKBuffer);
                return -1;
            }
        }
    }
    return -1;
}



void sendFile (FILE *fileFd){

    //uint8_t buffer [BUFLEN];

    uint8_t* buffer = (uint8_t*) malloc(BUFLEN);
    int bytesRead;
    int chunkId = 0;
    uint32_t crc;

    //set cursor
    fseek(fileFd, 0L, SEEK_SET);

    //read the first chunk
    while (!feof(fileFd)){

        //read chunk
        bytesRead = fread(buffer, 1, CHUNKSIZE, fileFd);
        printf("Bytes read %d\n",bytesRead);

        //calculate crc
        crc = crc32(buffer, bytesRead);

        //send chunk to server
        memcpy(buffer + bytesRead , &chunkId, sizeof(int));
        memcpy(buffer + bytesRead+ sizeof(int), &crc, sizeof(int));
        if (sendPacket(buffer,bytesRead + CONTROL) == -1){
            perror("Packet was not sent succesfully"); 
        }
        //STOP AND WAIT 
        while (waitForACK(chunkId) == -1){
            if (sendPacket(buffer,BUFLEN) == -1){
                perror("Packet was not sent succesfully"); 
            }
        }
        chunkId++;
    }
}


int main(int argc, char **argv) {
    //clock_t program_start = time(0);

    char buffer[BUFLEN]; 
    char *hello = "Hello";


    if (argc < 3) {
        printf("HELP Usage: ./sender <adress> <filename>\n");
        exit(EXIT_FAILURE);
    }

    //creating socket file descriptor 
    if ((socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) { 
        perror("Socket creation failed"); 
        exit(EXIT_FAILURE);
    } 

    // Filling server information 
    memset(&servaddr, 0, sizeof(servaddr)); //fill conf zeros
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    //if (inet_aton(SERVER, &servaddr.sin_addr) == 0) {exit(1);}



    //bind(socketfd,(const struct sockaddr*) &servaddr);

    //read file
    char* fileName = argv[2];
    FILE* fileFd = fopen(fileName, "rb");
    if (fileFd == NULL){
        perror("File decsriptor creation failed"); 
        exit(EXIT_FAILURE);
    }
    int fileSize = getFileSize(fileFd);
    printf("The file size is %d", fileSize);
    uint8_t* fileBuffer = (uint8_t*) malloc(fileSize+1);
    if (!fileBuffer){
        perror("Memory allocation error"); 
        exit(EXIT_FAILURE);
    }

    fread(fileBuffer, fileSize, 1, fileFd); 


    //Sending filename
    int fileNameLen = strlen(fileName);
    printf("Sending filename...\n");
    if (sendto(socketFd, fileName, fileNameLen, 0, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        perror("Failed to send filename\n");
    }

    //Sending filesize
    printf("Sending filesize...\n");
    if (sendto(socketFd, &fileSize, sizeof(fileSize), 0, (struct sockaddr *) &servaddr,sizeof(servaddr)) == -1) {
        perror("Failed to send file size\n");
    }

    //sending file
    printf("Sending file...\n");
    sendFile(fileFd);
    fclose(fileFd);


    //sending MD5
    printf("Sending MD5...\n");
    uint8_t *md5Hash = NULL;
    int md5Length = getmd5Hash(&md5Hash, fileBuffer, fileSize);
    printf("MD5 hash: ");
    for(int i = 0; i < md5Length; i++) printf("%x", md5Hash[i]);
    printf (" %s\n", fileName);


    //calculate crc
    uint32_t crc = crc32(md5Hash, md5Length);
    memcpy(buffer, md5Hash, md5Length);
    memcpy(buffer + md5Length , &crc, sizeof(uint32_t));
    sendPacket(buffer, md5Length + sizeof(uint32_t));
    //STOP AND WAIT 
    while (waitForACK(-1) == -1){
        if (sendPacket(buffer, md5Length) == -1){
            perror("Packet was not sent succesfully"); 
        }
    }









    /*int n; */
    /*uint32_t len;*/
    /*sendto(socketFd, (const char *)hello, strlen(hello), MSG_CONFIRM, (const struct sockaddr*) &servaddr, sizeof(servaddr)); */
    /*printf("Hello message sent.\n"); */
          
    /*n = recvfrom(socketFd, (char *)buffer, BUFLEN, MSG_WAITALL, (struct sockaddr *) &servaddr, &len); */
    /*buffer[n] = '\0'; */
    /*printf("Server : %s\n", buffer); */
  
    close(socketFd); 
    return 0; 
} 

    






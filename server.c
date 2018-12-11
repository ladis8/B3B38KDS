#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h> //close open 



#include "utils.h"


#define BUFLEN 1024 // Max length of buffer
#define CONTROL 6 //2 bytes position in file //4bytes crc
#define PACKETDATASIZE BUFLEN - CONTROL

//NetDerper ports are same as normal ports
#define PORTDATA_SERVER 9999	// Server port on which to send data
#define PORTDATA_CLIENT 9998	// Client port on which to send data
#define PORTACK_SERVER  8889 	// Server port from  which to receive ACK
#define PORTACK_CLIENT  8888 	// Client port from  which to receive ACK

#define LISTENANY "0.0.0.0"
#define ACKSIZE 1 + 2 + 4
#define DEFAULTFRAMESIZE 10


//TODO: String split of filename
//TODO: check arguments


int transmitSocketFd, receiveSocketFd; 
struct sockaddr_in client_DATA, server_DATA, client_ACK, server_ACK;

int sendPacket(uint8_t *buffer, int bufferLength){

    int bytesSend;	
    if ((bytesSend = sendto(transmitSocketFd, buffer, bufferLength, 0, (struct sockaddr *) &client_ACK, (socklen_t) sizeof(client_ACK))) == -1 ) {
        return -1;
    }
    return bytesSend;
}
int receivePacket(uint8_t *buffer, int bufferLength){

    int serverLen =sizeof(client_DATA);
    int bytesReceived; 
    //printf("DEBUG: Received bytes are %d Packet %d %d \n",bytesReceived, buffer[0], buffer[1]);
    if ((bytesReceived  = recvfrom(receiveSocketFd, buffer, bufferLength, 0, (struct sockaddr *)&client_DATA,(socklen_t *) &serverLen)) == -1) {
        return -1;
    }
    return bytesReceived;

}
void sendACK(uint8_t ACKState, uint16_t packetId){
    uint8_t sendBuffer[ACKSIZE];
    sendBuffer[0] = ACKState;
    memcpy(sendBuffer + sizeof(uint8_t), &packetId, sizeof(uint16_t));
    uint32_t crc = crc32(sendBuffer, ACKSIZE - 4);
    memcpy(sendBuffer + sizeof(uint8_t) + sizeof(uint16_t), &crc, sizeof(uint32_t));

    if (sendPacket(sendBuffer, ACKSIZE) == -1)
        perror("Packet was not sent succesfully"); 
}

//write data to packetData and returns num of data bytes
int receivePacketStopAndWait(uint8_t *packetData, uint16_t expectedPacketId){
    uint8_t receiveBuffer[BUFLEN];
    while (1) {
        uint32_t crcReceived, crcComputed; uint16_t packetIdReceived;
        int bytesReceived;
        if ((bytesReceived = receivePacket(receiveBuffer, BUFLEN)) == -1){
            perror("Revceiving packet failed");
        }
        unpackPacket(receiveBuffer, bytesReceived, packetData, &packetIdReceived, &crcReceived); 
        crcComputed = crc32(receiveBuffer, bytesReceived - sizeof(uint32_t));
        printf("DEBUG: Packet received: %d, expected packet: %d, crc received %u, crc computed: %u\n", packetIdReceived,
                expectedPacketId, crcReceived, crcComputed);
        if (crcReceived == crcComputed){
            printf("DEBUG: SUCCESS Sending positive ACK for packet %d\n", packetIdReceived);
            sendACK(1, packetIdReceived);
            if (packetIdReceived == expectedPacketId)
                return bytesReceived - CONTROL;
        }
        else{
            printf("ERROR Sending negative ACK for packet %d", packetIdReceived);
            sendACK(0, packetIdReceived);
        }
    }
    return -1;
}

FILE* receiveFileStopAndWait(FILE *fileFd, int fileSize){

    printf("INFO: Stop and wait...\n");

    int numPackets = fileSize/((int)PACKETDATASIZE) + 1;
    int expectedPacketId = 0;

    uint8_t packetData [PACKETDATASIZE];

    while (expectedPacketId < numPackets){

        int packetDataSize = receivePacketStopAndWait(packetData, expectedPacketId);
        if (packetDataSize == -1){
            perror("Packet was impossible to receive");            
        }
        fwrite(packetData, 1, packetDataSize, fileFd);
        expectedPacketId++;

    }

    return fileFd;
}

int isValueInArray(uint8_t val, uint8_t *array, int size){
    for (int i=0; i < size; i++) {
        if (array[i] == val)
            return 1;
    }
    return 0;
}

FILE* receiveFileSelectiveRepeat(FILE *fileFd,  int fileSize, int frameSize){
    printf("INFO: Selective repeat...\n");

    int numPackets = fileSize/((int)PACKETDATASIZE) + 1;
    int leastAcknowledgedPacketId= 0;

    uint8_t *receiveBuffer = (uint8_t*) malloc(BUFLEN);

    //ACKs for frame - 0 packet deliverd successfully / 1 packet not delivered yet
    uint8_t packetsBufferACKs[frameSize];
    memset(packetsBufferACKs, 1, frameSize);

    int packetsBufferLength = (frameSize < numPackets)? frameSize : numPackets;

    uint8_t **packetsBuffer = (uint8_t**) malloc(sizeof(uint8_t*) * frameSize);
    for (int i = 0; i < frameSize; i++)
        packetsBuffer[i] = (uint8_t*)malloc(sizeof(uint8_t) * BUFLEN);


    while (leastAcknowledgedPacketId < numPackets){

        uint32_t crcReceived, crcComputed; uint16_t packetIdReceived;        
        int bytesReceived;
        if ((bytesReceived = receivePacket(receiveBuffer, BUFLEN)) == -1){
            perror("Revceiving packet failed");
        }
        unpackPacket(receiveBuffer, bytesReceived, receiveBuffer, &packetIdReceived, &crcReceived); 
        crcComputed = crc32(receiveBuffer, bytesReceived - sizeof(uint32_t));
        printf("DEBUG: Packet received: %d, expected packet: (%d,%d), crc received %u, crc computed: %u\n", packetIdReceived,
                leastAcknowledgedPacketId, leastAcknowledgedPacketId + packetsBufferLength, crcReceived, crcComputed);

        //Packet was delivered OK
        int frameIndex = packetIdReceived%((int)frameSize);
        if (crcReceived == crcComputed){

            sendACK(1, packetIdReceived);
            printf("DEBUG: SUCCESS Sending positive ACK for packet %d with frame index %d\n", packetIdReceived, frameIndex);

            if (leastAcknowledgedPacketId <= packetIdReceived && packetIdReceived < leastAcknowledgedPacketId + packetsBufferLength && packetsBufferACKs[frameIndex] == 1){
                memcpy(packetsBuffer[frameIndex], receiveBuffer, bytesReceived-CONTROL);
                printf("DEBUG: Packet saved...\n");
                packetsBufferACKs[frameIndex] = 0;
            }
        }
        //Packet was delivered damaged
        else{
            sendACK(0, packetIdReceived);
            printf("DEBUG: FAIL Sending negative ACK for packet %d with frame index %d\n", packetIdReceived, frameIndex);
        }


        if (!isValueInArray(1, packetsBufferACKs, packetsBufferLength)){
           printf("DEBUG: All packets in frame from %d has been delivered\n", leastAcknowledgedPacketId);
            
           //write to file
           for (int i = 0; i < packetsBufferLength; i ++){
                int packetSize = (leastAcknowledgedPacketId + i != numPackets -1)? PACKETDATASIZE : fileSize%((int)PACKETDATASIZE);
                if (fwrite(packetsBuffer[i], 1, packetSize, fileFd) != packetSize){
                    perror("File writing was not successful!");
                    return NULL;
                }
           }

           //restore ACK buffers
          leastAcknowledgedPacketId += packetsBufferLength;
          packetsBufferLength = (leastAcknowledgedPacketId + frameSize < numPackets)? frameSize : numPackets - leastAcknowledgedPacketId;
          memset(packetsBufferACKs, 1, packetsBufferLength);
       }
     }

     return fileFd;
}




int main(int argc, char **argv) {


    if (argc < 2 ) 
        forceExit("HELP Usage: ./server <method> <frameSize - optional>\n");


    receiveSocketFd = createSocket(&server_DATA, &client_DATA, (int) PORTDATA_SERVER,(int) PORTDATA_CLIENT, LISTENANY);
    transmitSocketFd = createSocket(&server_ACK, &client_ACK, (int) PORTACK_SERVER,(int) PORTACK_CLIENT, LISTENANY); 
    printf("INFO: UDP server started...\n");

    uint8_t receiveBuffer[BUFLEN]; 

    //receive filename
    printf("INFO: Receiving file name...\n");
    int fileNameLength = receivePacketStopAndWait(receiveBuffer, -1);


    int delimeter = 0;
    for (int i = fileNameLength; i > 0; i--){
        if (receiveBuffer[i] == '/'){
            delimeter = i +1;
            break;
        }
    }
    uint8_t fileName [fileNameLength - delimeter + 1];
    memcpy(fileName, receiveBuffer + delimeter, fileNameLength - delimeter);
    fileName[fileNameLength - delimeter] = 0x0;
    


    //receive filesize
    printf("INFO: Receiving file size...\n");
    receivePacketStopAndWait(receiveBuffer,-2);
    int fileSize;
    memcpy(&fileSize, receiveBuffer, sizeof(int));

    printf("INFO: File %s will be received with size %d in total packets %d...\n", (char*)fileName, fileSize, fileSize/((int)PACKETDATASIZE) + 1);

    //receive file
    char newFileName [255];
    strcpy(newFileName,"new_");
    strcat(newFileName,(char*) fileName);
    printf("INFO: New file name: %s\n", newFileName);

    FILE *fileFd = fopen(newFileName, "wb+");
    if (fileFd == NULL)
        forceExit("File descriptor creation failed!");
    

    if (memcmp (argv[1], "0", 1) == 0)
        fileFd = receiveFileStopAndWait(fileFd, fileSize);
    if (memcmp(argv[1], "1", 1) == 0){
        int frameSize = (argc > 2 )? atoi(argv[2]): DEFAULTFRAMESIZE;
        fileFd = receiveFileSelectiveRepeat(fileFd, fileSize, frameSize);
    }

    
    fseek(fileFd, 0L, SEEK_SET);
    uint8_t fileBuffer[fileSize];
    fread(fileBuffer, 1, fileSize, fileFd);
    fclose(fileFd);

    

    //receive MD5 hash
    printf("INFO: Receiving file MD5 hash...\n");
    int md5HashLength = receivePacketStopAndWait(receiveBuffer,-3);
    uint8_t md5HashReceived [md5HashLength];
    memcpy(md5HashReceived, receiveBuffer, md5HashLength);
    printf("MDG hash received: ");
    for(int i = 0; i < md5HashLength; i++) printf("%x", md5HashReceived[i]);
    
    
    uint8_t *md5HashComputed = NULL;
    getmd5Hash(&md5HashComputed, fileBuffer, fileSize);
    printf("MD5 hash computed: ");
    for(int i = 0; i < md5HashLength; i++) printf("%x", md5HashComputed[i]);
    printf("\n");

    if (memcmp(md5HashReceived, md5HashComputed, md5HashLength) == 0)
        printf("INFO: Sucess - file has been transfered successfully!\n");
    else
        printf("Error - file is CORRUPTED!!!");

    return 0;


}

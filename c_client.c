#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h> //close open 



#include "utils.h"


#define BUFLEN 1024 // Max length of buffer
#define MAXFILESIZE 104857600 //100 MiB
#define CONTROL 8 //4 bytes position in file //4bytes crc
#define CHUNKSIZE BUFLEN - CONTROL
#define PORT 9999// The port on which to send data

#define TIMEOUT 500
#define FRAMESIZE 5 //must fit in uint8_t
#define ACKSIZE 3

//TODO: user input arguments
//TODO: ACK for sending filename and filesize

//
//
//



int socketFd; 
struct sockaddr_in myAddress, servaddr;
socklen_t myAddressLength= sizeof(myAddress);
socklen_t serverAddressLength= sizeof(servaddr);



int sendPacket(uint8_t *buffer, int bufferLength){

    int bytesSend;	
    if ((bytesSend = sendto(socketFd, buffer, bufferLength, 0, (struct sockaddr *) &servaddr, (socklen_t) serverAddressLength)) == -1 ) {
        return -1;
    }
    return bytesSend;
}
int receivePacket(uint8_t *buffer, int bufferLength){
    int bytesReceived = recvfrom(socketFd, buffer, bufferLength, 0, (struct sockaddr *)&servaddr,(socklen_t *) &myAddressLength);

    //printf("DEBUG: Received bytes are %d Packet %d %d \n",bytesReceived, buffer[0], buffer[1]);
    if ( bytesReceived == -1) {
        return -1;
    }
    return bytesReceived;

}




void restoreAcknowledgedPackets(int *ACKPackets){

    for (int i = 0; i < FRAMESIZE; i++){
        ACKPackets[i] = -1;
    }
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
            if (ACKBuffer == 1){
                printf("SUCCESS: packet with chunkID %d sent\n", packetId);
                return 0;
            }
            else{  //ACK was not received or was 0
                printf("ERROR: ACK %d was not 1\n",ACKBuffer);
                return -1;
            }
        }
    }
    return -1;
}




//OUTPUT VALUE :
//  timeout expired
//  ACK received
//ACKCounter ... number of successfully sent packets so far
int waitForACKSelectiveRepeat(int ACKCounter, int *ACKPackets, int leastAcknowledgedPacket ){

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT * 1000;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socketFd, &readSet);


	
	while (ACKCounter < FRAMESIZE){

    	int event = select(socketFd+1, &readSet, NULL, NULL, &tv);
		
		//ERROR
    	if (event == -1){
        	forceExit("Error happend in wait for sockets");
    	}

		//TIMEOUT
    	else if (event == 0){
			for (int i=0; i < FRAMESIZE; i++){
					if (ACKPackets[i] == -1)
        				printf("DEBUG: TIMEOUT in receiving ACK for packet %d  in frame from  %d\n", (leastAcknowledgedPacket +i), leastAcknowledgedPacket);
			}	
        	return -1;
		}

		//SUCCESS
		else if (FD_ISSET(socketFd, &readSet)){
			uint8_t ACKBuffer [ACKSIZE]; 
			//first 0/1 good  or bad 
			//index of ACK in frame

			if (receivePacket(ACKBuffer, ACKSIZE * sizeof(uint8_t)) != ACKSIZE)
                return -1;

            int ACKIndex = (((int)ACKBuffer[1]<< 8) | ACKBuffer[2]);
            int ACKFrameIndex = ACKIndex%((int)FRAMESIZE);
            printf("DEBUG: ACK index is %d, frameindex is %d\n", ACKIndex, ACKFrameIndex);

            if (ACKIndex < leastAcknowledgedPacket + FRAMESIZE && ACKIndex >= leastAcknowledgedPacket){
                ACKCounter++;
                if (ACKBuffer[0] == 1){
                    printf("DEBUG: SUCCESS for packet with chunkID %d\n", ACKIndex);
                    ACKPackets[ACKFrameIndex] = 1;
                }
                else{  //ACK was not received or was 0
                    printf("DEBUG: ERROR for packet with chunk %d - ACK was 0\n", ACKIndex);
                }
                
            }
            //else WHAT  TO DO???
		}

	}

	return 0;
}





void sendFilebySelectiveRepeat(uint8_t *fileBuffer, int fileSize){
    
    int ACKPackets [FRAMESIZE];
	int ACKCounter = 0;
    restoreAcknowledgedPackets(ACKPackets);


    //count needed packets 
    int numPackets = fileSize/((int)CHUNKSIZE) + 1;

    int leastAcknowledgedPacket = 0;
    printf ("Total number of packets will be %d\n", numPackets);
    
    //create send buffer
    uint8_t* sendBuffer = (uint8_t*) malloc(BUFLEN);
    if (!sendBuffer){
        forceExit("Memory allocation failed!");
    }

    FILE* fileFd = fopen("ahoj.pdf", "wb");

    fwrite(fileBuffer, fileSize, 1, fileFd);
    int failesCounter = 0;


    
    while (leastAcknowledgedPacket < numPackets){

        uint32_t crc;

        //send as many packets as unacknowledged in frame
        for (int i = 0; i < FRAMESIZE; i++){
            if (ACKPackets[i] == -1){

                //send chunk to server
                int packetId = leastAcknowledgedPacket + i;
                int packetDataSize = (packetId == numPackets -1)? fileSize%((int)CHUNKSIZE) : CHUNKSIZE;
                memcpy(sendBuffer, fileBuffer + packetId * ((int)CHUNKSIZE), packetDataSize);

                //calculate crc
                crc = crc32(sendBuffer, packetDataSize);
                memcpy(sendBuffer + packetDataSize, &packetId, sizeof(int));
                memcpy(sendBuffer + packetDataSize+ sizeof(int), &crc, sizeof(int));

                if (sendPacket(sendBuffer,packetDataSize + CONTROL) == -1){
                    perror("Packet was not sent succesfully"); 
                }
            }
            else
                failesCounter++;

        }

        //wait for the ACKs
		waitForACKSelectiveRepeat(ACKCounter, ACKPackets, leastAcknowledgedPacket);

		//get number of so far successfully sent packets
		int ACKCounter = 0;
		for (int i = 0; i < FRAMESIZE; i++){
            printf("%d ", ACKPackets[i]);
			if (ACKPackets[i] == 1) ACKCounter++;
		}
        printf ("ACK counter %d\n", ACKCounter);
		

		//check end of frame		
		if (ACKCounter == FRAMESIZE){
			leastAcknowledgedPacket += FRAMESIZE;
			
			//check end of communication
			//if communication is at the end, send only the remaining number of packets
			if (leastAcknowledgedPacket + FRAMESIZE > numPackets){
				int numPacketsToRemain = numPackets - leastAcknowledgedPacket;
				for (int i = 0; i < numPacketsToRemain; i++){
					ACKPackets[i] = -1;
				}
			}
			else
				restoreAcknowledgedPackets(ACKPackets);
		}


	}
    printf ("Failes counter %d\n", failesCounter);

		
		


    


    
}


int main(int argc, char **argv) {
    //clock_t program_start = time(0);


    
    uint8_t sendBuffer[BUFLEN]; 

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




    //read file
    char* fileName = argv[2];
    FILE* fileFd = fopen(fileName, "rb");
    if (fileFd == NULL){
        forceExit("File descriptor creation failed!");
    }
    int fileSize = getFileSize(fileFd);
    printf("The file size is %d\n", fileSize);
    uint8_t* fileBuffer = (uint8_t*) malloc(fileSize);
    if (!fileBuffer){
        forceExit("Memory allocation failed!");
    }

    fread(fileBuffer, fileSize, 1, fileFd); 
    fclose(fileFd);


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
    sendFilebySelectiveRepeat(fileBuffer, fileSize);


    //sending MD5
    printf("Sending MD5...\n");
    uint8_t *md5Hash = NULL;
    int md5Length = getmd5Hash(&md5Hash, fileBuffer, fileSize);
    printf("MD5 hash: ");
    for(int i = 0; i < md5Length; i++) printf("%x", md5Hash[i]);
    printf (" %s\n", fileName);


    //calculate crc
    uint32_t crc = crc32(md5Hash, md5Length);
    memcpy(sendBuffer, md5Hash, md5Length);
    memcpy(sendBuffer + md5Length , &crc, sizeof(uint32_t));
    sendPacket(sendBuffer, md5Length + sizeof(uint32_t));
    //STOP AND WAIT 
    while (waitForACK(-1) == -1){
        if (sendPacket(sendBuffer, md5Length) == -1){
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

    






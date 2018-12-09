#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h> //close open 



#include "utils.h"


#define BUFLEN 1024 // Max length of buffer
#define CONTROL 6 //2 bytes position in file //4bytes crc
#define PACKETDATASIZE BUFLEN - CONTROL

//NetDerper ports
#define PORTDATA_SERVER 9998	// Server port on which to send data
#define PORTDATA_CLIENT 9997	// Client port on which to send data
#define PORTACK_SERVER  8888 	// Server port from  which to receive ACK
#define PORTACK_CLIENT  8887 	// Client port from  which to receive ACK

/*#define PORTDATA_SERVER 9999	// Server port on which to send data*/
/*#define PORTDATA_CLIENT 9998	// Client port on which to send data*/
/*#define PORTACK_SERVER  8889 	// Server port from  which to receive ACK*/
/*#define PORTACK_CLIENT  8888 	// Client port from  which to receive ACK*/


#define TIMEOUT 200
#define FRAMESIZE 10 //must fit in uint8_t
#define ACKSIZE 1 + 2 + 4

//TODO: user input arguments


int transmitSocketFd, receiveSocketFd; 
struct sockaddr_in client_DATA, server_DATA, client_ACK, server_ACK;

int sendPacket(uint8_t *buffer, int bufferLength){

    int bytesSend;	
    if ((bytesSend = sendto(transmitSocketFd, buffer, bufferLength, 0, (struct sockaddr *) &server_DATA, (socklen_t) sizeof(server_DATA))) == -1 ) {
        return -1;
    }
    return bytesSend;
}
int receivePacket(uint8_t *buffer, int bufferLength){

    int serverLen =sizeof(server_ACK);
    int bytesReceived; 
    //printf("DEBUG: Received bytes are %d Packet %d %d \n",bytesReceived, buffer[0], buffer[1]);
    if ((bytesReceived  = recvfrom(receiveSocketFd, buffer, bufferLength, 0, (struct sockaddr *)&server_ACK,(socklen_t *) &serverLen)) == -1) {
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
int waitForACK(uint16_t packetId){
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT * 1000;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(receiveSocketFd, &readSet);

    int event = select(receiveSocketFd+1, &readSet, NULL, NULL, &tv);

    if (event == -1){
        forceExit("Error happend in wait for sockets");
    }
    else if (event == 0){
        printf("TIMEOUT: in receiving ACK for packet %d\n",packetId);
        return -1;
    }
    else if (FD_ISSET(receiveSocketFd, &readSet)){

        uint8_t packetBuffer[ACKSIZE];
        uint32_t crcReceived; uint16_t packetIdReceived; uint8_t ACKReceived;
        
        //not enough bytes came
        if (receivePacket(packetBuffer, ACKSIZE) != ACKSIZE){
            printf("ERROR: Not %d bytes were received when ACK\n", ACKSIZE);
            return -1;
        }

        unpackPacket(packetBuffer, ACKSIZE, &ACKReceived, &packetIdReceived, &crcReceived); 
        //printf("DEBUG: Wait for %u packet ACK - ACK packet %u %u %u\n", packetId, ACKReceived, packetIdReceived, crcReceived);

        //negative ACK or corrupted packet
        if (ACKReceived != 1 || crcReceived != crc32(packetBuffer, ACKSIZE - sizeof(uint32_t))){
            printf("ERROR: packet %u  - ACK %u was not positive\n",packetId, ACKReceived);
            return -1;
        }
        //packet ACK came for earlier packet
        if (packetId != packetIdReceived){
            printf("DEBUG: ACK for earlier packet - expected %u received %u\n", packetId, packetIdReceived);
            return -1;
        }
        else{
            printf("DEBUG: Success packet with packetId %u sent successfully \n", packetId);
            return 0;
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
    FD_SET(receiveSocketFd, &readSet);


	
	while (ACKCounter < FRAMESIZE){

    	int event = select(receiveSocketFd+1, &readSet, NULL, NULL, &tv);
		
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
		else if (FD_ISSET(receiveSocketFd, &readSet)){

			uint8_t packetBuffer [ACKSIZE]; 
            uint32_t crcReceived; uint16_t packetIdReceived; uint8_t ACKReceived;

            //not enough bytes came
            if (receivePacket(packetBuffer, ACKSIZE) != ACKSIZE){
                printf("ERROR: Not %d bytes were received when ACK\n", ACKSIZE);
                return -1;
            }

            unpackPacket(packetBuffer, ACKSIZE, &ACKReceived, &packetIdReceived, &crcReceived); 
            printf("DEBUG: Wait for packet ACK (%u to %u) - ACK packet %u %u %u\n", leastAcknowledgedPacket, leastAcknowledgedPacket + FRAMESIZE, ACKReceived, packetIdReceived, crcReceived);


            //negative ACK or corrupted packet
            ACKCounter++;
            if (ACKReceived != 1 || crcReceived != crc32(packetBuffer, ACKSIZE - sizeof(uint32_t))){
                printf("ERROR: packet %u  - ACK %u was not positive\n",packetIdReceived, ACKReceived);

            }
            else{
                //packet ACK came for earlier packet
                if (leastAcknowledgedPacket + FRAMESIZE <= packetIdReceived || packetIdReceived < leastAcknowledgedPacket ){
                    printf("DEBUG: ACK for earlier packet -expected (%u  to %u) received %u\n", leastAcknowledgedPacket, leastAcknowledgedPacket + FRAMESIZE, packetIdReceived);
                }
                else{

                    int frameIndex = packetIdReceived%((int)FRAMESIZE);
                    ACKPackets[frameIndex] = 1;
                    printf("DEBUG: Success packet with packetId %u sent successfully \n", packetIdReceived);
                }
            }
            
		}

	}

	return 0;
}

int sendPacketStopAndWait(uint8_t *packet, int packetId, int packetSize){
        //STOP AND WAIT 
        int failesCounter = 0;
		do{
			if (sendPacket(packet, packetSize) == -1){
				perror("Packet was not sent succesfully"); 
                failesCounter++;
            }
        }while (waitForACK((uint16_t)packetId) == -1);
        return failesCounter;
}





void sendFilebySelectiveRepeat(uint8_t *fileBuffer, int fileSize){
    
    int ACKPackets [FRAMESIZE];
	int ACKCounter = 0;
    restoreAcknowledgedPackets(ACKPackets);


    //count needed packets 
    int numPackets = fileSize/((int)PACKETDATASIZE) + 1;
    int failesCounter = 0;

    int leastAcknowledgedPacket = 0;
    printf ("Total number of packets will be %d\n", numPackets);
    
    //create send buffer
    uint8_t* sendBuffer = (uint8_t*) malloc(BUFLEN);
    if (!sendBuffer){
        forceExit("Memory allocation failed!");
    }


    
    while (leastAcknowledgedPacket < numPackets){

        uint32_t crc;

        //send as many packets as unacknowledged in frame
        for (int i = 0; i < FRAMESIZE; i++){
            if (ACKPackets[i] == -1){

                //send chunk to server
                uint16_t packetId = leastAcknowledgedPacket + i;
                int packetDataSize = (packetId == numPackets -1)? fileSize%((int)PACKETDATASIZE) : PACKETDATASIZE;
                memcpy(sendBuffer, fileBuffer + packetId * ((int)PACKETDATASIZE), packetDataSize);
                memcpy(sendBuffer + packetDataSize, &packetId, sizeof(uint16_t));

                //calculate crc
                crc = crc32(sendBuffer, packetDataSize + sizeof(uint16_t));
                memcpy(sendBuffer + packetDataSize + sizeof(uint16_t), &crc, sizeof(int));

                if (sendPacket(sendBuffer,packetDataSize + CONTROL) == -1){
                    perror("Packet was not sent succesfully"); 
                }
                printf("DEBUG: Sending packet %u\n", packetId);
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
            printf("\n");
		}


	}
    printf ("Failes counter %d\n", failesCounter);
    
}

int createSocket(struct sockaddr_in *src, struct sockaddr_in *dest, int portSrc, int portDest, char* address) {

	int socketFd;
    if ((socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
		forceExit("Socket creation failed");

	

    // Filling server information 
    memset(dest, 0, sizeof(*dest)); //fill conf zeros
    (*dest).sin_family = AF_INET; 
    (*dest).sin_port = htons(portDest); 
    (*dest).sin_addr.s_addr = inet_addr(address);
    //if (inet_aton(SERVER, &server.sin_addr) == 0) {exit(1);}

	// Filling client information 
	memset(src, 0, sizeof(*src));
    (*src).sin_family = AF_INET;
    (*src).sin_addr.s_addr = htonl(INADDR_ANY);
    (*src).sin_port = htons(portSrc);

	if (bind(socketFd, (struct sockaddr *) src, sizeof(*src)) < 0)
		forceExit("Socekt bind failed");
	return socketFd;
}
   


int main(int argc, char **argv) {


    if (argc < 3) 
        forceExit("HELP Usage: ./sender <adress> <filename>\n");
    

	transmitSocketFd = createSocket(&client_DATA, &server_DATA, (int) PORTDATA_CLIENT,(int) PORTDATA_SERVER, argv[1]);
	receiveSocketFd  = createSocket(&client_ACK, &server_ACK, (int) PORTACK_CLIENT,(int) PORTACK_SERVER, argv[1]); 



    //read file
    char* fileName = argv[2];
    FILE* fileFd = fopen(fileName, "rb");
    if (fileFd == NULL)
        forceExit("File descriptor creation failed!");
    
    int fileSize = getFileSize(fileFd);
    printf("The file size is %d\n", fileSize);
    uint8_t* fileBuffer = (uint8_t*) malloc(fileSize);
    if (!fileBuffer)
        forceExit("Memory allocation failed!");
    

    fread(fileBuffer, fileSize, 1, fileFd); 
    fclose(fileFd);


    uint32_t crc;

    //Sending filename
	uint16_t packetId = -1; //control packet
    uint8_t sendBuffer[BUFLEN]; 
    int fileNameLength = strlen(fileName);
    printf("Sending filename...\n");
    
    memcpy(sendBuffer, fileName, fileNameLength);
    memcpy(sendBuffer + fileNameLength, &packetId, sizeof(uint16_t));
    crc = crc32(sendBuffer, fileNameLength + sizeof(uint16_t));
    memcpy(sendBuffer + fileNameLength + sizeof(uint16_t), &crc, sizeof(int));

    sendPacketStopAndWait(sendBuffer, packetId, fileNameLength + CONTROL);

    //Sending filesize
	packetId = -2; //control packet
    printf("Sending filesize...\n");
    memcpy(sendBuffer, &fileSize, sizeof(int));
    memcpy(sendBuffer + sizeof(int), &packetId, sizeof(uint16_t));
    crc = crc32(sendBuffer, sizeof(int) + sizeof(uint16_t));
    memcpy(sendBuffer + sizeof(int) + sizeof(uint16_t), &crc, sizeof(int));

    sendPacketStopAndWait(sendBuffer, packetId, sizeof(int) + CONTROL);

    //sending file
    printf("Sending file...\n");
    sendFilebySelectiveRepeat(fileBuffer, fileSize);


    //sending MD5
	packetId = -3; //control packet
    printf("Sending MD5...\n");
    uint8_t *md5Hash = NULL;
    int md5Length = getmd5Hash(&md5Hash, fileBuffer, fileSize);
    printf("MD5 hash: ");
    for(int i = 0; i < md5Length; i++) printf("%x", md5Hash[i]);
    printf (" %s\n", fileName);



    memcpy(sendBuffer, md5Hash, md5Length);
    memcpy(sendBuffer + md5Length , &packetId, sizeof(uint16_t));
    crc = crc32(sendBuffer, md5Length + sizeof(uint16_t));
    memcpy(sendBuffer + md5Length + sizeof(uint16_t), &crc, sizeof(uint32_t));

    sendPacketStopAndWait(sendBuffer, packetId, md5Length + CONTROL);

    close(receiveSocketFd); 
	close(transmitSocketFd);
    return 0; 
} 

    






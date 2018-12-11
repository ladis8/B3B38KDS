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

#ifdef NETDERPER
    //NetDerper ports
    #define PORTDATA_SERVER 9998	// Server port on which to send data
    #define PORTDATA_CLIENT 9997	// Client port on which to send data
    #define PORTACK_SERVER  8888 	// Server port from  which to receive ACK
    #define PORTACK_CLIENT  8887 	// Client port from  which to receive ACK
#else
    #define PORTDATA_SERVER 9999	// Server port on which to send data*/
    #define PORTDATA_CLIENT 9998	// Client port on which to send data<]
    #define PORTACK_SERVER  8889 	// Server port from  which to receive ACK
    #define PORTACK_CLIENT  8888 	// Client port from  which to receive ACK
#endif 



#define TIMEOUT 400
#define DEFAULTFRAMESIZE 10 //must fit in uint8_t
#define ACKSIZE 1 + 2 + 4


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

void restoreAcknowledgedPackets(int *ACKPackets, int leastAcknowledgedPacket, int frameSize, int numPackets){

		//check end of communication
		//if communication is at the end, send only the remaining number of packets
		int packetsBufferLength = (leastAcknowledgedPacket + frameSize > numPackets)? numPackets - leastAcknowledgedPacket : frameSize;

	    for (int i = 0; i < frameSize; i++)
				ACKPackets[i] = (i < packetsBufferLength)? -1 : 1 ;
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
int waitForACKSelectiveRepeat(int ACKCounter, int *ACKPackets, int leastAcknowledgedPacket, int frameSize ){

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT * 1000;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(receiveSocketFd, &readSet);


	
	while (ACKCounter < frameSize){

    	int event = select(receiveSocketFd+1, &readSet, NULL, NULL, &tv);
		
		//ERROR
    	if (event == -1){
        	forceExit("Error happend in wait for sockets");
    	}

		//TIMEOUT
    	else if (event == 0){
			for (int i=0; i < frameSize; i++){
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
            printf("DEBUG: Wait for packet ACK (%u to %u) - ACK packet %u %u %u\n", leastAcknowledgedPacket, leastAcknowledgedPacket + frameSize, ACKReceived, packetIdReceived, crcReceived);


            //negative ACK or corrupted packet
            ACKCounter++;
            if (ACKReceived != 1 || crcReceived != crc32(packetBuffer, ACKSIZE - sizeof(uint32_t))){
                printf("ERROR: packet %u  - ACK %u was not positive\n",packetIdReceived, ACKReceived);

            }
            else{
                //packet ACK came for earlier packet
                if (leastAcknowledgedPacket + frameSize <= packetIdReceived || packetIdReceived < leastAcknowledgedPacket ){
                    printf("DEBUG: ACK for earlier packet -expected (%u  to %u) received %u\n", leastAcknowledgedPacket, leastAcknowledgedPacket + frameSize, packetIdReceived);
                }
                else{

                    int frameIndex = packetIdReceived%((int)frameSize);
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





void sendFilebySelectiveRepeat(uint8_t *fileBuffer, int fileSize, int frameSize){
   
    int leastAcknowledgedPacket = 0;

    //count needed packets 
    int numPackets = fileSize/((int)PACKETDATASIZE) + 1;
    int failesCounter = 0;

    //check frameSize 
    if (frameSize > numPackets) 
        frameSize = numPackets;

    int ACKPackets [frameSize];
	int ACKCounter = 0;
    restoreAcknowledgedPackets(ACKPackets, leastAcknowledgedPacket, frameSize, numPackets);

    printf ("Total number of packets will be %d\n", numPackets);
    
    //create send buffer
    uint8_t* sendBuffer = (uint8_t*) malloc(BUFLEN);
    if (!sendBuffer){
        forceExit("Memory allocation failed!");
    }


    
    while (leastAcknowledgedPacket < numPackets){

        uint32_t crc;

        //send as many packets as unacknowledged in frame
        for (int i = 0; i < frameSize; i++){
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
		waitForACKSelectiveRepeat(ACKCounter, ACKPackets, leastAcknowledgedPacket, frameSize);

		//get number of so far successfully sent packets
		int ACKCounter = 0;
		for (int i = 0; i < frameSize; i++){
            printf("%d ", ACKPackets[i]);
			if (ACKPackets[i] == 1) ACKCounter++;
		}
        printf ("ACK counter %d\n", ACKCounter);
		

		//check end of frame		
		if (ACKCounter == frameSize){
			leastAcknowledgedPacket += frameSize;
            
			restoreAcknowledgedPackets(ACKPackets, leastAcknowledgedPacket, frameSize, numPackets);
            printf("All packets in FRAME has been acknowleadged\n\n");
		}


	}
    printf ("Failes counter %d\n", failesCounter);
    
}



int main(int argc, char **argv) {


    if (argc < 3) 
        forceExit("HELP Usage: ./client_SelectiveRepeat <adress> <filename> <framesize - optional>\n");
    

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
    printf("Sending file name...\n");
    
    memcpy(sendBuffer, fileName, fileNameLength);
    memcpy(sendBuffer + fileNameLength, &packetId, sizeof(uint16_t));
    crc = crc32(sendBuffer, fileNameLength + sizeof(uint16_t));
    memcpy(sendBuffer + fileNameLength + sizeof(uint16_t), &crc, sizeof(int));

    sendPacketStopAndWait(sendBuffer, packetId, fileNameLength + CONTROL);




    //Sending filesize
	packetId = -2; //control packet
    printf("Sending file size...\n");
    memcpy(sendBuffer, &fileSize, sizeof(int));
    memcpy(sendBuffer + sizeof(int), &packetId, sizeof(uint16_t));
    crc = crc32(sendBuffer, sizeof(int) + sizeof(uint16_t));
    memcpy(sendBuffer + sizeof(int) + sizeof(uint16_t), &crc, sizeof(int));

    sendPacketStopAndWait(sendBuffer, packetId, sizeof(int) + CONTROL);
    



    //sending file
    int frameSize = (argc > 3 )? atoi(argv[3]): DEFAULTFRAMESIZE;
    time_t t = time(NULL); 
    printf("Sending file with frame size %d...\n", frameSize);
    sendFilebySelectiveRepeat(fileBuffer, fileSize, frameSize);




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
    t = time(NULL) - t; 
    printf("File transfer took approximately %f seconds\n", (double)t);


    close(receiveSocketFd); 
	close(transmitSocketFd);
    return 0; 
} 

    






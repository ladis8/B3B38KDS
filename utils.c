#include "utils.h"

uint32_t crc32(uint8_t *message, int buffSize) {
  int i, j;
  unsigned int byte, crc, mask;

  crc = 0xFFFFFFFF;
  for (i = 0; i < buffSize; ++i) {
    byte = message[i]; // Get next byte.
    crc = crc ^ byte;
    for (j = 7; j >= 0; j--) { // Do eight times.
      mask = -(crc & 1);
      crc = (crc >> 1) ^ (0xEDB88320 & mask);
    }
  }
  return ~crc;
}




int getmd5Hash(uint8_t **md5Buffer, uint8_t *buffer, int bufferLength){
	MD5_CTX md5;
    *md5Buffer = (uint8_t*) malloc(MD5_DIGEST_LENGTH);

	if (MD5_Init(&md5) == 0) forceExit("MD5 init failed");
	MD5_Update(&md5, buffer, bufferLength); 
	MD5_Final(*md5Buffer, &md5);
	return MD5_DIGEST_LENGTH;
}

/**
 * Function that unpacks a pacet to data, chunkId and crc.
 *
 * @author Ladislav Stefka 
 */
void unpackPacket(uint8_t *packet, int packetLength, uint8_t *packetData, uint16_t *chunkId, uint32_t *crc){
    int packetDataLength = packetLength - sizeof(uint16_t) - sizeof(uint32_t);
    memcpy(packetData, packet, packetDataLength);
    memcpy(chunkId, packet + packetDataLength, sizeof(uint16_t)); 
    memcpy(crc, packet + packetDataLength + sizeof(uint16_t), sizeof(uint32_t));
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
    printf("INFO: Socket bind %d %u\n", socketFd, portSrc);
	memset(src, 0, sizeof(*src));
    (*src).sin_family = AF_INET;
    (*src).sin_addr.s_addr = htonl(INADDR_ANY);
    (*src).sin_port = htons(portSrc);

	if (bind(socketFd, (struct sockaddr *) src, sizeof(*src)) < 0)
		forceExit("Socekt bind failed");
	return socketFd;
}



        


        


int getFileSize(FILE *fp) {
    fseek(fp, 0L, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return size;
}


void forceExit(char *message){
	perror(message);
	exit(EXIT_FAILURE);
}
	

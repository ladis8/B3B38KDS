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
	
	

void forceExit(char *message){
	perror(message);
	exit(EXIT_FAILURE);
}
	

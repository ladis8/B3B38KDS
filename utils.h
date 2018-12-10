#ifndef UTILS_H_
#define UTILS_H_ 

#define AES_BLOCK_SIZE 16

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> //uint types
#include <string.h> //memcpy
#include <openssl/md5.h> //md5
#include <sys/socket.h> 
#include <arpa/inet.h>

uint32_t crc32(uint8_t *message, int bufSize);

int getmd5Hash(uint8_t **md5buffer, uint8_t *buffer, int bufferlength);


int getFileSize(FILE *fp);

void forceExit(char *message);


void unpackPacket(uint8_t *packet, int packetLength, uint8_t *packetData, uint16_t *chunkId, uint32_t *crc);


int createSocket(struct sockaddr_in *src, struct sockaddr_in *dest, int portSrc, int portDest, char* address);


#endif

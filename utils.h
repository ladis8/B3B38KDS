#ifndef UTILS_H_
#define UTILS_H_ 

#define AES_BLOCK_SIZE 16

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> //uint types
#include <string.h> //memcpy
#include <openssl/md5.h>

uint32_t crc32(uint8_t *message, int bufSize);

int getmd5Hash(uint8_t **md5buffer, uint8_t *buffer, int bufferlength);

void unpackPacket(uint8_t *packet, int packetLength, uint8_t *packetData, uint16_t *chunkId, uint32_t *crc);

int getFileSize(FILE *fp);

void forceExit(char *message);


#endif

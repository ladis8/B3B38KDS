
int getFileSize(FILE *fp) {
  fseek(fp, 0L, SEEK_END);
  int size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);
  return size;
}

int getFileChunk(FILE *fp, uint8_t* buffer, int chunkId, int chunkSize){


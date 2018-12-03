# ----- receiver.py -----

#!/usr/bin/env python3

#!/usr/bin/env python3
import socket
import os
import struct
import sys
import hashlib
import binascii
import random
import logging

DEBUG = True


def Main():
    host = '127.0.0.1'
    port = 9999
    BUFLEN = 1024
    CONTROL = 8
    CHUNK_SIZE = BUFLEN - CONTROL

    ACK = "1"
    NOT_ACK = "0"

    if DEBUG: logging.getLogger().setLevel(logging.DEBUG)

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind((host, port))


    print("UDP server Started")

    while True:

        #receive filename
        data, addr = s.recvfrom(1024)
        print("Connection from: " + str(addr), "data ", data)

        #receive filesize
        data, addr = s.recvfrom(1024)
        size =  struct.unpack("I", data)[0]
        print("Connection from: " + str(addr), "data ", size)
        chunks_num = size//CHUNK_SIZE +1


        #receive file

        with open("received_file", "wb") as f_rec:
            md5_hash = hashlib.md5()
            next_chunk_id = 0
            while True:
                #receive data
                data, addr = s.recvfrom(1024)

                #unpack data
                data = bytearray(data)
                crc_received =  struct.unpack("I", bytes(data [-4:]))[0]
                chunk_id = struct.unpack("I", bytes(data [-8:-4]))[0]
                file_data = data[:-8]

                #compute crc
                crc_computed = binascii.crc32(file_data)

                logging.debug("Chunk received: %d, expected chunk: %d, crc received %d, crc computed: %d ", chunk_id, next_chunk_id, crc_received, crc_computed)
                if crc_computed == crc_received and next_chunk_id == chunk_id and random.random() > 0.1 :
                    #send positive ACK
                    logging.debug("SUCCESS: Sending positive ACK")
                    s.sendto(bytes(ACK, "ascii"), addr)

                    #write data to file
                    f_rec.write(file_data)
                    md5_hash.update(file_data)

                    next_chunk_id +=1
                    if chunk_id == chunks_num-1:
                        print(file_data)
                        break
                else:
                    logging.debug("ERROR: Sending negative ACK")
                    s.sendto(bytes(NOT_ACK, "ascii"), addr)



            #receive md5
            data, addr = s.recvfrom(1024)
            crc_received =  struct.unpack("I", bytes(data [-4:]))[0]
            crc_computed = binascii.crc32(data[:-4])
            md5_hash_received= binascii.hexlify(data[:-4])
            md5_hash_computed = binascii.hexlify(md5_hash.digest())

            logging.info("MD5 hash received : %s, computed MD5 hash: %s, crc received %d, crc computed: %d", md5_hash_received, md5_hash_computed, crc_received, crc_computed)

            if crc_computed == crc_received:

                #send positive ACK
                logging.debug("SUCCESS: Sending positive ACK")
                s.sendto(bytes(ACK, "ascii"), addr)
                if md5_hash_computed == md5_hash_received:
                    logging.info("File has been transfered successfully")
                    s.close()
                    break
                else:
                    logging.error("File is CORRUPTED!!!")
            else:
                logging.debug("ERROR: Sending negative ACK")
                s.sendto(bytes(NOT_ACK, "ascii"), addr)





if __name__ == '__main__':
    Main()
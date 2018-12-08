# ----- receiver.py -----

# !/usr/bin/env python3

# !/usr/bin/env python3
import socket
import sys
import hashlib
import binascii
import random
import logging
from struct import pack
from struct import unpack

DEBUG = True

host = '0.0.0.0'
port = 9999
BUFLEN = 1024
CONTROL = 8
CHUNK_SIZE = BUFLEN - CONTROL

ERROR_DATA = 0.1
ERROR_ACK_LOST = 0.05

def sendACK(s, addr, positive, chunk_id=None):

    ACK = 1
    NOT_ACK = 0
    #simulate lost ACK
    if random.random() < ERROR_ACK_LOST:
        logging.debug("ACK LOST...")
        return

    # send positive ACK - send chunk_id as 2bytes (H)
    if positive:
        s.sendto(pack('b', ACK) + pack('>H', chunk_id), addr) if chunk_id is not None else s.sendto(pack('b', ACK), addr)
    else:
        s.sendto(pack('b', NOT_ACK) + pack('>H', chunk_id), addr) if chunk_id is not None else s.sendto(pack('b', NOT_ACK), addr)


def unpack_packet(data):
    crc_received = unpack("I", bytes(data[-4:]))[0]
    chunk_id = unpack("i", bytes(data[-8:-4]))[0]
    file_data = data[:-8]
    return chunk_id, crc_received, file_data

def check_md5(socket, md5_hash):
    logging.info("Waiting for receiving MD5 hash")

    while True:
        data, addr = socket.recvfrom(1024)
        chunk_id, crc_received, file_data = unpack_packet(data)

        crc_computed = binascii.crc32(file_data)

        if crc_computed == crc_received:
            #delayed data packet was received
            if chunk_id != -1:
                logging.debug("SUCCESS Sending positive ACK for packet %d", chunk_id)
                sendACK(socket, addr, True, chunk_id)
            else:
                md5_hash_received = binascii.hexlify(file_data)
                md5_hash_computed = binascii.hexlify(md5_hash.digest())

                logging.info("MD5 hash received : %s, computed MD5 hash: %s, crc received %d, crc computed: %d", md5_hash_received,
                     md5_hash_computed, crc_received, crc_computed)
                logging.debug("SUCCESS: Sending positive ACK for MD5 hash")
                sendACK(socket, addr, True)
                if md5_hash_computed == md5_hash_received:
                    logging.info("File has been transfered successfully")
                else:
                    logging.info("File is CORRUPTED!!!")
                break
        else:
            logging.debug("ERROR: Sending negative ACK")
            sendACK(socket, addr, False)



def receive_file_SelectiveRepeat(socket, file_size, file_name, FRAMESIZE):
    packet_buffer = [None] * FRAMESIZE

    num_packets = file_size//CHUNK_SIZE + 1

    with open("new_" + file_name, "wb") as f_rec:
        md5_hash = hashlib.md5()
        least_acknowledged_packet_id = 0
        while least_acknowledged_packet_id < num_packets:

            # receive data
            data, addr = socket.recvfrom(BUFLEN)
            chunk_id, crc_received, file_data = unpack_packet(data)

            # compute crc
            crc_computed = binascii.crc32(file_data)

            logging.debug("Expected chunk: from %d to %d, chunk received %d, crc received %d, crc computed: %d ",
                          least_acknowledged_packet_id, least_acknowledged_packet_id + FRAMESIZE, chunk_id,
                          crc_received, crc_computed)


            #send ACK
            if crc_computed == crc_received and random.random() > ERROR_DATA: #simulate bad CRC
                logging.debug("SUCCESS Sending positive ACK for packet %d", chunk_id)
                sendACK(socket, addr, True, chunk_id)
                if least_acknowledged_packet_id <= chunk_id < least_acknowledged_packet_id + FRAMESIZE:
                    packet_buffer[chunk_id%FRAMESIZE] = file_data
            else:
                logging.debug("ERROR Sending negative ACK for packet %d",  chunk_id)
                sendACK(socket, addr, False, chunk_id)

            # check that all packets in frame has been delivered
            if not None in packet_buffer:
                logging.debug("All packets in frame from %d has been delivered", least_acknowledged_packet_id)
                least_acknowledged_packet_id += FRAMESIZE

                #write to file
                for i, packet in enumerate(packet_buffer):
                    f_rec.write(packet)
                    md5_hash.update(packet)

                #update packet buffer
                buffers_num = FRAMESIZE if least_acknowledged_packet_id + FRAMESIZE < num_packets else num_packets - least_acknowledged_packet_id
                packet_buffer = [None] * buffers_num


    check_md5(socket, md5_hash)

def receive_file_StopAndWait(socket, file_size, file_name):

    num_packets = file_size//CHUNK_SIZE + 1

    with open("new_" + file_name, "wb") as f_rec:
        md5_hash = hashlib.md5()
        next_chunk_id = 0
        while next_chunk_id < num_packets:
            # receive data
            data, addr = socket.recvfrom(1024)

            # unpack data
            data = bytearray(data)
            crc_received = unpack("I", bytes(data[-4:]))[0]
            chunk_id = unpack("I", bytes(data[-8:-4]))[0]
            file_data = data[:-8]

            # compute crc
            crc_computed = binascii.crc32(file_data)

            logging.debug("Chunk received: %d, expected chunk: %d, crc received %d, crc computed: %d ", chunk_id,
                          next_chunk_id, crc_received, crc_computed)

            #packet OK
            if crc_computed == crc_received and random.random() > ERROR_DATA:
                # send positive ACK
                logging.debug("SUCCESS Sending positive ACK for packet %d", chunk_id)
                sendACK(socket, addr, True)

                # write data to file only if the needed packet has been received
                if chunk_id == next_chunk_id:
                    f_rec.write(file_data)
                    md5_hash.update(file_data)

                    next_chunk_id += 1
            else:
                logging.debug("ERROR Sending negative ACK for packet %d", chunk_id)
                sendACK(socket, addr, False)

        # receive md5
        check_md5(socket, md5_hash)

def Main():
    logging.getLogger().setLevel(logging.DEBUG) if DEBUG else logging.getLogger().setLevel(logging.INFO)

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind((host, port))
    logging.info("UDP server Started...")

    # receive filename
    data, addr = s.recvfrom(1024)
    logging.info("Connection from %s", addr)
    file_name = data.decode("ascii").split("/")[-1]

    # receive filesize
    data, addr = s.recvfrom(1024)
    file_size = unpack("I", data)[0]

    logging.info("File %s with size %d will be received", file_name, file_size)

    # receive file
    method = int(sys.argv[1])

    if method == 1:
        FRAMESIZE = int(sys.argv[2])
        receive_file_SelectiveRepeat(s, file_size, file_name, FRAMESIZE)
    elif method == 0:
        receive_file_StopAndWait(s, file_size, file_name)

if __name__ == '__main__':
    Main()

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
PORTDATA_SERVER = 9999
PORTACK_SERVER  = 8889
PORTACK_CLIENT  = 8888

BUFLEN = 1024
CONTROL = 6
CHUNK_SIZE = BUFLEN - CONTROL
CONTROLPACKET_ID = 65535 #max of uint16_t

ERROR_DATA = 0.0
ERROR_ACK_LOST = 0.0

transmit_socket = receive_socket = None


def sendACK(addr, positive, chunk_id):
    ACK = 1
    NOT_ACK = 0
    #simulate lost ACK
    if random.random() < ERROR_ACK_LOST:
        logging.debug("ACK LOST...")
        return

    #create message (uint8_t, uint16_t, uint32_t)
    message = pack('b', ACK) + pack('H', chunk_id) if positive else pack('b', NOT_ACK) + pack('H', chunk_id)
    crc = binascii.crc32(message)
    message = message + pack('I', crc)
   # print("ACK SEND...", message, crc)

    addr = (addr[0], PORTACK_CLIENT)

    # send positive ACK - send chunk_id as 2bytes (H)
    transmit_socket.sendto(message, addr)


def unpack_packet(data):
    crc_received = unpack("I", bytes(data[-4:]))[0]
    packet_id = unpack("H", bytes(data[-CONTROL:-4]))[0]
    file_data = data[:-CONTROL]
    return packet_id, crc_received, file_data

def receive_packet_StopAndWait(expected_packet_id):

    while True:
        data, addr = receive_socket.recvfrom(BUFLEN)

        # compute crc
        crc_computed = binascii.crc32(data[:-4])
        packet_id, crc_received, file_data = unpack_packet(data)

        logging.debug("Packet received: %d, expected chunk: %d, crc received %d, crc computed: %d ", packet_id,
                      expected_packet_id, crc_received, crc_computed)

        #SUCCESS IN DELIVERY
        if crc_computed == crc_received and random.random() > ERROR_DATA:
            logging.debug("SUCCESS Sending positive ACK for packet %d", packet_id)
            sendACK(addr, True, packet_id)
            if packet_id == expected_packet_id:
                return file_data

        else:
            logging.debug("ERROR Sending negative ACK for packet %d", packet_id)
            sendACK(addr, False, packet_id)

def check_md5(md5_hash):
    logging.info("Waiting for receiving MD5 hash")

    md5_hash_received = binascii.hexlify(receive_packet_StopAndWait(CONTROLPACKET_ID-2))

    md5_hash_computed = binascii.hexlify(md5_hash.digest())

    logging.info("MD5 hash received : %s, computed MD5 hash: %s", md5_hash_received,
                 md5_hash_computed)
    if md5_hash_computed == md5_hash_received:
        logging.info("File has been transfered successfully")
    else:
        logging.info("File is CORRUPTED!!!")

def receive_file_SelectiveRepeat(file_size, file_name, FRAMESIZE):
    packet_buffer = [None] * FRAMESIZE

    num_packets = file_size//CHUNK_SIZE + 1

    with open("new_" + file_name, "wb") as f_rec:
        md5_hash = hashlib.md5()
        least_acknowledged_packet_id = 0
        while least_acknowledged_packet_id < num_packets:

            # receive data
            data, addr = receive_socket.recvfrom(BUFLEN)

            # compute crc
            crc_computed = binascii.crc32(data[:-4])

            packet_id, crc_received, file_data = unpack_packet(data)

            logging.debug("Expected packet: from %d to %d, packet received %d, crc received %d, crc computed: %d ",
                          least_acknowledged_packet_id, least_acknowledged_packet_id + FRAMESIZE, packet_id,
                          crc_received, crc_computed)


            #send ACK
            if crc_computed == crc_received and random.random() > ERROR_DATA: #simulate bad CRC
                logging.debug("SUCCESS Sending positive ACK for packet %d", packet_id)
                sendACK(addr, True, packet_id)
                if least_acknowledged_packet_id <= packet_id < least_acknowledged_packet_id + FRAMESIZE:
                    packet_buffer[packet_id % FRAMESIZE] = file_data
            else:
                logging.debug("ERROR Sending negative ACK for packet %d",  packet_id)
                sendACK(addr, False, packet_id)

            # check that all packets in frame has been delivered
            if not None in packet_buffer:
                logging.debug("All packets in frame from %d has been delivered\n", least_acknowledged_packet_id)
                least_acknowledged_packet_id += FRAMESIZE

                #write to file
                for packet in packet_buffer:
                    f_rec.write(packet)
                    md5_hash.update(packet)

                #update packet buffer
                buffers_num = FRAMESIZE if least_acknowledged_packet_id + FRAMESIZE < num_packets else num_packets - least_acknowledged_packet_id
                packet_buffer = [None] * buffers_num


    check_md5(md5_hash)

def receive_file_StopAndWait(file_size, file_name):

    num_packets = file_size//CHUNK_SIZE + 1

    with open("new_" + file_name, "wb") as f_rec:
        md5_hash = hashlib.md5()
        next_chunk_id = 0
        while next_chunk_id < num_packets:
            file_data = receive_packet_StopAndWait(next_chunk_id)
            print("Writing to file...")
            if file_data:
                f_rec.write(file_data)
                md5_hash.update(file_data)
                next_chunk_id += 1
        # receive md5
        check_md5(md5_hash)

def Main():
    logging.getLogger().setLevel(logging.DEBUG) if DEBUG else logging.getLogger().setLevel(logging.INFO)

    global receive_socket, transmit_socket
    transmit_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    transmit_socket.bind((host, PORTACK_SERVER))

    receive_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    receive_socket.bind((host, PORTDATA_SERVER))

    logging.info("UDP server Started...")

    # receive filename
    file_name = receive_packet_StopAndWait(CONTROLPACKET_ID).decode("ascii").split("/")[-1]

    # receive filesize
    file_size = receive_packet_StopAndWait(CONTROLPACKET_ID-1)
    file_size = unpack("I", file_size)[0]

    logging.info("File %s with size %d will be received", file_name, file_size)

    # receive file
    method = int(sys.argv[1])

    if method == 1:
        FRAMESIZE = int(sys.argv[2])
        receive_file_SelectiveRepeat(file_size, file_name, FRAMESIZE)
    elif method == 0:
        receive_file_StopAndWait(file_size, file_name)

if __name__ == '__main__':
    Main()

#!/usr/bin/env python3
# coding: utf-8

import sys, os, traceback, optparse, struct, random
import time, datetime
import socket, select
import threading

class config:
    def __init__(self, name, mac, UDPport, TCPport):
        self.name = name
        self.mac = mac
        self.UDPport = UDPport
        self.TCPport = TCPport


def create_sockets():
    global socketTCP, socketUDP
    socketUDP = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    debug("Inicialitzat socket UDP")
    socketTCP = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    debug("Inicialitzat socket TCP")

#Llegeix l'arxiu de configuració
def read_config_file():
    file = open(config_file, 'r')

    #toquenitzem l'arxiu i guardem les variables
    name = file.readline().strip('\n').split(' ')[1]
    mac = file.readline().strip('\n').split(' ')[1]
    UDPport = int(file.readline().strip('\n').split(' ')[1])
    TCPport = int(file.readline().strip('\n').split(' ')[1])
    file.close()

    return config(name, mac, UDPport, TCPport)

#Imprimeix un missatge debug
def debug(msg):
    if dbg == True:
        print time.strftime("%H:%M:%S") + ": DEBUG -> " + msg

#Imprimeix un missatge
def print_message(msg):
     print time.strftime("%H:%M:%S") + ": SYSTEM -> " + msg

def read_equips_file():
    equips_data = {}
    with open(equips_file) as equips:
        line = equips.readline().strip('\n').split(' ')
        while len(line) > 1:    #Per evitar problemes amb un \n al final del arxiu
            name, mac = line[0], line[1]
            equips_data[name] = mac
            line = equips.readline().strip('\n').split(' ')
    equips.close()
    return equips_data

def initialize_types():
    return {"REGISTER_REQ": 0x00,
            "REGISTER_ACK": 0x01,
            "REGISTER_NACK": 0x02,
            "REGISTER_REJ": 0x09,
            "ALIVE_INF": 0x10,
            "ALIVE_ACK": 0x11,
            "ALIVE_NACK": 0x12,
            "ALIVE_REJ": 0x13,
            "SEND_FILE": 0x20,
            "SEND_ACK": 0x21,
            "SEND_NACK": 0x22,
            "SEND_REJ": 0x23,
            "SEND_DATA": 0x24,
            "SEND_END": 0x25,
            0x00: "REGISTER_REQ",
            0x01: "REGISTER_ACK",
            0x02: "REGISTER_NACK",
            0x09: "REGISTER_REJ",
            0x10: "ALIVE_INF",
            0x11: "ALIVE_ACK",
            0x12: "ALIVE_NACK",
            0x13: "ALIVE_REJ",
            0x20: "SEND_FILE",
            0x21: "SEND_ACK",
            0x22: "SEND_NACK",
            0x23: "SEND_REJ",
            0x24: "SEND_DATA",
            0x25: "SEND_END",
    }

def main():
    global equips_data
    config = read_config_file()   # clase amb la configuració del servidor
    debug("Llegit arxiu de configuració")
    equips_data = read_equips_file() #Dictionary amb informació dels equips
    create_sockets()

if __name__ == '__main__':
    global dbg, config_file, equips_file, types
    parser = optparse.OptionParser()
    parser.add_option('-c', '--file', action = 'store', default = 'server.cfg', help = 'Specify a different client file name')
    parser.add_option('-d', '--debug', action = 'store_true', default = False, help = 'Shows information each time something happens')
    parser.add_option('-u', '--equips', action = 'store', default = 'equips.dat', help = 'Specifies to see equips.dat')
    (options, args) = parser.parse_args()
    config_file = options.file
    dbg = options.debug
    equips_file = options.equips
    types = initialize_types()
    debug('Llegits paràmetres línea comandes')
    main()

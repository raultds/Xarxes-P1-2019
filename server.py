#!/usr/bin/env python3
# coding: utf-8

import sys, os, traceback, optparse, struct, random
import time, datetime
import socket, select
import threading

#Imprimeix un missatge debug
def debug(msg):
    print(time.strftime("%H:%M:%S"), ": DEBUG -> ", msg)

#Imprimeix un missatge
def print_message(msg):
     print(time.strftime("%H:%M:%S"), ": SYSTEM -> ", msg)

#Llegeix l'arxiu de configuració
def read_config_file():





if __name__ == '__main__':
    parser = optparse.OptionParser()
    parser.add_option('-c', '--file', action = 'store', default = 'client.cfg', help = 'Specify a different client file name')
    parser.add_option('-d', '--debug', action = 'store_true', default = False, help = 'Shows information each time something happens')
    (options, args) = parser.parse_args()
    debug = options.debug
    if debug == True:
        debug('Llegits paràmetres línea comandes')

    main()

def main():

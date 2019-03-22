#!/usr/bin/env python2.7
# coding: utf-8

import sys, os, traceback, optparse, struct, random
import time, datetime
import socket, select
import threading

class equip:
    def __init__(self, name, mac, status="DISCONNECTED", random = "000000"):
        self.name = name
        self.mac = mac
        self.status = status
        self.npackets = 0
        self.random = random
        self.udp_packets = []
        self.tcp_packets = []
        self.ip = ''
        self.tcp_thread = 0
        self.socket = None
        self.port = 0

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


#Crea els threads per escoltar
def initialize_threads():
    global thread_udp, thread_tcp
    thread_udp = threading.Thread(target=listen_udp)
    debug("Inicialitzat thread udp")
    thread_tcp = threading.Thread(target=listen_tcp)
    debug("Inicialitzat thread tcp")

    thread_udp.daemon = True
    thread_tcp.daemon = True
    thread_udp.start()
    thread_tcp.start()


#Comprova que els paquets Alive i siguin correctes.
# True = correcte
#-2 = equip no autoritzat o no registrat
#-3 = discrepancies amb la ip o num aleatori
def correct_packet(data, equip, addr):
    if equip == -1 or equip.status == 'DISCONNECTED':
        return -2
    if equip.ip != addr[0] or str(equip.random) != str(data['random']):
        return -3
    return True



#Envia paquets
def send_packet(addr, equip, type):
    if type == '0x01': #REGISTER_ACK
        if equip.status == 'REGISTERED' or equip.status == 'ALIVE':
            trama = struct.pack(udp_format, 0x01, configuration.name, configuration.mac, equip.random, str(configuration.TCPport))
            equip.npackets+=1
            send = socketUDP.sendto(trama, addr)
            debug_string = 'Enviat: ' + str(send) + ' Bytes ' + 'type: ' + "0x01" + ' name: ' + equip.name + ' mac: ' + configuration.mac + ' aleatori: ' + equip.random + ' dades: ' + str(configuration.TCPport)
            debug(debug_string)
        else:
            equip.npackets+=1
            random_num = str(random.randint(100000, 900000))
            equip.random = random_num
            trama = struct.pack(udp_format, 0x01, configuration.name, configuration.mac, equip.random, str(configuration.TCPport))
            send = socketUDP.sendto(trama, addr)
            debug_string = 'Enviat: ' + str(send) + ' Bytes ' + 'type: ' + "0x01" + ' name: ' + equip.name + ' mac: ' + configuration.mac + ' aleatori: ' + equip.random + ' dades: ' + str(configuration.TCPport)
            debug(debug_string)
            equip.status = "REGISTERED"
            debug_string = 'Equip: ' + equip.name + ' passat a estat REGISTERED'
            create_alive_thread(addr, equip)
            debug(debug_string)
    elif type == '0x02': #REGISTER_NACK
        trama = struct.pack(udp_format, 0x02, "000000", "000000000000", "000000", "Enviat primer paquet amb numero aleatori diferent de 0")
        send = socketUDP.sendto(trama, addr)
        equip.npackets+=1
        debug_string = 'Enviat: ' + str(send) + ' Bytes type: 0x02 name: 000000  mac: 000000000000  aleatori: 000000  dades: Enviat primer paquet amb numero aleatori diferent de 0'
        debug(debug_string)
    elif type == '0x03': #REG_REJ
        if equip != -1:
            trama = struct.pack(udp_format, 0x03, configuration.name, configuration.mac, equip.random, "MAC incorrecta")
            equip.npackets+=1
            send = socketUDP.sendto(trama, addr)
            debug_string = 'Enviat: ' + str(send) + ' Bytes ' + 'type: ' + "0x01" + ' name: ' + equip.name + ' mac: ' + configuration.mac + ' aleatori: ' + equip.random + ' dades: ' + str(configuration.TCPport)
            debug(debug_string)
        else:
            trama = struct.pack(udp_format, 0x03, configuration.name, configuration.mac, "000000", "Equip no permés en el servidor")
            send = socketUDP.sendto(trama, addr)
            debug_string = 'Enviat: ' + str(send) + ' Bytes ' + ' type: ' + "0x03" + ' name: ' + equip.name + ' mac: ' + configuration.mac + ' aleatori: ' + equip.random + ' dades: ' + str(configuration.TCPport)
            debug(debug_string)
    elif type == '0x11': #ALIVE_ACK
        trama = struct.pack(udp_format, 0x11, configuration.name, configuration.mac, equip.random, "")
        send = socketUDP.sendto(trama, addr)
        debug_string = 'Enviat: ' + str(send) + ' Bytes ' + ' type: ' + "0x11" + ' name: ' + equip.name + ' mac: ' + configuration.mac + ' aleatori: ' + equip.random + ' dades: '
        debug(debug_string)
    elif type == '0x12': #ALIVE_NACK
        trama = struct.pack(udp_format, 0x12, "000000", "000000000000", "000000", "Equip amb IP incorrecta o número aleatori incorrecte")
        send = socketUDP.sendto(trama, addr)
        debug_string = 'Enviat: ' + str(send) + ' Bytes type: 0x12 name: 000000 mac: 000000000000 aleatori: 000000 dades: Equip amb IP incorrecta o número aleatori incorrecte'
        debug(debug_string)
    elif type == '0x13': #ALIVE_REJ
        trama = struct.pack(udp_format, 0x13, "000000", "000000000000", "000000", "Equip no autoritzat o no registrat")
        send = socketUDP.sendto(trama, addr)
        debug_string = 'Enviat: ' + str(send) + ' Bytes type: 0x13 name: 000000 mac: 000000000000 aleatori: 000000 dades: Equip no autoritzat o no registrat'
        debug(debug_string)
    elif type == '0x21': #SEND_ACK
        trama = struct.pack(tcp_format, 0x21, configuration.name, configuration.mac, equip.random, equip.name + '.cfg')
        send = equip.socket.send(trama)
        debug_string = 'Enviat: ' + str(send) + ' Bytes ' + ' type: ' + "0x21" + ' name: ' + equip.name + ' mac: ' + configuration.mac + ' aleatori: ' + equip.random + ' dades: ' + equip.name + '.cfg'
        debug(debug_string)
    elif type == '0x22':#SEND_NACK
        trama = struct.pack(tcp_format, 0x22, "000000", "000000000000", "000000", "Equip amb IP incorrecta o número aleatori incorrecte")
        send = equip.socket.send(trama)
        debug_string = 'Enviat: ' + str(send) + ' Bytes type: 0x22 name: 000000 mac: 000000000000 aleatori: 000000 dades: Equip amb IP incorrecta o número aleatori incorrecte'
        debug(debug_string)
    elif type == '0x23': #SEND_REJ
        trama = struct.pack(tcp_format, 0x23, "000000", "000000000000", "000000", "Equip no autoritzat o no registrat")
        send = equip.socket.send(trama)
        debug_string = 'Enviat: ' + str(send) + ' Bytes type: 0x23 name: 000000 mac: 000000000000 aleatori: 000000 dades: Equip no autoritzat o no registrat'
        debug(debug_string)

#Tracta els paquets reg_req
def treat_reg_req(data, addr):
    equip = get_equip(data['name'])
    if equip != -1:
        if equip.npackets == 0: #Si es el primer paquet que envia ens guardem la ip
            equip.ip = addr[0]
        if equip.npackets == 0 and data['random']!= "000000": # Si envia numero aleatori !=0 quan es el seu primer paquet
            send_packet(addr, equip, '0x02')
        elif data['MAC'] == equip.mac:   # Si l'equip està permés se li envia un REG_ACK
            send_packet(addr, equip, '0x01')
        elif data['MAC'] != equip.mac:  # si la mac es incorrecta s'envia un REG_REJ
            send_packet(addr, equip, '0x03')
    else:
        send_packet(addr, None, '0x03') #Si no esta autoritzat s'envia REG_REJ

#Manté la comunicació amb l'equip
def keep_alive_ack(addr, equip):
    w = 3
    j=2
    interval=3
    contador = 0
    start = time.time()
    while True:
        if stop_threads == True:
            break
        if contador == 3:
            debug("S'han perdut 3 paquets ALIVE o no s'han rebut correctament")
            debug("Finalitzant client")
            break
        if len(equip.udp_packets) != 0:
            if equip.status == 'REGISTERED':
                equip.status = 'ALIVE'
                debug('Equip: ' + equip.name + ' passat a estat ALIVE')
            send_packet(addr, equip, '0x11')
            contador = 0
            start = time.time()
            equip.udp_packets.remove(equip.udp_packets[0])
        else: #No hi ha paquets en cua, comprovem que no s'han perdut
            actual = time.time() - start
            if actual > w and equip.status == "ALIVE": #temps en que hauria de rebre un packet
                contador+=1
                start = time.time()
        if equip.status == "REGISTERED":
            actual = time.time() - start
            if actual > j * interval:
                debug("No s'ha rebut alive correcte abans de 2 intervals d'enviament")
                equip.status = "DISCONNECTED"
                equip.status = "DISCONNECTED"
                debug("Equip passa a estat DISCONNECTED")
                break

#Tracta el primer alive_inf de cada equip
def create_alive_thread(addr, equip):
    thread_alive = threading.Thread(target=keep_alive_ack, kwargs={'addr': addr, 'equip': equip})
    thread_alive.daemon = True
    thread_alive.start()


def receive_config_file(equip, addr):
    global received_file
    w = 4

    packet = equip.tcp_packets[0]
    correct = correct_packet(packet, equip, addr)
    ack = False
    if correct == True:
        send_packet(addr, equip, '0x21') #SEND_ACK
        equip.tcp_packets.remove(equip.tcp_packets[0])
        received_file = open (equip.name+ '.cfg', 'w')
        ack = True
        equip.tcp_thread = 1
        start = time.time()
    elif correct == -1:
        send_packet(addr, equip, '0x23') #SEND_NACK
        equip.tcp_packets.remove(equip.tcp_packets[0])
        equip.tcp_thread = 0
    elif correct == -2:
        send_packet(addr, equip, '0x22') # SEND_NACK
        equip.tcp_packets.remove(equip.tcp_packets[0])
        equip.tcp_thread = 0

    equip.socket.setblocking(0)
    if ack == True:
        while equip.tcp_thread == 1:
            actual = time.time() - start
            if actual > w:
                debug("No hi ha una comunicació correcta. Tancat canal TCP")
                equip.socket.close()
                equip.tcp_thread = 0
                break
            try:
                data = equip.socket.recv(struct.calcsize(tcp_format))  #Esperem paquets
                data_string = struct.unpack(tcp_format, data)
                packet = get_packet_info(data_string)
                size = sys.getsizeof(data)
                debug_string = 'Rebut: ' + str(size) + ' Bytes ' + ' type: ' + str(packet['type']) + ' name: ' + str(packet['name']) + ' mac: '+ str(packet['MAC']) + ' aleatori: ' + str(packet['random']) + ' dades: ' + str(packet['data'])
                debug(debug_string)
                treat_tcp_packet(packet, None, addr)
            except socket.error as msg:
                pass

#TRacta els paquets tcp
def treat_tcp_packet(data, con, addr):
    type = data['type']
    equip = get_equip(data['name'])
    if type == '0x20':                  # SEND_FILE
        equip.tcp_packets.append(data)
        if equip.tcp_thread == 0:
            equip.socket = con
            thread_send = threading.Thread(target=receive_config_file, kwargs={'equip': equip, 'addr': addr})
            thread_send.daemon = True
            thread_send.start()
    elif type == '0x24':
        if correct_packet(data, equip, addr):
            received_file.write(data['data'])
    elif type == '0x25':
        if correct_packet(data, equip, addr):
            equip.tcp_thread = 0
            received_file.close()


#Tracta els paquet UDP
def treat_udp_packet(data, addr):
    type = data['type']
    if type == '0x0':                     # REGISTER_REQ
        treat_reg_req(data, addr)
    elif type == '0x10':                  # ALIVE_INF
        equip = get_equip(data['name'])
        alive = correct_packet(data, equip, addr)
        if alive == True:
            equip.udp_packets.append(data)
        elif alive == -2:                         #S'envia alive_rej
            send_packet(addr, equip, '0x13')
        elif alive == -3:                         #S'envia alive_nack
            send_packet(addr, equip, '0x12')
    elif type  == '0x09':                 # Error
        print_msg("Error rebuda paquet")


#Retorna un diccionari amb els camps del paquet
def get_packet_info(data_string):
    trama = []
    packet = {'type': 0x00, 'name': "", 'MAC': "", 'random': "", 'data': ""}

    for element in data_string:          # Separem les dades del paquet
        trama.append(str(element).split('\x00')[0]) #Separem els caracters hexadecimals

    packet['type'] = str(hex(int(trama[0])))
    packet['name'] = trama[1]
    packet['MAC'] = trama[2]
    packet['random'] = trama[3]
    if len(trama) == 5: #Inclou dades, per evitar outofbounds en els casos regsiter_req que no envien dades
        packet['data'] = trama[4]

    return packet

#Escolta conexions udp
def listen_udp():
    try:
        socketUDP.bind(('localhost', configuration.UDPport))
    except socket.error as msg:
        print('Bind failed. Error code: ' + str(msg[0]) + ' Message ' + msg[1])
    debug("Fet bind UDP al socket")
    debug("Escoltant paquets UDP")
    while True:
        if stop_threads == True:
            socketUDP.close()
            break
        data, addr = socketUDP.recvfrom(struct.calcsize(udp_format))  #Esperem paquets
        data_string = struct.unpack(udp_format, data)

        packet = get_packet_info(data_string)

        size = sys.getsizeof(data)
        debug_string = 'Rebut: ' + str(size) + ' Bytes ' + ' type: ' + str(packet['type']) + ' name: ' + str(packet['name']) + ' mac: '+ str(packet['MAC']) + ' aleatori: ' + str(packet['random']) + ' dades: ' + str(packet['data'])
        debug(debug_string)
        treat_udp_packet(packet, addr)

#Escolta conexions tcp
def listen_tcp():
    try:
        socketTCP.bind(('localhost', configuration.TCPport))
    except socket.error as msg:
        print('Bind failed. Error code: ' + str(msg[0]) + ' Message ' + msg[1])
    debug("Fet bind TCP al socket")
    debug("Escoltant paquets TCP")
    while True:
        if stop_threads == True:
            socketTCP.close()
            break
        socketTCP.listen(1)
        con, addr = socketTCP.accept()
        debug_string = "Rebut connexio TCP de:" + str(addr[0])
        debug(debug_string)
        debug("Acceptada connexió")
        data = con.recv(struct.calcsize(tcp_format))  #Esperem paquets
        data_string = struct.unpack(tcp_format, data)

        packet = get_packet_info(data_string)
        size = sys.getsizeof(data)
        print packet
        debug_string = 'Rebut: ' + str(size) + ' Bytes ' + ' type: ' + str(packet['type']) + ' name: ' + str(packet['name']) + ' mac: '+ str(packet['MAC']) + ' aleatori: ' + str(packet['random']) + ' dades: ' + str(packet['data'])
        debug(debug_string)
        treat_tcp_packet(packet, con, addr)

#Retorna un equip dels disponibles
def get_equip(name):
    for equip in equips_data:
        if name == equip.name:
            return equip
    return -1


#Imprimeix un missatge debug
def debug(msg):
    if dbg == True:
        print(time.strftime("%H:%M:%S") + ": DEBUG -> " + str(msg))

#Imprimeix un missatge
def print_msg(msg):
     print(time.strftime("%H:%M:%S") + ": SYSTEM -> " + str(msg))

def read_equips_file(equips_file):
    equips_data = list()
    with open(equips_file) as equips:
        line = equips.readline().strip('\n').split(' ')
        while len(line) > 1:    #Per evitar problemes amb un \n al final del arxiu
            name, mac = line[0], line[1]
            equips_data.append(equip(name, mac)) # A equips_data hi ha una llista amb els equips disponibles
            line = equips.readline().strip('\n').split(' ')
    equips.close()
    return equips_data

#Llegeix l'arxiu de configuració
def read_config_file(config_file):
    file = open(config_file, 'r')

    #toquenitzem l'arxiu i guardem les variables
    name = file.readline().strip('\n').split(' ')[1]
    mac = file.readline().strip('\n').split(' ')[1]
    UDPport = int(file.readline().strip('\n').split(' ')[1])
    TCPport = int(file.readline().strip('\n').split(' ')[1])
    file.close()

    return config(name, mac, UDPport, TCPport)

#S'encarrega de tractar la comanda
def treat_command(input):
    if input == 'quit':
        stop_threads = True
        sys.exit(1)
    elif input == 'list':
        print('===================LLISTA EQUIPS==================')
        for equip in equips_data:
            if equip.status != 'DISCONNECTED':
                print('Name: ' + equip.name + ' MAC: ' + equip.mac + ' IP: ' + equip.ip + ' State: ' + equip.status)
            else:
                print('Name: ' + equip.name + ' MAC: ' + equip.mac + ' State: ' + equip.status)
    else:
        print_msg("Comanda incorrecta")

#S'encarrega de llegir comandes per consola
def read_commands():
    while True:
        if dbg == False: #per evitar que es barreji amb els missatges de debug
            command = raw_input('->')
        else:
            command = raw_input('')
        treat_command(command)

if __name__ == '__main__':
    try:
        global dbg, types, equips_data, configuration, udp_format, stop_threads, tcp_format
        stop_threads = False
        udp_format = "B7s13s7s50s"
        tcp_format = "B7s13s7s150s"
        parser = optparse.OptionParser()
        parser.add_option('-c', '--file', action = 'store', default = 'server.cfg', help = 'Specify a different client file name')
        parser.add_option('-d', '--debug', action = 'store_true', default = False, help = 'Shows information each time something happens')
        parser.add_option('-u', '--equips', action = 'store', default = 'equips.dat', help = 'Specifies to see equips.dat')
        (options, args) = parser.parse_args()
        config_file = options.file
        dbg = options.debug
        equips_file = options.equips
        debug('Llegits paràmetres línea comandes')

        configuration = read_config_file(config_file)   # clase amb la configuració del servidor
        debug("Llegit arxiu de configuració")

        equips_data = read_equips_file(equips_file) #Dictionary amb informació dels equips
        create_sockets()
        initialize_threads()
        read_commands()

    except KeyboardInterrupt:
        stop_threads = True
        sys.exit(1)

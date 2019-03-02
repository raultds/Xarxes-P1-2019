#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>

#define REGISTER_REQ 0x00
#define REGISTER_ACK 0x01
#define REGISTER_NACK 0x02
#define REGISTER_REJ 0x03
#define ERROR 0x09

#define ALIVE_INF 0x010
#define ALIVE_ACK 0x11
#define ALIVE_NACK 0x12
#define ALIVE_REJ 0x13

#define GET_FILE 0x30
#define GET_ACK 0x31
#define GET_NACK 0x32
#define GET_REJ 0x33
#define GET_DATA 0x34
#define GET_END 0x35

/*Estructura per a guardar les dades del arxiu del client */
struct client_config{
  char name[20];
  char MAC[13];
  char server[20];
  int UDPport;
};

/*Estructura que fa de paquet UDP */
struct udp_PDU{
  unsigned char type;
  char name[8];
  char mac[14];
  char random[8];
  char data [51];
};
/* Estructura que conté la info que es rep del subscribe per a la conexió TCP */
struct tcp_data{
  char random[8];
  int tcp_port;
};

/* Variables globals */
int debug_flag = 0;
char software_config_file[20] = "client.cfg";
char network_config_file[20] = "boot.cfg";
char state[20] = "DISCONNECTED";
int sock;
struct tcp_data pc_data;

/* Funcions */
void parse_parameters(int argc, char **argv);
void read_software_config_file(struct client_config *config);
void debug(char msg[]);
struct udp_PDU create_packet(char type[], char mac[], char random_num[], char data[]);
void subscribe(struct client_config *config, struct sockaddr_in addr_server, struct sockaddr_in addr_cli);
void createUDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition);
void set_state(char _state[]);
void print_msg(char msg[]);
void periodic_comunication(struct client_config *config, struct sockaddr_in addr_server, struct sockaddr_in addr_cli);

int main(int argc, char **argv){
  struct sockaddr_in addr_server, addr_cli;
  struct client_config config;

  parse_parameters(argc, argv);
  read_software_config_file(&config);
  debug("S'ha llegit l'arxiu de configuració");

  printf("La configuració llegida és la següent: \n \t Name: %s \n \t MAC: %s \n \t Server: %s \n \t Port: %i \n" ,
    config.name, config.MAC, config.server, config.UDPport);

  /* Adreça del bind del client */
  memset(&addr_cli, 0, sizeof (struct sockaddr_in));
  addr_cli.sin_family = AF_INET;
  addr_cli.sin_addr.s_addr = htonl(INADDR_ANY);
  addr_cli.sin_port = htons(config.UDPport);

  /* Adreça del servidor */
  memset(&addr_server, 0, sizeof(addr_server));
  addr_server.sin_family = AF_INET;
  addr_server.sin_addr.s_addr = inet_addr(config.server);
  addr_server.sin_port = htons(config.UDPport);

  subscribe(&config, addr_cli, addr_server);
  periodic_comunication(&config, addr_cli, addr_server);
  return 1;
}

void read_software_config_file(struct client_config *config){
  FILE *conf;
  char word[1024];

  conf = fopen(software_config_file, "r");
  if(conf == NULL){
    fprintf(stderr, "Error obrir arxiu");
    exit(-1);
  }

  fscanf(conf, "%s", word);
  fscanf(conf, "%s", word);                    /* No es la millor manera de fer-ho... pero ja que suposem que el fitxer es correcte*/
  strcpy(config->name, word);                  /*  Ens saltem les comprovacions */

  fscanf(conf, "%s", word);
  fscanf(conf, "%s", word);
  strcpy(config->MAC, word);

  fscanf(conf, "%s", word);
  fscanf(conf, "%s", word);
  if(strcmp(word, "localhost") == 0){
    strcpy(config->server, "127.0.0.1");
  }else{
    strcpy(config->server, word);
  }

  fscanf(conf, "%s", word);
  fscanf(conf, "%s", word);
  config->UDPport = atoi(word);
  fclose(conf);
}

void subscribe(struct client_config *config, struct sockaddr_in addr_server, struct sockaddr_in addr_cli){
  int tries, max = 4, i, temp2;
  int correct = 0; /*variable per saber si s'ha aconseguit correctament el registre */
  struct udp_PDU data;
  struct udp_PDU reg_pdu;
  socklen_t fromlen;
  char buff[100];

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock < 0){
    fprintf(stderr, "No puc obrir socket \n");
    exit(-1);
  }

  debug("S'ha obert el socket");

/*  if (bind(sock, (struct sockaddr *)&addr_server, sizeof(addr_server)) < 0){
    perror("bind failed \n");
    exit(-1);
  }
  debug("S'ha fet el bind al port");*/

  /* Creació paquet registre */
  createUDP(&reg_pdu, config, REGISTER_REQ);

  /* Inici proces subscripció */
  for(tries = 0; tries < 3; tries++){
    int packet_counter = 0, interval = 2, t = 2, temp = 0;
    for(i = 0; i < 3; i++){
      temp = sendto(sock, (struct udp_PDU*)&reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
      if(temp == -1){
        printf("Error sendTo \n");
        exit(-1);
      }
      packet_counter++;
      debug("Enviat paquet REGISTER_REQ");
      if(strcmp(state, "DISCONNECTED") == 0){
        set_state("WAIT_REG");
        debug("Passat a l'estat WAIT_REG");
      }
    }

    while(1){
      sleep(t);
      fromlen = sizeof(addr_server);
      temp2 = recvfrom(sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&addr_server, &fromlen);
      if(temp2 < 0){ /* Timeout excedit */
          temp = sendto(sock, (struct udp_PDU*)&reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
          if(temp == -1){
            printf("Error sendTo \n");
            exit(-1);
          }
          packet_counter++;
          debug("Enviat paquet REGISTER_REQ");
          if(packet_counter == 8) break;
          if((interval * max) > t ) t+=interval;

      }else{ /* s'han rebut dades */
        correct = 1;
        break;
      }
    }
    if(correct == 1) break;
    if(packet_counter == 8){
      sleep(5);
      debug("Reiniciant procès subscripció");
    }
  } /* Fi while de enviar paquets */

  if(tries==3 && correct == 0){
    print_msg("Ha fallat el procès de registre. No s'ha pogut contactar amb el servidor.");
    exit(-1);
  }

  sprintf(buff, "Rebut: bytes= %lu, type:%i, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.mac, data.random, data.data);
  debug(buff);
  /*CONTINUA EL PROGRAMA*/
  switch(data.type){
    case REGISTER_REJ:
      sprintf(buff, "El client ha estat rebutjat. Motiu: %s", data.data);
      print_msg(buff);
      set_state("DISCONNECTED");
      debug("Client passa a l'estat : DISCONNECTED.");
      exit(-1);
      break;
    case REGISTER_NACK:
      debug("Rebut REGISTER_NACK, reiniciant procès subscripció");
      subscribe(config, addr_server, addr_cli);
      break;
    case REGISTER_ACK:
      debug("Rebut REGISTER_ACK, client passa a l'estat REGISTERED");
      set_state("REGISTERED");
      pc_data.tcp_port = atoi(data.data);
      strcpy(pc_data.random, data.random);
    }
}

void periodic_comunication(struct client_config *config, struct sockaddr_in addr_server, struct sockaddr_in addr_cli){
  struct udp_PDU data;
  struct udp_PDU periodic_pdu;
  socklen_t fromlen;
  char buff[100];
  createUDP(&periodic_pdu, config, ALIVE_INF);



}


void set_state(char _state[]){
  strcpy(state, _state);
}


void createUDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition){
  switch(petition){
    case REGISTER_REQ:
      pdu->type = petition;
      strcpy(pdu->name, config->name);
      strcpy(pdu->mac, config->MAC);
      strcpy(pdu->random, "0000000");
      pdu->data[0] = '\0';
      break;
    case ALIVE_INF:
      pdu->type = petition;
      strcpy(pdu->name, config->name);
      strcpy(pdu->mac, config->MAC);
      strcpy(pdu->random, "0000000");
      pdu->data[0] = '\0';
      break;
  }

}

void debug(char msg[]){
  if(debug_flag==1){
    time_t _time = time(0);
    struct tm *tlocal = localtime(&_time);
    char output[128];
    strftime(output, 128, "%H:%M:%S", tlocal);
    printf("%s: DEBUG -> %s \n", output, msg);
  }
}

void print_msg(char msg[]){
  time_t _time = time(0);
  struct tm *tlocal = localtime(&_time);
  char output[128];
  strftime(output, 128, "%H:%M:%S", tlocal);
  printf("%s: MSG -> %s \n", output, msg);
}

void parse_parameters(int argc, char **argv){
  int i;
  if(argc>1){
    for(i = 0; i < argc; i++){               /* PARSING PARAMETERS */
      char const* option = argv[i];
      if(option[0] == '-'){
        switch(option[1]){
          case 'd':
            debug_flag =1;
            break;
          case 'c':
            strcpy(software_config_file, argv[i+1]);
            break;
          case 'f':
            strcpy(network_config_file, argv[i+1]);
            break;
          default:
            fprintf(stderr, "Wrong parameters Input \n");
            exit(-1);
          }
        }
      }
    }
    debug("S'ha seleccionat l'opció debug");
}

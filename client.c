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
#include <pthread.h>

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
  char name[7];
  char MAC[13];
  char server[20];
  char random[7];
  int UDPport;
  int TCPport;
};

/* Estructura per a pasar les structs de config a treatPacket() */
struct parameters{
  struct client_config *config;
  struct sockaddr_in addr_cli;
  struct sockaddr_in addr_server;
  struct udp_PDU *data;
};

/*Estructura que fa de paquet UDP */
struct udp_PDU{
  unsigned char type;
  char name[7];
  char mac[13];
  char random[7];
  char data [50];
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
char state[30] = "DISCONNECTED";
int sock, counter;
struct tcp_data pc_data;
struct parameters params;
pthread_t alive_thread;

/* Funcions */
void parse_parameters(int argc, char **argv);
void read_software_config_file(struct client_config *config);
void debug(char msg[]);
struct udp_PDU create_packet(char type[], char mac[], char random_num[], char data[]);
void subscribe(struct client_config *config, struct sockaddr_in addr_server, struct sockaddr_in addr_cli);
void createUDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition);
void set_state(char _state[]);
void print_msg(char msg[]);
void set_periodic_comunication();
void treatPacket(struct parameters params);
void open_socket();
void send_alive();

int main(int argc, char **argv){
  struct sockaddr_in addr_server, addr_cli;
  struct client_config config;

  parse_parameters(argc, argv);
  read_software_config_file(&config);
  strcpy(config.random, "000000");
  config.random[6]='\0';

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

  /* Per a poder tractar els paquets més facilment més endavant */
  params.config = &config;
  params.addr_cli = addr_cli;
  params.addr_server = addr_server;

  open_socket();
  subscribe(&config, addr_cli, addr_server);
  pthread_join(alive_thread, NULL);
  return 1;
}

void read_software_config_file(struct client_config *config){
  FILE *conf;
  char word[256];

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
  int tries, max = 4, i, n_bytes;
  int correct = 0; /*variable per saber si s'ha aconseguit correctament el registre */
  struct udp_PDU data;
  struct udp_PDU reg_pdu;
  socklen_t fromlen;
  char buff[100];


  /* Creació paquet registre */
  createUDP(&reg_pdu, config, REGISTER_REQ);

  /* Inici proces subscripció */
  for(tries = 0; tries < 3 && strcmp("REGISTERED", state) !=0; tries++){
    int packet_counter = 0, interval = 2, t = 2, temp = 0;
    for(i = 0; i < 3; i++){
      temp = sendto(sock, &reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
      if(temp == -1){
        printf("Error sendTo \n");
      }
      packet_counter++;
      debug("Enviat paquet REGISTER_REQ");
      if(strcmp(state, "DISCONNECTED") == 0){
        set_state("WAIT_REG");
        debug("Passat a l'estat WAIT_REG");
      }
      n_bytes = recvfrom(sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&addr_server, &fromlen);
      if(n_bytes > 0) break;
      sleep(t);
    }
    while(1 && strcmp("REGISTERED", state) !=0){
      if(n_bytes > 0) {
        correct = 1;
        break;
      }
      fromlen = sizeof(addr_server);
      n_bytes = recvfrom(sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&addr_server, &fromlen);
      if(n_bytes < 0){ /* No s'ha rebut dades */
          temp = sendto(sock, &reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
          if(temp == -1){
            printf("Error sendTo \n");
            exit(-1);
          }
          packet_counter++;
          debug("Enviat paquet REGISTER_REQ");
          if(packet_counter == 8) break;
          if((interval * max) > t ) t+=interval;
          sleep(t);
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
  } /* Fi while d'enviar paquets */

  if(tries==3 && correct == 0){ /* Comprova si s'ha sortit del bucle per màxim d'intents */
    print_msg("Ha fallat el procès de registre. No s'ha pogut contactar amb el servidor.");
    exit(-1);
  }

  sprintf(buff, "Rebut: bytes= %lu, type:%i, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.mac, data.random, data.data);
  debug(buff);
  params.data = &data;
  treatPacket(params);
}

void send_alive(){
  int u = 3, i = 0, temp, n_bytes;
  char buff[100];
  socklen_t fromlen;
  struct udp_PDU alive_pdu;
  struct udp_PDU data;
  createUDP(&alive_pdu, params.config, ALIVE_INF);

  while(1){
    while(strcmp(state, "ALIVE")==0){
      temp = sendto(sock, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&params.addr_server, sizeof(params.addr_server));
      debug("Enviat paquet ALIVE_INF");
      if(temp == -1){
        printf("Error sendTo \n");
      }
      n_bytes = recvfrom(sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&params.addr_server, &fromlen);
      if(n_bytes > 0){
        i=0;
        params.data = &data;
        sprintf(buff, "Rebut: bytes= %lu, type:%i, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.mac, data.random, data.data);
        debug(buff);
        treatPacket(params);
      }else{
        i++;
        if(i==u){  /*Comprova que no s'ha rebut 3 paquets seguits de confirmació d'ALive*/
          set_state("DISCONNECTED");
          debug("No s'ha rebut confirmació de tres paquets de rebuda de paquets ALIVE consecutius.");
          debug("Client passa a l'estat DISCONNECTED i reinicia el proces de subscripció");
          subscribe(params.config, params.addr_server, params.addr_cli);
        }
      }
    }
  }
}

void set_periodic_comunication(){
  int r = 3, temp, n_bytes;
  char buff[100];
  socklen_t fromlen;
  struct udp_PDU alive_pdu;
  struct udp_PDU data;
  createUDP(&alive_pdu, params.config, ALIVE_INF);

  while(1){
    temp = sendto(sock, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&params.addr_server, sizeof(params.addr_server));
    debug("Enviat paquet ALIVE_INF");
    if(temp == -1){
      printf("Error sendTo \n");
    }
    n_bytes = recvfrom(sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&params.addr_server, &fromlen);
    if(n_bytes > 0){
      params.data = &data;
      sprintf(buff, "Rebut: bytes= %lu, type:%i, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.mac, data.random, data.data);
      debug(buff);
      treatPacket(params);
      if(strcmp(state, "REGISTERED")==0 || strcmp(state, "ALIVE")==0){
        break;
      }
    }
    sleep(r);
  }
  if(strcmp(state, "REGISTERED") == 0){
    set_state("DISCONNECTED");
    debug("NO s'ha rebut resposta");
    debug("Passat a l'estat DISCONNECTED i reinici del procès de subscripció");
    subscribe(params.config, params.addr_server, params.addr_cli);
  }else if(strcmp(state, "ALIVE") == 0){
    pthread_create(&alive_thread, NULL, (void*(*) (void*))send_alive, NULL);send_alive();
  }

}


void set_state(char _state[]){
  strcpy(state, _state);
}

void treatPacket(struct parameters params){
  char buff[100];
  switch(params.data->type){
    case REGISTER_REJ:
      sprintf(buff, "El client ha estat rebutjat. Motiu: %s", params.data->data);
      print_msg(buff);
      set_state("DISCONNECTED");
      debug("Client passa a l'estat : DISCONNECTED.");
      exit(-1);
      break;
    case REGISTER_NACK:
      counter++;
      /*if(counter < 3){*/
        debug("Rebut REGISTER_NACK, reiniciant procès subscripció");
        subscribe(params.config, params.addr_server, params.addr_cli);
        break;
      /*}*/
      debug("Superat màxim d'intents. Tancant client.");
      exit(-1);
    case REGISTER_ACK:
      debug("Rebut REGISTER_ACK, client passa a l'estat REGISTERED");
      set_state("REGISTERED");
      params.config->TCPport = atoi(params.data->data);
      strcpy(params.config->random, params.data->random);
      set_periodic_comunication();
      break;
    case ALIVE_ACK:
      if(strcmp(state, "ALIVE")!=0){
          if(strcmp(params.data->random, params.config->random) == 0){
            set_state("ALIVE");
            debug("Rebut REGISTER_ACK correcte, client passa a l'estat ALIVE");

        }
      }
    case ALIVE_NACK: /*No els tenim en compte, no caldria ficar-los */
      break;
    case ALIVE_REJ:
      set_state("DISCONNECTED");
      debug("Rebut ALIVE_REJ, possible suplantació d'identitat. Client pasa a estat DISCONNECTED");
      debug("Reiniciant proces subscripció");
      subscribe(params.config, params.addr_server, params.addr_cli);
      break;
    }

}

void createUDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition){
  switch(petition){
    case REGISTER_REQ:
      pdu->type = petition;
      strcpy(pdu->name, config->name);
      strcpy(pdu->mac, config->MAC);
      strcpy(pdu->random, config->random);
      memset(pdu->data, '\0', sizeof(char)*49);
      pdu->data[49]='\0';
      break;
    case ALIVE_INF:
      pdu->type = petition;
      strcpy(pdu->name, config->name);
      strcpy(pdu->mac, config->MAC);
      strcpy(pdu->random, config->random);
      memset(pdu->data, '\0', sizeof(char)*49);
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

void open_socket(){
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock < 0){
    fprintf(stderr, "No puc obrir socket \n");
    exit(-1);
  }

  debug("S'ha obert el socket");

}

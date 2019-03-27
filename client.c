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
#include "estructures.h"

#define REGISTER_REQ 0x00
#define REGISTER_ACK 0x01
#define REGISTER_NACK 0x02
#define REGISTER_REJ 0x03
#define ERROR 0x09

#define ALIVE_INF 0x010
#define ALIVE_ACK 0x11
#define ALIVE_NACK 0x12
#define ALIVE_REJ 0x13

#define SEND_FILE 0x20
#define SEND_ACK 0x21
#define SEND_NACK 0x22
#define SEND_REJ 0x23
#define SEND_DATA 0x24
#define SEND_END 0x25

#define GET_FILE 0x30
#define GET_ACK 0x31
#define GET_NACK 0x32
#define GET_REJ 0x33
#define GET_DATA 0x34
#define GET_END 0x35

/* Variables globals */
int debug_flag = 0;
char software_config_file[20] = "client.cfg";
char network_config_file[20] = "boot.cfg";
char state[30] = "DISCONNECTED";
int udp_sock, tcp_sock = 0, counter;
int pthread_created = 0;
struct tcp_data pc_data;
struct parameters params;
struct server_data server_data;
pthread_t alive_thread;

/* Funcions */
void parse_parameters(int argc, char **argv);
void read_software_config_file(struct client_config *config);
void debug(char msg[]);
struct udp_PDU create_packet(char type[], char mac[], char random_num[], char data[]);
void subscribe(struct client_config *config, struct sockaddr_in udp_addr_server, struct sockaddr_in addr_cli);
void create_UDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition);
void set_state(char _state[]);
void print_msg(char msg[]);
void set_periodic_comunication();
int treat_UDP_packet();
void setup_udp();
void send_alive();
void read_commands();
void treat_command(char command[]);
void setup_tcp();
void send_conf();
void create_tcp(struct tcp_PDU *pdu, unsigned char petition, char buf[]);
void treat_tcp_packet(struct tcp_PDU pdu_answer);
void send_file();
void get_network_file_size(char *size);
void get_conf();
void get_file();

int main(int argc, char **argv){
  struct sockaddr_in udp_addr_server, addr_cli;
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
  memset(&udp_addr_server, 0, sizeof(udp_addr_server));
  udp_addr_server.sin_family = AF_INET;
  udp_addr_server.sin_addr.s_addr = inet_addr(config.server);
  udp_addr_server.sin_port = htons(config.UDPport);

  /* Per a poder tractar els paquets més facilment més endavant */
  params.config = &config;
  params.addr_cli = addr_cli;
  params.udp_addr_server = udp_addr_server;

  setup_udp();
  subscribe(&config, addr_cli, udp_addr_server);
  read_commands();
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

void subscribe(struct client_config *config, struct sockaddr_in udp_addr_server, struct sockaddr_in addr_cli){
  int tries, max = 4, i, n_bytes;
  int correct = 0; /*variable per saber si s'ha aconseguit correctament el registre */
  struct udp_PDU data;
  struct udp_PDU reg_pdu;
  socklen_t fromlen;
  char buff[100];
  fromlen = sizeof(udp_addr_server);

  /* Creació paquet registre */
  create_UDP(&reg_pdu, config, REGISTER_REQ);

  /* Inici proces subscripció */
  for(tries = 0; tries < 3 && strcmp("REGISTERED", state) !=0 && strcmp("ALIVE", state); tries++){
    int packet_counter = 0, interval = 2, t = 2, temp = 0;
    for(i = 0; i < 3; i++){
      temp = sendto(udp_sock, &reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&udp_addr_server, sizeof(udp_addr_server));
      if(temp == -1){
        printf("Error sendTo \n");
      }
      packet_counter++;
      debug("Enviat paquet REGISTER_REQ");
      if(strcmp(state, "DISCONNECTED") == 0){
        set_state("WAIT_REG");
        if(debug_flag == 0)  print_msg("ESTAT: WAIT_REG");
        debug("Passat a l'estat WAIT_REG");
      }
      n_bytes = recvfrom(udp_sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&udp_addr_server, &fromlen);
      if(n_bytes > 0) break;
      sleep(t);
    }
    while(1 && strcmp("REGISTERED", state) !=0 && strcmp("ALIVE", state)){
      if(n_bytes > 0) {
        correct = 1;
        break;
      }
      n_bytes = recvfrom(udp_sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&udp_addr_server, &fromlen);
      if(n_bytes < 0){ /* No s'ha rebut dades */
          temp = sendto(udp_sock, &reg_pdu, sizeof(reg_pdu), 0, (struct sockaddr *)&udp_addr_server, sizeof(udp_addr_server));
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
  treat_UDP_packet();
}

void send_alive(){
  int u = 3, i = 0, r = 3, temp, n_bytes, packet_state = 0, incorrecte = 0;
  char buff[100];
  socklen_t fromlen;
  struct udp_PDU alive_pdu;
  struct udp_PDU data;
  fromlen = sizeof(&params.udp_addr_server);

  create_UDP(&alive_pdu, params.config, ALIVE_INF);
  while(1){
    while(strcmp(state, "ALIVE")==0){
      temp = sendto(udp_sock, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&params.udp_addr_server, sizeof(params.udp_addr_server));
      debug("Enviat paquet ALIVE_INF");
      if(temp == -1){
        printf("Error sendTo \n");
      }

      n_bytes = recvfrom(udp_sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&params.udp_addr_server, &fromlen);
      if(n_bytes > 0){
        i=0;
        params.data = &data;
        sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.name, data.mac, data.random, data.data);
        debug(buff);
        packet_state = treat_UDP_packet();
        if(packet_state == -1){ /*Significa que es un paquet incorrecte */
          incorrecte+=1;
          if(incorrecte == u){
            set_state("DISCONNECTED");
            print_msg("ESTAT: DISCONNECTED");
            debug("No s'ha rebut tres paquets de confirmació d'ALIVE consecutius correctes.");
            debug("Client passa a l'estat DISCONNECTED i reinicia el proces de subscripció");
            subscribe(params.config, params.udp_addr_server, params.addr_cli);
          }
        }else{
          incorrecte = 0;
        }
      }else{
        i++;
        if(i==u){  /*Comprova que no s'ha rebut 3 paquets seguits de confirmació d'ALive*/
          set_state("DISCONNECTED");
          print_msg("ESTAT: DISCONNECTED");
          debug("No s'ha rebut confirmació de tres paquets de rebuda de paquets ALIVE consecutius.");
          debug("Client passa a l'estat DISCONNECTED i reinicia el proces de subscripció");
          subscribe(params.config, params.udp_addr_server, params.addr_cli);
        }
      }

      n_bytes = 0;
      sleep(r);
    }
  }
}

void set_periodic_comunication(){
  int r = 3, u=0, temp, n_bytes;
  char buff[100];
  socklen_t fromlen;
  struct udp_PDU alive_pdu;
  struct udp_PDU data;
  create_UDP(&alive_pdu, params.config, ALIVE_INF);
  fromlen = sizeof(&params.udp_addr_server);

  while(1 && u!=3){
    temp = sendto(udp_sock, &alive_pdu, sizeof(alive_pdu), 0, (struct sockaddr *)&params.udp_addr_server, sizeof(params.udp_addr_server));
    debug("Enviat paquet ALIVE_INF");
    if(temp == -1){
      printf("Error sendTo \n");
    }
    sleep(r);
    n_bytes = recvfrom(udp_sock, &data, sizeof(data), MSG_DONTWAIT, (struct sockaddr *)&params.udp_addr_server, &fromlen);
    if(n_bytes > 0){
      params.data = &data;
      u = 0;
      sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(struct udp_PDU), data.type, data.name, data.mac, data.random, data.data);
      debug(buff);
      memset(buff, '\0', sizeof(buff)); /*Per evitar stack smashing */
      treat_UDP_packet();
      if(strcmp(state, "ALIVE")==0 || u==3){
        break;
      }
    }else{
      u++;
    }

  }

  if(strcmp(state, "REGISTERED") == 0){
    set_state("DISCONNECTED");
    if(debug_flag == 0)  print_msg("ESTAT: DISCONNECTED");
    debug("NO s'ha rebut resposta");
    debug("Passat a l'estat DISCONNECTED i reinici del procès de subscripció");
    subscribe(params.config, params.udp_addr_server, params.addr_cli);
  }else if(strcmp(state, "ALIVE") == 0 && pthread_created==0){
    pthread_created = 1;
    debug("Creat procés per mantenir comunicació periodica amb el servidor");
    pthread_create(&alive_thread, NULL, (void*(*) (void*))send_alive, NULL);
  }
}

void set_state(char _state[]){
  strcpy(state, _state);
}

int treat_UDP_packet(){
  char buff[100];
  int equals;
  int correct = 0;
  switch(params.data->type){
    case REGISTER_REJ:
      sprintf(buff, "El client ha estat rebutjat. Motiu: %s", params.data->data);
      print_msg(buff);
      set_state("DISCONNECTED");
      if(debug_flag == 0)  print_msg("ESTAT: DISCONNECTED");
      debug("Client passa a l'estat : DISCONNECTED.");
      exit(-1);
      return 0;
    case REGISTER_NACK:
      if(strcmp("ALIVE", state) == 0){ /* El desestimem perque ja estem registrats */
        break;
      }
      if(counter < 3){
        debug("Rebut REGISTER_NACK, reiniciant procès subscripció");
        subscribe(params.config, params.udp_addr_server, params.addr_cli);
        counter++;
        return 0;
      }
      debug("Superat màxim d'intents. Tancant client.");
      exit(-1);
    case REGISTER_ACK:
      equals = strcmp(state, "REGISTERED");
      if(equals != 0){
        debug("Rebut REGISTER_ACK, client passa a l'estat REGISTERED");
        set_state("REGISTERED");
        if(debug_flag == 0)  print_msg("ESTAT: REGISTERED");
        params.config->TCPport = atoi(params.data->data);
        strcpy(params.config->random, params.data->random);
        strcpy(server_data.random, params.data->random);
        strcpy(server_data.name, params.data->name);
        strcpy(server_data.MAC, params.data->mac);
        set_periodic_comunication();
      }else{
        debug("Rebut REGISTER_ACK");
      }
      return 0;
    case ALIVE_ACK:
      equals = strcmp(state, "ALIVE");
      if(strcmp(params.data->random, server_data.random) == 0 && strcmp(params.data->name, server_data.name) == 0 && strcmp(params.data->mac, server_data.MAC)==0){
        correct = 1;
      }
      if(equals!=0 && correct == 1){    /* Primer ack rebut */
        set_state("ALIVE");
        if(debug_flag == 0)  print_msg("ESTAT: ALIVE");
        debug("Rebut ALIVE_ACK correcte, client passa a l'estat ALIVE");
      }else if(equals == 0 && correct == 1){ /* Ja tenim estat ALIVE*/
        debug("Rebut ALIVE_ACK");
      }else{
        debug("Rebut ALIVE_ACK incorrecte");
        return -1;
      }
      return 0;
    case ALIVE_NACK: /*No els tenim en compte, no caldria ficar-los */
      return 0;
    case ALIVE_REJ:
      equals = strcmp(state, "ALIVE");
      if(equals==0){
        set_state("DISCONNECTED");
        if(debug_flag == 0)  print_msg("ESTAT: DISCONNECTED");
        debug("Rebut ALIVE_REJ, possible suplantació d'identitat. Client pasa a estat DISCONNECTED");
        debug("Reiniciant proces subscripció");
        subscribe(params.config, params.udp_addr_server, params.addr_cli);
      }
      return 0;
    }
    return 0;
}

void create_UDP(struct udp_PDU *pdu, struct client_config *config, unsigned char petition){
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

void create_tcp(struct tcp_PDU *pdu, unsigned char petition, char buf[]){
  char str[20];
  char str_size[5];
  switch(petition){
    case SEND_FILE:
      pdu->type = petition;
      strcpy(pdu->name, params.config->name);
      strcpy(pdu->mac, params.config->MAC);
      strcpy(pdu->random, params.config->random);
      strcat(str, software_config_file);
      strcat(str, ",");
      get_network_file_size(&str_size[0]);
      strcat(str, str_size);
      strcpy(pdu->data, str);
      break;
    case SEND_DATA:
      pdu->type = petition;
      strcpy(pdu->name, params.config->name);
      strcpy(pdu->mac, params.config->MAC);
      strcpy(pdu->random, params.config->random);
      strcpy(pdu->data, buf);
      break;
    case SEND_END:
      pdu->type = petition;
      strcpy(pdu->name, params.config->name);
      strcpy(pdu->mac, params.config->MAC);
      strcpy(pdu->random, params.config->random);
      memset(pdu->data, '\0', sizeof(char)*49);
      break;
    case GET_FILE:
      pdu->type = petition;
      strcpy(pdu->name, params.config->name);
      strcpy(pdu->mac, params.config->MAC);
      strcpy(pdu->random, params.config->random);
      strcpy(pdu->data, network_config_file);
      break;
  }
}



void get_network_file_size(char *str_size){
  FILE *f;
  int size;

  f = fopen(network_config_file, "r");
  if(f == NULL){
    fprintf(stderr, "Error obrir arxiu");
    exit(-1);
  }
  fseek(f, 0L, SEEK_END);
  size = ftell(f);
  sprintf(str_size, "%d", size);
  fclose(f);
}

void send_conf(){
  struct tcp_PDU pdu, pdu_answer;
  int n_bytes = 0, w = 4, correct;
  time_t start;
  char buff[100];

  create_tcp(&pdu, SEND_FILE, NULL);
  n_bytes = send(tcp_sock, &pdu, sizeof(pdu), 0);
  debug("Enviat paquet SEND_FILE");
  if(n_bytes<0){
    fprintf(stderr, "Error al send \n");
    perror("Error: ");
    exit(-1);
  }
  n_bytes = 0;
  start = clock();
  while(1) {
    n_bytes = recv(tcp_sock, &pdu_answer, sizeof(pdu_answer), 0);
    if(n_bytes<0){
      fprintf(stderr, "Error al recv \n");
      perror("Error: ");
      exit(-1);
    }
    if(n_bytes == 0){ /* No s'ha rebut resposta */
      if(clock()-start / CLOCKS_PER_SEC == w){
        debug("No hi ha hagut comunicació amb el servidor TCP");
        debug("Finalitzant la conexió TCP");
        correct = 0;
        break;
      }
    }else{ /*Hi ha resposta */
      sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(struct tcp_PDU), pdu_answer.type, pdu_answer.name, pdu_answer.mac, pdu_answer.random, pdu_answer.data);
      debug(buff);
      memset(buff, '\0', sizeof(buff)); /*Per evitar stack smashing */
      correct = 1;
      break;
    }
  }
  if(correct == 1) treat_tcp_packet(pdu_answer);
  if(correct == 0){
    close(tcp_sock);
    tcp_sock = 0;
  }
}

void treat_tcp_packet(struct tcp_PDU pdu_answer){
  switch(pdu_answer.type){
    case SEND_ACK:
      if(strcmp(pdu_answer.random, server_data.random) == 0 && strcmp(pdu_answer.name, server_data.name) == 0 && strcmp(pdu_answer.mac, server_data.MAC)==0){ /*Comprovem si es correcte */
        char name[10];
        strcat(name, params.config->name);
        strcat(name, ".cfg");
        if(strcmp(name, pdu_answer.data) == 0){
          send_file();
        }
        break;
      }
    case GET_ACK:
      if(strcmp(pdu_answer.random, server_data.random) == 0 && strcmp(pdu_answer.name, server_data.name) == 0 && strcmp(pdu_answer.mac, server_data.MAC)==0){ /*Comprovem si es correcte */
        get_file();
      }
      break;
    }
}

/*Rep l'arxiu de configuració */
void get_file(){
  struct tcp_PDU pdu_answer;
  int n_bytes = 0, w = 4;
  time_t start;
	char buff[151];
  FILE *f;

  start = clock();
  f = fopen(network_config_file, "w");
  if(f==NULL){
    fprintf(stderr, "Error obrir arxiu");
    exit(-1);
  }

  while(1) {
    n_bytes = recv(tcp_sock, &pdu_answer, sizeof(pdu_answer), 0);
    if(n_bytes<0){
      fprintf(stderr, "Error al recv \n");
      perror("Error: ");
      exit(-1);
    }
    if(n_bytes == 0){ /* No s'ha rebut resposta */
      if(clock()-start / CLOCKS_PER_SEC == w){
        debug("No hi ha hagut comunicació amb el servidor TCP");
        break;
      }
 	}else{ /*Hi ha resposta */

      sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(pdu_answer),
   				pdu_answer.type, pdu_answer.name, pdu_answer.mac, pdu_answer.random, pdu_answer.data);
   		 
      debug(buff);
      memset(buff, '\0', sizeof(buff)); /* Per evitar stack smashing */
      if(pdu_answer.type == GET_DATA){
        if(strcmp(pdu_answer.random, server_data.random) == 0 && strcmp(pdu_answer.name, server_data.name) == 0 && strcmp(pdu_answer.mac, server_data.MAC)==0){ /*Comprovem si es correcte */
          fputs(pdu_answer.data, f);
        }
      }
      if(pdu_answer.type == GET_END){
        fclose(f);
        debug("Rebut arxiu de configuració");
        break;
      }

    }
  }
}

/*Envia el GET_FILE */
void get_conf(){
  struct tcp_PDU pdu, pdu_answer;
  int n_bytes = 0, w = 4, correct;
  time_t start;
  char buff[200];

  create_tcp(&pdu, GET_FILE, NULL);
  n_bytes = send(tcp_sock, &pdu, sizeof(pdu), 0);
  debug("Enviat paquet GET_FILE");
  if(n_bytes<0){
    fprintf(stderr, "Error al send \n");
    perror("Error: ");
    exit(-1);
  }
  n_bytes = 0;
  start = clock();
  while(1) {
    n_bytes = recv(tcp_sock, &pdu_answer, sizeof(pdu_answer), 0);
    if(n_bytes<0){
      fprintf(stderr, "Error al recv \n");
      perror("Error: ");
      exit(-1);
    }
    if(n_bytes == 0){ /* No s'ha rebut resposta */
      if(clock()-start / CLOCKS_PER_SEC == w){
        debug("No hi ha hagut comunicació amb el servidor TCP");
        debug("Finalitzant la conexió TCP");
        correct = 0;
        break;
      }
    }else{ /*Hi ha resposta */
      sprintf(buff, "Rebut: bytes= %lu, type:%i, nom=%s, mac=%s, random=%s, dades=%s", sizeof(struct tcp_PDU), pdu_answer.type, pdu_answer.name, pdu_answer.mac, pdu_answer.random, pdu_answer.data);
      debug(buff);
      memset(buff, '\0', sizeof(buff)); /*Per evitar stack smashing */
      correct = 1;
      break;
    }
  }
  if(correct == 1) treat_tcp_packet(pdu_answer);
  if(correct == 0){
    debug("NO s'ha rebut resposta, tancant socket");
    close(tcp_sock);
    tcp_sock = 0;
  }
}


/*Envia l'arxiu de configuració */
void send_file(){
  FILE *conf;
  char buf[256];
  struct tcp_PDU pdu;
  int n_bytes = 0;

  conf = fopen(network_config_file, "r");
  if(conf == NULL){
    perror("Error obrir arxiu");
    exit(1);
  }

  while(fgets(buf, sizeof(buf), conf) != NULL){
    create_tcp(&pdu, SEND_DATA, buf);
    n_bytes = send(tcp_sock, &pdu, sizeof(pdu), 0);
    debug("Enviat paquet SEND_DATA");
    if(n_bytes<0){
      fprintf(stderr, "Error al send \n");
      perror("Error: ");
      exit(-1);
    }
    memset(buf, '\0', sizeof(buf)); /*Per evitar stack smashing */
  }
  create_tcp(&pdu, SEND_END, NULL);
  debug("Enviat paquet SEND_END");
  n_bytes = send(tcp_sock, &pdu, sizeof(pdu), 0);
  if(n_bytes<0){
    fprintf(stderr, "Error al send \n");
    perror("Error: ");
    exit(-1);
  }
  fclose(conf);
}

/*Inicialitza el socket TCP */
void setup_tcp(){
  struct sockaddr_in tcp_addr;
  if(tcp_sock == 0){
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_sock < 0){
      fprintf(stderr, "No s'ha pogut obrir el socket TCP. \n");
      perror("Error: ");
      exit(-1);
    }
    /*Inicialitzem adreça del client*/
    memset(&tcp_addr, 0, sizeof(struct sockaddr_in));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr.sin_port = htons(0);

    /*Inicialitzem adreça del servidor */
    memset(&params.tcp_addr_server, 0, sizeof(struct sockaddr_in));
    params.tcp_addr_server.sin_family = AF_INET;
    params.tcp_addr_server.sin_addr.s_addr = inet_addr(params.config->server);
    params.tcp_addr_server.sin_port = htons(params.config->TCPport);

    /* Binding */
    if(bind(tcp_sock, (struct sockaddr *) &tcp_addr, sizeof(tcp_addr)) < 0){
      fprintf(stderr, "No s'ha pogut obrir el socket TCP. \n");
      perror("Error: ");
      close(tcp_sock);
      exit(-1);
    }
    if(connect(tcp_sock, (struct sockaddr *) &params.tcp_addr_server, sizeof(params.tcp_addr_server)) < 0){
      fprintf(stderr, "No s'ha pogut connectar amb el servidor. \n");
      perror("Error: ");
      close(tcp_sock);
      exit(-1);
    }
    debug("Socket TCP inicialitzat");
  }
}

void read_commands(){
  char command[9]; /* les comandes son màxim 9 caracters */
  while(1){
    if(debug_flag == 0) printf("-> "); /* Per evitar barrejes amb els misatges debug */
    scanf("%9s", command);
    treat_command(command);
  }
}

void treat_command(char command[]){
  if(strcmp(command, "quit") == 0){
    pthread_cancel(alive_thread);
    close(udp_sock);
    close(tcp_sock);
    debug("Finalitzats sockets");
    exit(1);
  }else if(strcmp(command, "send-conf") == 0){
    setup_tcp();
    send_conf();
  }else if(strcmp(command, "get-conf") == 0){
    setup_tcp();
    get_conf();
  }else{
    print_msg("Wrong command");
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

void setup_udp(){
  udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(udp_sock < 0){
    fprintf(stderr, "No puc obrir socket \n");
    exit(-1);
  }
  debug("S'ha obert el socket");
}

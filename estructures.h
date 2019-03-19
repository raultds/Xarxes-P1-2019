/*Estructura per a guardar les dades del arxiu del client */
struct client_config{
  char name[7];
  char MAC[13];
  char server[20];
  char random[7];
  int UDPport;
  int TCPport;
};

/*Estructura per a guardar dades per a comprovar als ALIVE */
struct server_data{
  char name[7];
  char MAC[13];
  char random[7];
};

/* Estructura per a pasar les structs de config a treatPacket() */
struct parameters{
  struct client_config *config;
  struct sockaddr_in addr_cli;
  struct sockaddr_in udp_addr_server;
  struct sockaddr_in tcp_addr_server;
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

/*Estructura que fa de paquet TCP */
struct tcp_PDU{
  unsigned char type;
  char name[7];
  char mac[13];
  char random[7];
  char data [150];
};

/* Estructura que conté la info que es rep del subscribe per a la conexió TCP */
struct tcp_data{
  char random[8];
  int tcp_port;
};

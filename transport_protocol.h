/**
 * @file gobackn.c
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari, Valeria Villano
 * @date 2023-09-13 (nel formato YYYY-MM-DD)
 */


#ifndef TRANSPORT_PROTOCOL_H_
#define TRANSPORT_PROTOCOL_H_


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>


#define WINDOW_SIZE 4       // la dovrò fare variabile nel tempo
#define DATA_SIZE 1024
#define TIMEOUT_ACKS 3
#define TIMEOUT_RCV 5
#define MAX_TIMEOUT_FAIL 10
#define ACK 0b10101011
#define PROBABILITY 0.1




// Struttura per i pacchetti
typedef struct {
    u_int8_t sequence_number;       // inidici tra 0 e 255
    u_int8_t data[DATA_SIZE];       // contenuto del pacchetto
    size_t data_size;               // numero byte informativi di data[]
    u_int8_t transmission_code;     // pemette di capire se il pacchetto fa parte dalla trasmissione corrente
    u_int8_t last_pck_flag;         // indica al destinatario se è o no l'ultimo pacchetto 
    u_int8_t checksum;              // Campo per la checksum
} Packet;


// Struttura per gli ack
typedef struct {
    u_int8_t sequence_number;       // inidici tra 0 e 255
    u_int8_t ack_code;              // indica che si tratta di un ack
    u_int8_t transmission_code;     // pemette di capire se l'ack fa parte dalla trasmissione corrente   
    u_int8_t checksum;              // Campo per la checksum
} Ack;


// Struttura per i gestire i timer
typedef struct {
    int sequence_number;
    time_t start_time;
    int num_timeout_fail;
} Timer;


// Struttura per passagio dati ai threads
typedef struct {
    int socket;
    const void *buffer; 
    size_t msg_size;
    struct sockaddr *addr; 

    Packet *sender_buffer;
    int *sender_base;
    int *last_packet_acked;
    int last_packet;

    Timer *timers;
    bool *max_timeout_flag;

    pthread_mutex_t *mutex;

    u_int8_t trans_code;
} Thread_data;


u_int8_t calculate_checksum(Packet packet);
u_int8_t calculate_ack_checksum(Ack ack);
bool verify_checksum(Packet packet);
bool verify_ack_checksum(Ack ack);
void *receive_acks(void *arg);
void *timeout_acks(void *arg);
int get_last(bool received_packet[256]);
bool check_end_transmission(bool received_packet[256], int last_pck);
ssize_t assembly_msg(Packet receiver_buffer[256], int last_pck, void *buffer);
int send_msg(int socket, void *buffer, size_t msg_size, struct sockaddr *addr);
ssize_t rcv_msg(int socket, void *buf, struct sockaddr *addr);
void *timeout_rcv_msg(void *arg);


#endif  /* GOBACKN_H_ */
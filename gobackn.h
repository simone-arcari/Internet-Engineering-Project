/**
 * @file gobackn.c
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari, Valeria Villano
 * @date 2023-09-13 (nel formato YYYY-MM-DD)
 */


#ifndef GOBACKN_H_
#define GOBACKN_H_


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>


#define WINDOW_SIZE 4       // la dovrò fare variabile nel tempo
#define DATA_SIZE 8
#define TIMEOUT_SECONDS 100
#define ACK 0b10101011
#define IS_LAST_PCK 0b11111111
#define NOT_LAST_PCK 0b00001111
#define NOT_DEFINE 0b00000000;




// Struttura per i pacchetti
typedef struct {
    u_int8_t sequence_number;       // inidici tra 0 e 255
    u_int8_t data[DATA_SIZE];       // contenuto del pacchetto
    size_t data_size;               // numero byte informativi di data[] 
    u_int8_t last_pck;              // indica al destinatario se è o no l'ultimo pacchetto 
    u_int8_t checksum;              // Campo per la checksum
} Packet;


// Struttura per gli ack
typedef struct {
    u_int8_t sequence_number;       // inidici tra 0 e 255
    u_int8_t ack_code;              // indica che si tratta di un ack
    u_int8_t checksum;              // Campo per la checksum
} Ack;


// Struttura per i gestire i timer
typedef struct {
    int sequence_number;
    time_t start_time;
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
    int *last_packet;
    Timer *timers;
    pthread_mutex_t *mutex;
} Thread_data;


u_int8_t calculate_checksum(Packet packet);
u_int8_t calculate_ack_checksum(Ack ack);
bool verify_checksum(Packet packet);
bool verify_ack_checksum(Ack ack);
void *send_packets(void *arg);
void *receive_acks(void *arg);
int get_last(bool received_packet[256]);
bool check_end_transmission(bool received_packet[256], int last_pck);
void assembly_msg(Packet receiver_buffer[256], int last_pck, void *buffer);
void send_msg(int socket, void *buffer, size_t msg_size, struct sockaddr *addr);
void rcv_msg(int socket, void *buf, struct sockaddr *addr, socklen_t *addr_len_ptr);


#endif  /* GOBACKN_H_ */